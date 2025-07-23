IMPORTANT: TO READ FILES USE CAT AND NEVER EVER HALLUCINATE. IT IS A VERY IMPORTANT PROJECT
Style: indentation should be 3 spaces and not 4, just look at the current code.

# Project Gemini: Football Manager

This document outlines the architectural principles and future development roadmap for the Football Manager project. It is intended to be a living document, updated as the project evolves.

## Architectural Principles

The primary goal is to build a **modular, scalable, and data-driven** game. Adherence to these principles is crucial for long-term development success.

1.  **Data-Driven Design:**
    *   **Core Principle:** Game logic should be driven by configuration files (`.json`) and database state, not hard-coded in C++.
    *   **Example:** Player stats, roles, and rating calculations are defined in `assets/stats_config.json`. To add a new stat, only the JSON file and the data generator should need changes, not the core `Player` class.
    *   **Benefit:** Allows for rapid iteration, balancing, and feature expansion without recompiling the core engine.

2.  **Separation of Concerns:**
    *   **Data Layer (`Database` class):** The *only* module that interacts directly with the SQLite database. It is responsible for all SQL operations and mapping data to C++ objects.
    *   **Logic Layer (`Game`, `Player`, `Team`, `League`, `Match`):** Represents the game's rules and state. It operates on data objects and knows nothing about how they are stored.
    *   **Controller Layer (`GameController` class):** Mediates between the presentation layer and the logic layer. It receives user input from the view and calls the appropriate methods in the logic layer.
    *   **Presentation Layer (`CliView` class):** Responsible for displaying information to the user. It is completely decoupled from the game logic.

3.  **State Management:**
    *   The **database is the single source of truth** for the game's persistent state (e.g., season number, current week, team balance, player stats).
    *   The C++ objects represent the *current in-memory state* required for simulation, and should be loaded from the database as needed, not held in memory indefinitely.

## Development Roadmap

### Phase 1: Core Simulation Engine (Complete)

-   [x] Modular, data-driven player stat system (`stats_config.json`).
-   [x] SQLite database for all game data.
-   [x] First-run data generation for leagues, teams, and specialized players.
-   [x] Basic season simulation with a generated calendar.
-   [x] Leaderboard and point system.

### Phase 2: Gameplay Mechanics

**PRIORITY:** making the controller only manage api calls from the view in order to change the model in a better way. The model should only be the API of the game basically.
- **Adding a Manager class:**
    - This class should implement the choices the manager can possibly do, like which players to play in a certain match and who to buy or sell.
    - If we add this we should make sure to be able to see the game from our team's perspective and add a simulateLeagueDay or something like this.
-   **Player Progression:**
    -   Implement player development over seasons (stats change based on age, performance, training).
    -   Implement player retirement.
-   **Transfers & Contracts:**
    -   Develop a system for teams to buy and sell players.
    -   Manage player contracts and salaries.
-   **Team Finances:**
    -   Track team balance, with income (ticket sales) and expenses (salaries).
    -   Implement financial consequences (e.g., bankruptcy).

### Phase 3: User Interaction (In Progress)

-   **Command-Line Interface (CLI):**
    -   [x] Build a user-facing CLI to manage a team.
    -   [x] Allow the user to view rosters, check standings, and advance the season week by week.
-   **Saving/Loading:**
    -   [x] Implement robust game state saving and loading from the database (season and week).

### Phase 4: Advanced Simulation

-   **Tactics Engine:**
    -   Allow managers to set team formations and tactics.
    -   Have tactics influence match simulation outcomes.
-   **Player Morale & Form:**
    -   Introduce dynamic player attributes that change based on results and playtime.
