# Default world data pack

This directory is the world that a new career starts from. Clubs are named
after real cities (including the Apulian towns of the Italian second
division, Acaya among them), but the pack contains **no real crests, kits,
stadiums or professional players**: kit colours, nicknames, founding years
and every player are made up, and stadiums use a generic municipal naming
pattern (which can coincide with a real municipal stadium's name). Keep it that way when you edit:
do not add real players, real club badges or copied kits.

Files are read at startup from the asset root (`AssetPaths` in
`src/global/paths.h.in`: installed data next to the executable, else the
source checkout).
Every `.json` file in `teams/` and `players/` is loaded, so a data pack can
be split over as many files as you like. All files are UTF-8 JSON.

## `leagues/leagues.json`

Array of leagues.

| Field           | Required | Meaning                                                         |
|-----------------|----------|-----------------------------------------------------------------|
| `id`            | yes      | League id (1-255). Also keys the economy/nationality profile in `src/model/world_tuning.h` (`LEAGUE_PROFILES`); unknown ids use a default profile. |
| `name`          | yes      | Display name. The domestic cup of a top division is named after it ("League" becomes "Cup"). |
| `parent_league` | no       | Id of the division above; links the tiers of one country for promotion and relegation. |
| `tiebreak`      | no       | `"head_to_head"` or omitted for goal difference.                |

## `teams/*.json`

Array of clubs. The default pack uses 20 clubs per league; keep leagues at
an even size and check `src/model/match_scheduler.*` before changing it.

| Field                 | Required | Meaning                                                   |
|-----------------------|----------|-----------------------------------------------------------|
| `id`                  | yes      | Unique team id (1-65535; 0 is reserved for free agents). Keep ids stable: saves refer to them. |
| `name`                | yes      | Display name.                                             |
| `league_id`           | yes      | League the club starts in.                                |
| `balance`             | no       | Ignored for new worlds (opening cash follows reputation). |
| `short_name`          | no       | Three-letter code, unique in the pack, e.g. `"LEC"`.      |
| `colours.primary`     | no       | Kit colour `"#RRGGBB"`.                                   |
| `colours.secondary`   | no       | Kit colour `"#RRGGBB"`, different from the primary.       |
| `stadium.name`        | no       | Stadium name.                                             |
| `stadium.capacity`    | no       | Nominal capacity (see below).                             |
| `founded`             | no       | Founding year.                                            |
| `nickname`            | no       | Club nickname.                                            |

The optional fields are read by `DataGenerator::loadClubIdentities()` and are
not stored in saves. Reputation, facilities, ticket prices and the stadium
capacity used by the simulation are generated from the world seed, so
`stadium.capacity` is descriptive only for now.

## `players/*.json`

Optional hand-made players. Every club is filled up to a full squad with
generated players, so this directory may be empty.

| Field            | Meaning                                                       |
|------------------|---------------------------------------------------------------|
| `id`             | Unique player id below 50000 (generated players start there). |
| `team_id`        | Club id.                                                      |
| `first_name`, `last_name` | Fictional names only.                                |
| `age`, `height`  | Years, centimetres.                                           |
| `nationality`    | Demonym such as `"Italian"` (see `stringToLanguage` in `src/global/languages.h`; unknown values fall back to English). |
| `role`           | Role name understood by `RoleUtils::fromString`.              |
| `stats`          | Map of the attribute names in `assets/config/stats_config.json` to 1-100 values. |
| `contract_years`, `wage` | Contract length and weekly wage.                      |
| `preferred_foot` | `"Left"` or `"Right"`.                                        |
| `status`         | Optional, defaults to 0.                                      |

## `names_files/`

`first_names.json` and `last_names.json` (`{"names": [...]}`) are the name
pools for generated players, staff and scouts.
