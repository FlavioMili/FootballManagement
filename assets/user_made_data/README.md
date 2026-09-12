# Default world data pack

This directory is the world that a new career starts from: eleven
countries, each with a top division and a second division linked for
promotion and relegation (three clubs up, three down every season). Clubs
are named after real cities and towns (including the Apulian towns of the
Italian second division, Acaya among them), but the pack contains **no real crests, kits,
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
| `id`            | yes      | League id (1-249; 250 and up are continental competitions). Also keys the economy/nationality/region profile in `src/model/world_tuning.h` (`LEAGUE_PROFILES`): add a row for every new league, otherwise it silently gets a default English, European profile. |
| `name`          | yes      | Display name. The domestic cup of a top division is named after it ("League" becomes "Cup"). |
| `parent_league` | no       | Id of the division above; links the tiers of one country for promotion and relegation. A league without a parent is a country's top division: it names the domestic cup (entered by every division of the country) and is the country for continental places and scouting. |
| `tiebreak`      | no       | `"head_to_head"` or omitted for goal difference.                |

## `teams/*.json`

Array of clubs. The default pack uses 20 clubs per league; keep leagues at
an even size and check `src/model/match_scheduler.*` before changing it.
Team ids follow the league: league `L` holds ids `L*100+1` to `L*100+20`
(for example 1401-1420 for the English second division). The second
divisions of every country except Italy are in `second_divisions.json`.

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
| `article_it`          | no       | Italian article (`"il"`, `"lo"`, `"l'"`, `"la"`) when the default (masculine, chosen from the first letters) is wrong, e.g. `"la"` for Roma. |

The optional fields are read by `DataGenerator::loadClubIdentities()` (and
`article_it` by `loadClubArticles()`) and are
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

`first_names.json` and `last_names.json` are the name pools for generated
players, staff and scouts.

| Field            | Required | Meaning                                                   |
|------------------|----------|-----------------------------------------------------------|
| `names`          | yes      | Array of names, used for nationalities without a pool of their own. Must not be empty. |
| `by_nationality` | no       | Object mapping a nationality demonym (as for players, e.g. `"Italian"`) to an array of names. Generated players and staff of that nationality draw from it; unknown demonyms and empty arrays are ignored. |

`first_names.json` may also carry a hashed exclusion list,
`excluded_name_hashes`: full names the generator must never produce, stored
only as hashes so the pack contains no real people's names. Each entry is the
16-digit lowercase hex form of `NamePool::nameHash` (salted 64-bit FNV-1a of
the name in lower case, Latin accents folded, single spaces; see
`src/model/world_generation.cpp`). A generated first and last name pair whose
hash is on the list is drawn again. Do not add plain names to the pack for
this purpose.
