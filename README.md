# Project Gemini: Football Manager

This document outlines the architectural principles and future development roadmap for the Football Manager project.
It is intended to be a living document, updated as the project evolves.

## Architectural Principles

The primary goal is to build a **modular, scalable, and data-driven** game. 
Adherence to these principles is crucial for long-term development success.

1.  **Data-Driven Design:**
    *   **Core Principle:** Game logic should be driven by configuration files (`.json`) and database state, not hard-coded in C++.
        *   **Example:** Player stats, roles, and rating calculations are defined in `assets/stats_config.json`. 
    To add a new stat, only the JSON file and the data generator should need changes, not the core `Player` class.
    *   **Benefit:** Allows for rapid iteration, balancing, and feature expansion without recompiling the core engine.

2.  **Separation of Concerns:**
    *   **Data Layer (`Database` class):** The *only* module that interacts directly with the SQLite database. It is responsible for all SQL operations and mapping data to C++ objects.
    *   **Logic Layer (`Game`, `Player`, `Team`, `League`, `Match`)::** Represents the game's rules and state. It operates on data objects and knows nothing about how they are stored.
    *   **Controller Layer (`GameController` class):** Mediates between the presentation layer and the logic layer. It receives user input from the view and calls the appropriate methods in the logic layer.
    *   **Presentation Layer (`CliView` class):** Responsible for displaying information to the user. It is completely decoupled from the game logic.

3.  **State Management:**
    *   The **database is the single source of truth** for the game's persistent state (e.g., season number, current week, team balance, player stats).
    *   The C++ objects represent the *current in-memory state* required for simulation, and should be loaded from the database as needed, not held in memory indefinitely.

## Development Roadmap

### Phase 1: Core Simulation Engine (Complete)

-   [x] Modular, data-driven player stat system (`stats_config.json`).
-   [x] SQLite database for all game data.
-   [x] First-run data generation for leagues, teams (16 per league), and specialized players.
-   [x] Randomized managed team assignment on first run.
-   [x] Basic season simulation with league-specific, double round-robin calendar generation.
-   [x] Leaderboard and point system (league-specific).
-   [x] Basic CLI for interaction.
-   [x] Save/Load functionality.

### Phase 2: Gameplay Mechanics (In Progress)

-   **Player Progression:**
    -   [ ] Implement player aging at the end of each season.
    -   [ ] Implement player development (stats change based on age, performance, training).
    -   [ ] Implement player retirement.
-   **Transfers & Contracts:**
    -   [ ] Develop a system for teams to buy and sell players.
    -   [ ] Manage player contracts and salaries.
-   **Team Finances:**
    -   [ ] Track team balance, with income (ticket sales) and expenses (salaries).
    -   [ ] Implement financial consequences (e.g., bankruptcy).

### Phase 3: Advanced Simulation

-   **Tactics Engine:**
    -   [ ] Allow managers to set team formations and tactics.
    -   [ ] Have tactics influence match simulation outcomes.
-   **Player Morale & Form:**
    -   [ ] Introduce dynamic player attributes that change based on results and playtime.

### Phase 4: User Interface

-   **Command-Line Interface (CLI):**
    -   [ ] Improve the CLI to allow for more complex interactions.
    -   [ ] Add more detailed player and team information views.
-   **Graphical User Interface (GUI):**
    -   [ ] Plan and design a GUI.
    -   [ ] Implement the GUI using a suitable library.
