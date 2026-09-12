# Save migrations and save protocol

## Schema versions

- Every save records the migrations applied to it in `schema_migrations`
  (number, name, game version, checksum) and its schema version in the
  single-row `save_meta` table (contracts/save-format.md).
- `Migrations::currentSchemaVersion()` is the highest number in
  `registry.cpp`. A save with a higher version is refused before anything is
  written (`FutureVersionError`, lang key `SAVE_ERROR_FUTURE_VERSION`).
- Saves from before versioning have no `save_meta` and count as version 0.
  All migrations check for existence explicitly, so every intermediate layout
  (tables and columns added over time) upgrades from version 0.

`Migrations::migrate()` (called by `GameData` for new and loaded worlds):

1. reads the version (read-only) and rejects future versions;
2. runs `assets/db/schema.sql` (`CREATE TABLE IF NOT EXISTS`, outside a
   transaction because it sets the journal mode): tables that did not exist
   when the save was written are created with their full definition;
3. applies every pending migration in number order, each in one
   `BEGIN IMMEDIATE ... COMMIT` together with its `schema_migrations` row and
   `save_meta.schema_version`; a failure rolls back and leaves the save at the
   previous version;
4. re-adds missing columns of already recorded migrations (hand-edited or
   partially copied saves) and logs the repair.

## Adding a migration

- New table: add `CREATE TABLE IF NOT EXISTS` to `assets/db/schema.sql`. No
  migration is needed unless existing rows must be back-filled.
- New column on an existing table, or a data change: add a `ColumnSpec` array
  and/or an idempotent `apply` function to `registry.cpp`, append one line to
  `REGISTRY` with the next number (`NNNN_snake_name`), and update the table in
  `schema.sql` so new saves get the column directly.
- Never renumber, edit or remove a released migration; fix mistakes with a new
  one. Back-fills that need randomness use the `Migration` RNG stream keyed by
  entity id, never the clock.
- Test it: upgrade a save built in-test without the column (see
  `test/test_save_migrations.cpp`), check values, run the upgrade twice.

## Save protocol

A career is played on an in-memory working database. Nothing reaches the slot
file (`saves/save_<slot>.db`) until the player saves or an autosave runs, so a
crash mid-session loses only the progress since the last save and can never
leave a half-written or logically inconsistent slot.

Commit points (`GameController::saveGame()` / autosave):

1. `Game::saveGame()` flushes the in-memory aggregates (game state, calendar,
   competitions, world, transfers, players, teams) to the working database in
   one transaction; dirty flags are cleared only after the commit. Fixtures
   are diffed against the stored rows (only new, changed and dropped ones are
   written); every player changes daily, so player rows are rewritten with
   reused statements and allocation-free encoders.
2. `SaveManager::stampMetadata()` updates `save_meta` (game, engine, RNG and
   simulation versions of the build, world seed, last saved in-game date,
   playtime, UTC timestamp).
3. `SaveManager::persist()`:
   1. online backup of the working database to `<slot>.tmp`, switched to a
      rollback journal so the file is self-contained (no `-wal` sidecar);
   2. `PRAGMA quick_check` on the temporary file, then `fsync`;
   3. rotation: `.bak.N-1` -> `.bak.N` ... and the current slot file is
      hard-linked (copied if links are unsupported) to `.bak.1`;
   4. atomic `rename(<slot>.tmp, <slot>)`, then `fsync` of the directory.

A failure at any step (disk full, read-only folder, I/O error, crash) leaves
the previous slot file intact; a crash can only leave a stale `.tmp`, which
the next save removes. A slot file is never copied by hand while a `-wal`
exists: legacy files that still have one are checkpointed first.

Loading: `SaveManager::inspect()` opens the file read-only and runs
`PRAGMA integrity_check` (refuses damaged, unfinished or newer saves without
touching them; `foreign_key_check` findings are logged, not fatal, until the
free-agent sentinel is repaired), keeps a `<slot>.pre-v<N>.bak` copy once when
the save needs an upgrade, then copies it into the working database where
the migrations run. Restoring a backup never deletes the replaced file: it is
kept as `<slot>.corrupt-<utc>` or `<slot>.replaced-<utc>`.

Autosave (`AutosavePolicy`, default weekly with 3 backups) runs at the end of
each simulated day, on the Continue worker thread, never on the UI thread.

## Compatibility policy

- Every public release loads saves from all previous public releases.
- Saves from older releases stay loadable for at least two releases after the
  one that introduced a replacement format; removing a migration path is
  announced one release earlier and requires a standalone converter.
- Upgrading the rules edition, RNG version or engine version of an existing
  career requires the player's confirmation; otherwise the career keeps the
  versions recorded in `save_meta`.
