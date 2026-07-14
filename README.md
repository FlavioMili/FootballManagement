# Project Football Management

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![C++](https://img.shields.io/badge/C++-23-blue.svg)
![Build](https://img.shields.io/badge/build-passing-brightgreen.svg)

Welcome to **Football Management**, a fast-paced, data-driven football management simulator built from the ground up in C++23 using SDL3, Dear ImGui, and SQLite3.

Whether you're looking to manage your favorite club to glory, simulate years of football history, or contribute to an open-source C++ game engine, you're in the right place!

---

## 🤝 Community & Contributing

We welcome contributions from humans and AI alike! Before you start, please check out our community guidelines:

* **[Contributing Guide](CONTRIBUTING.md)**: Start here if you want to set up the project locally, learn our branching strategy, and open a Pull Request.
* **[AI Guidelines](AI_GUIDELINES.md)**: We are proudly **pro-AI**. However, all AI-assisted code must be explicitly declared and heavily human-reviewed. Read this file to understand our philosophy.
* **[Code of Conduct](CODE_OF_CONDUCT.md)**: We are committed to fostering a welcoming and inclusive environment.
* **[Agent Context](AGENT.md)**: If you are an autonomous AI Agent, read this file for specific architectural constraints and tooling requirements.

---

## Installation & Usage

### Prerequisites

Make sure your system has a **C++23 compiler** and the required libraries installed.

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

make -j $(nproc)
```

### Have fun (and report issues :D)
```bash
./FootballManagement
```

---

## Architectural Principles

The primary goal is to build a **modular, scalable, and data-driven** game.
Adherence to these principles is crucial for long-term development success.

### 0. Use Modern C++
- We rely on C++23 features. Keep it modern, safe, and self-explanatory.

### 1. Data-Driven Design
- **Core Principle:** Game logic should be driven by configuration files (`.json`) and database state, not hard-coded in C++.
  - *Example:* Player stats, roles, and rating calculations are defined in `assets/stats_config.json`.
    To edit a stat, only the JSON file and the data generator should need changes, not the core `Player` class.
- **Benefit:** Allows for rapid iteration, balancing, and feature expansion without recompiling the core engine. (Also cheating and modding :D )

### 2. Separation of Concerns
- **Data Layer (`Database` class):** The *only* module that interacts directly with the SQLite database.
- **Logic Layer (`Game`, `Player`, `Team`, `League`, `Match`):** Represents the game's rules and state, operates on data objects, and is decoupled from storage.
- **Controller Layer (`GameController` class):** Receives user input and manipulates the logic layer accordingly.
- **Presentation Layer (`GuiView` class):** Displays information to the user using Dear ImGui. It is completely decoupled from the game logic.

### 3. State Management
- The **database is the single source of truth** for persistent game state (e.g., season, week, team balance).
- C++ objects represent the *in-memory state* required for simulation, loaded on demand.

---

## Development Roadmap

### Phase 1: Core Simulation Engine ✅
- [x] Modular, data-driven player stat system
- [x] SQLite database for all game data
- [x] First-run data generation for leagues and teams
- [x] Basic season simulation with league-specific calendar
- [x] Leaderboard and point system
- [x] Basic CLI for interaction - out of development but might be retaken into consideration
- [x] Save/Load functionality

### Phase 2: Gameplay Mechanics 🚧
- **Player Progression:**
  - [x] Player aging at season end
  - [x] Player development
  - [x] Player retirement
  - [ ] Regen logic to maintain player pool
- **Transfers & Contracts:**
  - [ ] Transfer market system
  - [ ] Player contracts and salaries
  - [ ] Player selection before starting the game
- **Team Finances:**
  - [ ] Track team balance
  - [ ] Bankruptcy consequences

### Phase 3: Advanced Simulation
- **Calendar Overhaul:**
  - [ ] Replace "matchday" structure with 365-day time model
- **Tactics & Strategy:**
  - [ ] Link team tactics to match AI behavior
- **Player Morale, Form and training:**
  - [ ] Dynamic player morale based on playtime and results
  - [ ] Add training sessions

### Phase 4: User Interface 🚧
- **Graphical User Interface (GUI):**
  - [x] Port to Dear ImGui
  - [x] Implement robust Language System (JSON)
  - [ ] Improve the GUI layout (Docking, Data Visualizations)
  - [ ] Expand database to support advanced GUI elements (avatars, crests)
