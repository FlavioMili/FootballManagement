# Project Football Management

## Installation & Usage

### Prerequisites

Make sure your system has a **C++20 compiler** and the required libraries installed.

#### Ubuntu / Debian
```bash
sudo apt update
sudo apt install g++ make libsdl3-dev libsdl3-ttf-dev libsqlite3-dev libspdlog-dev libfmt-dev
```

#### Arch
```bash
sudo pacman -Syu
sudo pacman -S gcc make sdl3 sdl3_ttf sqlite spdlog fmt
```

#### MacOS 
```bash
brew update
brew install gcc make sdl3 sdl3_ttf sqlite spdlog fmt
```

### Installing and Compiling
```bash 
git clone https://github.com/FlavioMili/FootballManagement
cd FootballManagement
mkdir build && cd build

# Debug build (includes DEBUG macro and sanitizers)
cmake -DCMAKE_BUILD_TYPE=Debug ..

# Release build (optimized, no debug info)
cmake -DCMAKE_BUILD_TYPE=Release ..

# Release with debug info (optimized + debug symbols)
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..

make -j $nproc
```

### Have fun (and report issues :D)
```bash
./FootballManagement
```

---

## Architectural Principles

The primary goal is to build a **modular, scalable, and data-driven** football management simulation.  

### 0. Use Modern C++
- Embrace C++20 features for clear, self-explanatory, and memory-safe code (smart pointers, ranges, concepts).

### 1. Data-Driven Design
- **Core Principle:** Game logic and balance should be driven by configuration files (`.json`) and database state, not hard-coded.
  - *Example:* Player stats, roles, and rating calculations are defined in `assets/stats_config.json`.
- **Benefit:** Allows for rapid iteration, balancing, and feature expansion without recompiling the core engine.

### 2. Separation of Concerns & UI Evolution
- **Data Layer (`Database`):** The *only* module that interacts directly with SQLite.
- **Logic Layer (`Game`, `Match`, `Player`):** Operates entirely independent of how it is displayed.
- **Presentation Layer:** Currently using raw SDL3. **Immediate Goal:** Transition to a robust immediate-mode GUI framework (like Dear ImGui) or a web-based/HTML layout engine (like RmlUi) to scale menu complexity without writing manual coordinate-based UI code.

### 3. Match Simulation & State Management
- **State:** The database is the single source of truth for persistent game state. C++ objects represent the *in-memory state* for active simulation.
- **Match Engine:** Instead of instantly calculating results, the engine operates on a stateful **Minute-by-Minute (Playing Time)** simulation, allowing mid-match events, tactical changes, and substitutions.

---

## Development Roadmap & Long-Term Plan

### Phase 1: Core Engine & Foundation (Completed ✅)
- [x] SQLite database for game data persistence.
- [x] First-run data generation for leagues, teams, and players.
- [x] Basic season simulation with league-specific calendars.
- [x] Abstract instantaneous match simulation.
- [x] Basic CLI and raw SDL3 UI for interaction.
- [x] Save/Load functionality.

### Phase 2: Immediate Fixes & UI Overhaul (Current Focus 🚧)
- **UI & Menus Upgrade:**
  - [ ] Replace custom SDL widgets with a robust UI framework (e.g., Dear ImGui) for "Better Menus".
  - [ ] Create detailed dashboards for Team Roster, League Standings, and Player Profiles.
- **Stateful Match Engine ("Actual Playing Time"):**
  - [ ] Refactor `Match::simulate` into an event-driven loop (90 minutes + stoppage time).
  - [ ] Generate mid-game events (Shots, Saves, Fouls, Cards, Injuries).
  - [ ] Implement a live "Match Day View" allowing the player to watch events unfold and make live substitutions.
- **Architecture:**
  - [ ] Reduce dependency on the global `GameData` singleton to improve testability.

### Phase 3: Advanced Gameplay Mechanics
- **Player Progression & Dynamics:**
  - [x] Player aging and static development.
  - [ ] Dynamic morale and form based on playtime, match results, and training.
  - [ ] Regen logic: create new youth players when older players retire.
- **Transfers & Contracts:**
  - [ ] Implement player contracts, wage expectations, and release clauses.
  - [ ] AI-driven transfer market and inter-club bidding logic.
- **Team Finances:**
  - [ ] Track team balance (ticket sales, TV rights, salary expenses).
  - [ ] Board expectations and job security.

### Phase 4: Strategy & Deep Simulation
- **Tactical Depth:**
  - [ ] Allow precise player positioning, team mentality (Attacking/Defensive), and passing style.
  - [ ] Link tactical setup to the minute-by-minute Match Engine probabilities.
- **Calendar & World:**
  - [ ] Move from an abstract "Matchday" system to a true 365-day calendar.
  - [ ] Add domestic cups and continental competitions.

### Phase 5: Polish & Visuals
- **Immersive Match View:**
  - [ ] Develop a 2D pitch view (similar to classic Football Manager) representing player movements.
- **Rich Media:**
  - [ ] Generated avatars for players.
  - [ ] Club crests, detailed UI themes, and sound design.
