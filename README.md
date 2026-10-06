# Football Management

![C++](https://img.shields.io/badge/C++-23-blue.svg)
[![CI](https://github.com/FlavioMili/FootballManagement/actions/workflows/ci.yml/badge.svg)](https://github.com/FlavioMili/FootballManagement/actions/workflows/ci.yml)

Football Management is an open-source football management game for Linux,
macOS and Windows, written in C++23 with SDL3, Dear ImGui and SQLite. You
take over a club in a world of 22 leagues and 440 clubs: pick the squad and
the tactics, keep the board, the dressing room and the fans on your side,
trade in the transfer market, bring players through the academy and build a
career that can take you from club to club and to a national team. Every
match is played by a real-time simulation that you can watch in a 2D
tactical view or a 3D broadcast view, or play yourself by taking control of
your team on the pitch; the rest of the world plays on the same engine.

> **Status:** version 1.0 is being prepared for release. The
> [changelog](CHANGELOG.md) lists what it contains and its known
> limitations.
>
> *Football Management* is a working title and may change before or after
> the release.

![Home dashboard](docs/images/home.png)

## Contents

- [Features](#features)
- [Screenshots](#screenshots)
- [Building](#building)
- [Running](#running)
- [Controls](#controls)
- [Where your files are](#where-your-files-are)
- [Tests and tools](#tests-and-tools)
- [Game data and modding](#game-data-and-modding)
- [Roadmap](#roadmap)
- [Contributing](#contributing)
- [License](#license)

## Features

**Management**
- One management shell with seven sidebar hubs, F1-F7 shortcuts and a
  command palette (Ctrl+K) that finds players, clubs, screens and pending
  tasks; browser-style Back and Forward (touchpad swipe, mouse side
  buttons, Alt+Left/Right).
- A Home dashboard with the next fixture and a *Next steps* card, and an
  inbox that separates decisions from information, lets you act inline and
  filters by player or club; decision moments such as a disputed fine or a
  fans' protest about ticket prices.
- A world news hub, cup and continental draw ceremonies, and a career
  timeline you can export as a journal.
- Continue advances the calendar on a background worker and can be stopped
  at the end of a day; holiday mode hands the club to your assistant until
  a date, a match or a decision.
- Delegation of lineup fixes, substitutions, training, renewals, scouting,
  friendlies and youth contracts to the assistant manager.
- A welcome tour, a first-week checklist, and a Help screen (F8) with the
  shortcuts and a football glossary.
- English and Italian with locale-aware money, six themes, colour-vision
  modes, text size and interface scaling for HiDPI displays, layouts that
  adapt from 1280x720 to 4K, and keyboard shortcuts you can rebind.

**Match day**
- A real-time engine with player movement and fatigue, ball flight with
  drag, bounce and roll, goalkeeping, dribbling, tackles, marking and
  pressing.
- The laws of the game: offside, fouls and advantage, yellow and red cards,
  free kicks, penalties, corners, throw-ins, goal kicks and added time.
- 2D and 3D views of the same match, speeds from 1x to 30x, a Highlights
  mode and a Quick result button; six 3D cameras including a TV director,
  with mouse orbit, pan and zoom, animated players, and a day-lit stadium
  for afternoon kick-offs.
- Play mode: take control of your team on the pitch with the keyboard or a
  gamepad, one footballer at a time, under the same rules, attributes and
  fatigue as a watched match.
- Player roles and duties for every position, and a separate shape with
  the ball that you drag into place on the tactics screen.
- A substitutions board, an in-match tactics panel and eleven touchline
  shouts; pre-match and half-time team talks that the players carry onto
  the pitch.
- Opposition reports with individual instructions the engine plays out:
  tight marking, closing down, showing a player onto his weaker foot or
  doubling up on him.
- Cup ties played to a winner through extra time and a live penalty
  shootout; match reports with expected goals, shot maps, touch maps and
  pass networks, and half-time and full-time analysis.
- Procedural match sound: crowd, whistles and ball contacts.

**Competitions**
- 22 leagues in eleven countries, each with a top and a second division,
  promotion and relegation, and a domestic cup.
- Continental club competitions with Swiss-style league phases, two-legged
  knockout rounds, prize money and coefficients.
- National teams with qualifiers, finals tournaments, friendlies and
  call-ups of your players.
- League tables with clinch marks, archived seasons, a data hub with
  league leaders, and an end-of-season review with the board's verdict.

**The club and the world**
- Finances with TV, merit, gate and commercial income, wage and staff
  budgets, a ledger, ticket pricing, parachute payments and transfer
  budgets tied to real cash.
- Board objectives for the league, the cup, the finances and young
  players; confidence, warnings, dismissal, transfer embargoes and facility
  projects. Supporter mood that the board listens to.
- Injuries and a medical centre with load charts, rest and minute limits;
  fatigue, sharpness, morale and form; player development driven by age,
  training, minutes and staff.
- Squad status, captains, set-piece takers and squad numbers, a squad
  planner and player comparison; player conversations, promises and
  dressing-room stories.
- Coaching, medical, scouting and youth staff; weekly training plans and
  mentoring groups.

**Transfers, scouting and youth**
- Transfer search with affordability filters and need-based
  recommendations, and transfer windows that follow each country's
  calendar.
- Negotiations with counter-offers, instalments, add-ons and sell-on
  clauses, loans in both directions, pre-contracts, free agents and release
  clauses; agents who talk back; bids for your own players that you can
  negotiate too.
- Scouts with nationalities, languages and regional experience. Players
  outside your club are shown as estimates whose ranges narrow as your
  knowledge grows.
- A yearly youth intake with a February preview and a March intake, an
  under-18 league, scholarships and first professional contracts; an
  under-21 squad for every club with its own league.

**Career**
- A manager profile with reputation and coaching licence, a job centre,
  interviews, offers from other clubs, resignation and sacking. You can
  start unemployed.
- National-team jobs, alone or next to your club job: pick the squad for
  each international window and qualify for the finals.
- Monthly and season awards, player honours, and club and league records.
- Three save slots, autosave and restorable backups. The game plays on an
  in-memory copy and only replaces a save with a verified snapshot; a save
  that does not load can be restored from its newest good backup, and a
  crash leaves a report on your computer (nothing is sent anywhere).

The [changelog](CHANGELOG.md) has the complete list.

## Screenshots

| | |
|---|---|
| ![The squad screen](docs/images/squad.png) | ![Tactics](docs/images/tactics.png) |
| **Squad**: the first team with a player's details beside the list. | **Tactics**: styles, instructions, roles and the shape with the ball. |
| ![A transfer offer](docs/images/transfer-offer.png) | ![League table](docs/images/competitions.png) |
| **Transfers**: an offer for another club's player. | **Competitions**: league tables, cups and continental football. |
| ![The 3D match view](docs/images/match-3d.png) | ![The 2D match view](docs/images/match-2d.png) |
| **Match day in 3D**: the broadcast camera. | **Match day in 2D**: the tactical view of the same match. |
| ![Scouting](docs/images/scouting.png) | ![Youth academy](docs/images/youth.png) |
| **Scouting**: scouts, assignments and reports. | **Youth academy**: facilities, the under-18 league and the next intake. |

## Building

The game builds with CMake 3.29 or newer and a C++23 compiler. The first
configure needs Git and network access: SDL3, SDL3_ttf, Dear ImGui, fmt,
spdlog and nlohmann/json (and GoogleTest when tests are enabled) are fetched
at pinned versions. SQLite comes from the system if CMake finds it and is
fetched otherwise.

CI builds Linux with GCC 14, macOS with Homebrew LLVM and Windows with
Visual Studio 2022 and clang-cl. Windows support is experimental: it builds
in CI, but it has had far less testing than the other two.

### Linux

SDL is built from source, so you need its build dependencies. On Ubuntu
24.04 or Debian:

```sh
sudo apt install git g++-14 ninja-build pkg-config \
  libfreetype-dev libharfbuzz-dev libsqlite3-dev \
  libasound2-dev libpulse-dev libpipewire-0.3-dev \
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev \
  libxi-dev libxss-dev libxtst-dev libxkbcommon-dev \
  libdrm-dev libgbm-dev libgl1-mesa-dev libegl1-mesa-dev libgles2-mesa-dev \
  libwayland-dev libdecor-0-dev libdbus-1-dev libudev-dev
```

Ubuntu 24.04 ships CMake 3.28, which is too old: install a newer one, for
example with `pip install cmake` or from [cmake.org](https://cmake.org).
It also defaults to GCC 13, so select GCC 14 before the first configure.
Any newer GCC works too. The complete package list CI uses is in
[`.github/workflows/build.yml`](.github/workflows/build.yml).

```sh
git clone https://github.com/FlavioMili/FootballManagement
cd FootballManagement
CC=gcc-14 CXX=g++-14 cmake --preset release
cmake --build --preset release --parallel 4
./out/build/release/src/FootballManagement
```

If CMake cannot find FreeType or HarfBuzz, add `-DSDLTTF_VENDORED=ON` to the
configure command to build them as well.

### macOS

macOS 15 on Apple silicon is the supported setup. AppleClang's standard
library is missing parts of C++23 that the game uses, so build with
Homebrew's LLVM and link its libc++ statically, as CI does:

```sh
brew install cmake ninja llvm
LLVM="$(brew --prefix llvm)"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
  -DCMAKE_C_COMPILER="$LLVM/bin/clang" \
  -DCMAKE_CXX_COMPILER="$LLVM/bin/clang++" \
  "-DCMAKE_EXE_LINKER_FLAGS=-nostdlib++ $LLVM/lib/c++/libc++.a $LLVM/lib/c++/libc++abi.a"
cmake --build build --parallel 4
open build/src/FootballManagement.app
```

### Windows (experimental)

Install Visual Studio 2022 with the *Desktop development with C++* workload
and its *C++ Clang tools for Windows* component (clang-cl), CMake 3.29 or
newer and Git.
From a Developer PowerShell:

```powershell
git clone https://github.com/FlavioMili/FootballManagement
cd FootballManagement
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -T ClangCL `
  -DBUILD_TESTING=OFF -DSDLTTF_VENDORED=ON
cmake --build build --config Release --target FootballManagement
.\build\src\Release\FootballManagement.exe
```

The test suites use POSIX APIs, so tests are not built on Windows.

### Other build options

The presets in [`CMakePresets.json`](CMakePresets.json) are `release`,
`release-tests`, `debug-sanitized` (AddressSanitizer and UBSan), `profile`
and `clang-tidy`; their build trees go to `out/build/<preset>`. A plain
build directory works as well (`cmake -S . -B build -G Ninja`). See
[docs/development/builds.md](docs/development/builds.md) for the details and
[docs/development/release.md](docs/development/release.md) for packaging.

## Running

Start the executable from the build tree (see the commands above). A
development build reads the game data from the source checkout, so it runs
from any working directory; `FM_ASSET_ROOT` points it at another data
directory. Installed packages use the data next to the executable.

To install a Linux build for your user (including all game assets):

```sh
cmake --preset release
cmake --build --preset release --parallel 2
cmake --install out/build/release --component game --prefix "$HOME/.local"
"$HOME/.local/bin/FootballManagement"
```

For a Makefiles build, `make -C build install` also installs the game and
assets, using the prefix chosen with `-DCMAKE_INSTALL_PREFIX=...` when
configuring. Saves and settings go to your user data directory, so the
installation directory does not need to be writable during play.

`FM_WORLD_SEED` fixes the seed of a new world, which helps when you report a
bug.

## Controls

The game is played with the mouse; the keyboard speeds up what you do all the
time. The essentials:

| Where | Keys |
|-------|------|
| Anywhere | **F12** screenshot |
| Management screens | **Ctrl+K** command palette, **F1-F7** sidebar hubs, **F8** help, **Space/Enter** Continue, **Ctrl+S** save, **Alt+Left/Right** back and forward, **Esc** close the screen |
| Match | **Space** pause, **. ,** faster and slower, **V** 2D/3D view, **1-6** 3D cameras, **S** substitutions, **T** tactics, **Shift+1...0, Shift+-** touchline shouts, **F** pitch focus, **M** mute |
| Play mode | **WASD** or arrows move, **Shift** sprint, **J** pass, **K** shoot, **L** through ball, **I** lofted pass, **E** jockey, **Q** switch player, **Esc/P** pause menu; or a gamepad |

These are the defaults: every shortcut except F12 and sprint can be rebound
in *Settings > Controls*.

Every shortcut and mouse gesture is listed in
[docs/user/controls.md](docs/user/controls.md). New to the game? Start with
the [getting-started guide](docs/user/getting-started.md).

## Where your files are

Saves, settings, logs and screenshots go to the user data directory, never
next to the executable:

| System | Folder |
|--------|--------|
| Linux | `$XDG_DATA_HOME/FlavioMili/FootballManagement` (usually `~/.local/share/FlavioMili/FootballManagement`) |
| macOS | `~/Library/Application Support/FlavioMili/FootballManagement` |
| Windows | `%APPDATA%\FlavioMili\FootballManagement` |

Saves are SQLite databases in `saves/` (one per slot, with numbered
backups beside it that the main menu can restore). The log and any crash
reports are in `logs/`; attach both when you report a crash.

## Tests and tools

Configure with tests (`release-tests` preset or `-DBUILD_TESTING=ON`):

```sh
cmake --preset release-tests
cmake --build --preset release-tests --parallel 4

# The fast suites, as in CI
ctest --preset release-tests -LE "playtest|monkey|slow"

# Everything, including the long playtest, GUI monkey and adversarial suites
ctest --preset release-tests
```

Tests run headless (SDL's dummy video driver) with fixed seeds and a scratch
data directory under `/tmp/football-management-tests`, so they never touch
your saves. Labels: `unit`, `core` (no GUI), `gui`, `lab`, `playtest`,
`monkey`, `adversarial` and `slow`.

`fm_lab`, built next to the game, is a headless balance lab. It plays large
batches of matches or whole seasons and compares the results with real-world
ranges:

```sh
fm_lab matches --n 1000 --spread --threads 4   # league-like rating gaps
fm_lab season --seasons 1 --threads 2          # a season of the whole world
fm_lab tactics                                 # tactic presets round robin
```

`fm_lab --help` lists every option.

The screenshots in `docs/images` come from a real career, captured by the
`showcase_captures` test (skipped unless `FM_SHOWCASE_DIR` is set):

```sh
FM_SHOWCASE_DIR=/tmp/showcase ctest --preset release-tests -R showcase
```

It writes PNG files at interface scale 2; the header of
[test/test_showcase_captures.cpp](test/test_showcase_captures.cpp) explains
the options.

## Game data and modding

The world a new career starts from is plain JSON under
[`assets/user_made_data/`](assets/user_made_data/README.md): leagues, clubs
(name, league, kit colours, stadium, founding year, nickname), optional
hand-made players and the name pools for generated players and staff. Every
file in `teams/` and `players/` is loaded, so a data pack can be split
across as many files as you like. Attribute definitions and role weights are
in `assets/config/stats_config.json`, translations in `assets/lang/`.

Clubs carry the names of real cities and towns, but there are no real
players, crests or kits: colours, nicknames, founding years and every player
are made up, and competitions have invented names. Please keep it that way
in contributions. The game is not affiliated with any league, club or other
football management game.

## Roadmap

The next steps come from the known limitations of version 1.0 (see the
[changelog](CHANGELOG.md)):

- Play mode: an accelerated match clock, a practice match and set pieces
  taken by the player.
- More realism: shot conversion, the possession gap between strong and
  weak sides, and offsides closer to real football.
- Southern-hemisphere and American season calendars for Brazil, Argentina
  and the United States.
- Watching and directing your national team's matches live, players with
  more than one nationality, and appearance clauses in loans for other
  clubs' players.
- Whole worlds that stay bit-identical across platforms.
- Match sound with club chants and a commentary voice; Italian commentary
  with the clubs' articles.
- Signed and notarised macOS packages, and a validated Windows build.

## Contributing

Bug reports, ideas and pull requests are welcome. Read the
[contributing guide](CONTRIBUTING.md) and the
[code of conduct](CODE_OF_CONDUCT.md) first; the architecture is described
in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## License

License: to be announced. The licence of the game has not been chosen yet;
until it is, please ask before reusing the code or the data. The bundled
third-party components keep their own licences, which ship with the release
packages (see [release.md](docs/development/release.md)).
