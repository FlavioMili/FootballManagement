# Football Management

![License](https://img.shields.io/badge/license-GPL--3.0-blue.svg)
![C++](https://img.shields.io/badge/C++-23-blue.svg)
[![CI](https://github.com/FlavioMili/FootballManagement/actions/workflows/ci.yml/badge.svg)](https://github.com/FlavioMili/FootballManagement/actions/workflows/ci.yml)

Football Management is an open-source football management game written in
C++23 with SDL3, Dear ImGui and SQLite. You run a club in a living world of
22 leagues: pick the squad and the tactics, deal with the board, the
dressing room and the transfer market, bring players through the academy and
build a manager career across clubs.

Matches are played by a real-time simulation. One second of match time is one
second of simulation at normal speed, and you can watch the same match in a
2D tactical view or a 3D broadcast view, switching between them whenever you
like. Every other match in the world runs on the same engine.

> **Status:** v0.1, the first public release, is being prepared. See the
> [changelog](CHANGELOG.md) for what is in it and its known limitations.

## Features

**Management**
- One management shell with seven sidebar hubs, tabs for their screens,
  F-key shortcuts and a command palette (Ctrl+K) that finds players, clubs,
  screens and pending tasks.
- Home dashboard with the next fixture and a *Next steps* card; an inbox that
  separates decisions from information and lets you act inline.
- Continue runs the calendar on a background worker with a progress overlay;
  holiday mode hands the club to your assistant manager until a date, a
  match or a decision.
- Delegation of lineup fixes, substitutions, training, renewals, scouting,
  friendlies and youth contracts to your assistant manager.
- English and Italian, six themes, interface scaling for HiDPI displays and
  layouts that adapt from 1280x720 to 4K.

**Match engine and the laws of the game**
- Real-time engine with player kinematics and fatigue, ball physics (drag,
  bounce, roll), goalkeeping, dribbling, tackles, marking and pressing.
- Offside, fouls and advantage, yellow and red cards with referee strictness,
  free kicks, penalties, corners, throw-ins, goal kicks and added time from
  the stoppages of each half.
- Five substitutions in three windows, pre-match and half-time team talks,
  and a lineup check that replaces injured or suspended players.
- 2D and 3D views of the same match, speeds from 1x to 30x, a highlights
  mode, a quick result, five 3D cameras with mouse orbit, pan and zoom, and
  a pitch focus mode.
- Match reports, half-time and full-time analysis, and opposition reports.
- Procedural match sound (crowd, whistles, ball) with master, crowd and
  effects volumes.

**Competitions**
- 22 leagues with 440 clubs: a top division and a second division in each
  of eleven countries, with promotion and relegation, and domestic cups.
- Cup ties and continental deciders are played to a winner, through extra
  time and a penalty shootout, live or in the background.
- Continental club competitions with Swiss-style league phases, two-legged
  knockout rounds, prize money and coefficients.
- National teams with qualifiers, finals tournaments, friendlies,
  international windows and call-ups of your players.

**World simulation**
- Finances: TV, merit, gate and commercial income, wage and staff budgets,
  a full ledger, ticket prices, and transfer budgets tied to real cash.
- Board objectives and confidence, warnings, dismissal and transfer
  embargoes; facility projects funded by the board.
- Injuries and a medical screen, fatigue, sharpness, morale and form.
- Player development driven by age, training, minutes and staff.
- Squad status, captains and set-piece takers, a squad planner and player
  comparison.
- Player conversations, promises, dressing-room mood and story chains.

**Youth academy**
- A yearly intake with a February preview and a March intake, scouted
  ranges, scholarships and first professional contracts.
- An under-18 league whose minutes feed development; homegrown status.

**Transfers and negotiation**
- Search with affordability filters and need-based recommendations.
- Negotiations with counter-offers, instalments, loans, pre-contracts, free
  agents, listings and releases; clubs that value key players and long
  contracts, and agents who open high.
- Computer-controlled clubs that buy, sell, loan and trim their squads.

**Scouting**
- Scouts with nationalities, languages and regional experience, sent to a
  continent, country or league.
- Players outside your club are shown as estimates whose ranges narrow as
  knowledge grows.

**Staff and training**
- Coaching, medical, scouting and youth staff with contracts.
- Weekly training plans with presets, per-session intensity and automatic
  lightening in congested weeks; mentoring groups for young players.

**Manager career**
- A manager profile with reputation and coaching licence, a job centre with
  applications, interviews and contract talks, offers from other clubs,
  resignation and sacking. You can start unemployed.

**Awards and records**
- Monthly and season awards, player honours, and club and league records.

**Saves**
- Three save slots, autosave, and numbered backups restorable from the main
  menu. The game works on an in-memory copy and replaces the save file only
  with a verified snapshot. Older saves are migrated when loaded.

## Screenshots

There are no screenshots in the repository yet. To add some, capture them in
the running game with F12 (the image is written to `captures/screenshot.bmp`
in the user data directory, see [Where your files are](#where-your-files-are),
or to the path in `FM_SCREENSHOT_PATH`). Convert them to PNG, put them in
`docs/screenshots/` and link them here. Useful shots: the Home dashboard, the
squad, the transfer negotiation dialog, and a match in both the 2D and the 3D
view (`FM_MATCH_VIEW=3d` or `2d` forces the initial match view).

## Documentation

- [Getting started](docs/user/getting-started.md): a walkthrough of your
  first season.
- [Controls](docs/user/controls.md): every keyboard and mouse shortcut.
- [Changelog](CHANGELOG.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Reproducible builds](docs/development/builds.md): presets, sanitizers,
  clang-tidy and dependency updates.
- [Building and releasing packages](docs/development/release.md)
- [Data pack format](assets/user_made_data/README.md)

## Building

### Requirements

- CMake 3.29 or newer and Ninja (the presets use Ninja).
- A C++23 compiler. CI uses GCC 14 on Ubuntu 24.04 and Homebrew LLVM on
  macOS; newer GCC releases work too. AppleClang's libc++ is not enough (see
  [release.md](docs/development/release.md)).
- Git and network access for the first configure: SDL3, SDL3_ttf, Dear
  ImGui, fmt, spdlog, nlohmann/json (and GoogleTest when tests are enabled)
  are fetched at pinned versions. SQLite is taken from the system if found,
  otherwise fetched.

SDL is built from source, so on Linux you need its build dependencies. On
Ubuntu or Debian:

```sh
sudo apt install g++-14 ninja-build pkg-config \
  libasound2-dev libpulse-dev libx11-dev libxext-dev libxrandr-dev \
  libxcursor-dev libxfixes-dev libxi-dev libxss-dev libxtst-dev \
  libxkbcommon-dev libdrm-dev libgbm-dev libgl1-mesa-dev libegl1-mesa-dev \
  libgles2-mesa-dev libwayland-dev libdecor-0-dev libdbus-1-dev \
  libudev-dev libpipewire-0.3-dev
```

Ubuntu 24.04 defaults to GCC 13, so select GCC 14 with `CC=gcc-14 CXX=g++-14`
before the first configure. On Ubuntu 24.04 the packaged CMake is 3.28, which
is too old: install a newer one (for example with `pip install cmake` or from
cmake.org). The full package list used by CI is in
`.github/workflows/build.yml`. If CMake cannot find FreeType or HarfBuzz for
SDL3_ttf, either install their development packages or configure with
`-DSDLTTF_VENDORED=ON` to build them too.

### Configure and build

```sh
git clone https://github.com/FlavioMili/FootballManagement
cd FootballManagement
cmake --preset release
cmake --build --preset release --parallel 2
```

The build tree is `out/build/release`. Other presets are `release-tests`,
`debug-sanitized`, `profile` and `clang-tidy`; see
[builds.md](docs/development/builds.md). A plain build directory works too:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
```

## Running

```sh
./out/build/release/src/FootballManagement
```

(or `./build/src/FootballManagement` for a plain build directory). On macOS
every build produces `src/FootballManagement.app`.

A development build reads the game data from the source checkout, so you can
run it from any directory. `FM_ASSET_ROOT` points it at another data
directory.

### Where your files are

Saves, settings, logs and screenshots are written to the user data
directory, never next to the executable:

- Linux: `$XDG_DATA_HOME/FlavioMili/FootballManagement` (usually
  `~/.local/share/FlavioMili/FootballManagement`)
- macOS: `~/Library/Application Support/FlavioMili/FootballManagement`
- Windows: `%APPDATA%\FlavioMili\FootballManagement`

`FM_WORLD_SEED` fixes the seed of a new world, which is handy for bug reports.

## Tests

Configure with tests enabled (the `release-tests` preset or
`-DBUILD_TESTING=ON`), then:

```sh
cmake --preset release-tests
cmake --build --preset release-tests --parallel 2

# Fast suites, as in CI
ctest --preset release-tests -LE "playtest|monkey|slow"

# Everything, including the long playtest, GUI monkey and adversarial suites
ctest --preset release-tests
```

Tests run headless (SDL's dummy video driver), with fixed world and match
seeds and a scratch data directory under `/tmp/football-management-tests`,
so they never touch your saves. The labels are `unit`, `core` (no GUI),
`gui`, `lab`, `playtest`, `monkey`, `adversarial` and `slow`; select them
with `-L` or exclude them with `-LE`. Run the full suite before a release.

## Balance lab (`fm_lab`)

`fm_lab` is a headless tool that plays large batches of matches or whole
seasons and compares the results with real-world ranges. It is built with the
game (`out/build/release/src/fm_lab`).

```sh
# 2000 matches between equal 65-rated sides
fm_lab matches

# League-like rating gaps, 1000 matches, 4 threads
fm_lab matches --n 1000 --spread --threads 4

# A fixed pairing, e.g. 70 v 60
fm_lab matches --home-rating 70 --away-rating 60

# One season of the whole world with only computer-managed clubs
fm_lab season --seasons 1 --threads 2

# Round robin of the tactic presets
fm_lab tactics
```

`--fidelity background` uses the cheaper step of unwatched fixtures, `--seed`
makes a run repeatable, and `--out-dir` chooses where `report.md` and
`metrics.json` go (by default a fresh `/tmp/fm-lab-*` directory). The lab
lowers its own priority and uses a scratch data directory that is removed at
exit. It exits with 0 when every gate target passes and 1 when one fails.
Run `fm_lab --help` for all options.

## Data and modding

The world a new career starts from is plain JSON under
`assets/user_made_data/`: leagues, clubs (name, league, short name, kit
colours, stadium, founding year, nickname), optional hand-made players and
the name pools for generated players and staff. Every file in `teams/` and
`players/` is loaded, so you can split a data pack across as many files as
you like. The format is documented in
[assets/user_made_data/README.md](assets/user_made_data/README.md).
Attribute definitions and role weights are in
`assets/config/stats_config.json`, translations in `assets/lang/`.

Clubs carry the names of real cities (plus a few Apulian towns in the
Italian second division), but there are no real players, crests or kits:
colours, nicknames, founding years and every player are made up. Please keep
it that way in contributions.

## Contributing

Contributions are welcome. Read the [contributing guide](CONTRIBUTING.md) and
the [code of conduct](CODE_OF_CONDUCT.md) before opening a pull request.

## Licence

Football Management is free software released under the
[GNU General Public License v3.0](LICENSE). The licences of the bundled
third-party components are shipped with the release packages (see
[release.md](docs/development/release.md)).
