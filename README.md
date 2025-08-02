# Project Football Management

This document outlines the architectural principles and future development roadmap for the Football Manager project.  
It is intended to be a living document, updated as the project evolves.

---

## Architectural Principles

The primary goal is to build a **modular, scalable, and data-driven** game.  
Adherence to these principles is crucial for long-term development success.

### 1. Data-Driven Design
- **Core Principle:** Game logic should be driven by configuration files (`.json`) and database state, not hard-coded in C++.
  - *Example:* Player stats, roles, and rating calculations are defined in `assets/stats_config.json`.  
    To add a new stat, only the JSON file and the data generator should need changes, not the core `Player` class.
- **Benefit:** Allows for rapid iteration, balancing, and feature expansion without recompiling the core engine.

### 2. Separation of Concerns
- **Data Layer (`Database` class):** The *only* module that interacts directly with the SQLite database.
- **Logic Layer (`Game`, `Player`, `Team`, `League`, `Match`):** Represents the game's rules and state, operates on data objects, and is decoupled from storage.
- **Controller Layer (`GameController` class):** Receives user input and manipulates the logic layer accordingly.
- **Presentation Layer (`CliView` class):** Displays information to the user. It is completely decoupled from the game logic.

### 3. State Management
- The **database is the single source of truth** for persistent game state (e.g., season, week, team balance).
- C++ objects represent the *in-memory state* required for simulation, loaded on demand.

---

## Development Roadmap

### Phase 1: Core Simulation Engine ✅
- [x] Modular, data-driven player stat system (`stats_config.json`)
- [x] SQLite database for all game data
- [x] First-run data generation for leagues, teams (16 per league), and specialized players
- [x] Randomized managed team assignment on first run
- [x] Basic season simulation with league-specific, double round-robin calendar
- [x] Leaderboard and point system (league-specific)
- [x] Basic CLI for interaction
- [x] Save/Load functionality

### Phase 2: Gameplay Mechanics 🚧
- **Player Progression:**
  - [x] Player aging at season end
  - [x] Player development based on age, performance
  - [x] Player retirement
  - [ ] Regen logic: create new players when others retire to maintain pool size
- **Transfers & Contracts:**
  - [ ] Transfer market system
  - [ ] Player contracts and salaries
  - [ ] Add player selection before starting the game
- **Team Finances:**
  - [ ] Track team balance (income from tickets, expenses for salaries)
  - [ ] Bankruptcy consequences

### Phase 3: Advanced Simulation
- **Calendar Overhaul:**
  - [ ] Replace "matchday" structure with 365-day time model for more flexible market simulation
- **Tactics & Strategy:**
  - [ ] Strategy/tactics system configurable per team
  - [ ] Link team tactics to match AI behavior
- **Player Morale, Form and training:**
  - [ ] Dynamic player morale based on playtime, performance, results
  - [ ] Add training sessions for players
 
### Phase 4: User Interface
- **Command-Line Interface (CLI):** (Possibly skippable but currently use for debugging)
  - [ ] Improve interaction model
  - [ ] Add detailed views for teams, players, stats
- **Graphical User Interface (GUI):**
  - [ ] Expand database to support advanced GUI elements (e.g. avatars, club crests)
  - [ ] Plan and design a GUI layout
  - [ ] Implement GUI with a suitable toolkit

---
