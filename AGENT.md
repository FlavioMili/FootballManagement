# Football Management - Agent Context

Welcome to the Football Management project! If you are an autonomous AI Agent (e.g., Antigravity), this document is your primary context and instruction manual.

## 1. Project Overview
A football management simulator built in C++ using SDL3, Dear ImGui, and SQLite3.

## 2. Current Architecture
- **UI Framework:** Dear ImGui hooked into SDL3 and SDL_Renderer3.
- **Database:** SQLite3 for persistent game state.
- **Localization:** JSON-based dictionaries loaded instantly into memory via `LanguageManager`.
- **Logic:** Data-driven game logic loaded from SQLite and `stats_config.json`. Separation of concerns is maintained between Database, Logic, Controller, and View.

## 3. Top Priority: UI Enhancements
Improving the UI is the absolute maximum priority.
Future agents must consult this roadmap when designing or expanding features:
1. **Dynamic Layouts:** Utilize ImGui's docking or columns to create information-dense, modern management dashboards.
2. **Theming:** Replace the default ImGui styling with a custom theme (colors, padding, borders, custom fonts).
3. **Interactive Elements:** Add hover states, tooltips, and clickable tables for deeper data exploration.
4. **Data Visualization:** Create graphs/charts for league standings, finances, and player attribute webs.

## 4. Coding Standards & Style
You MUST adhere to our linting and formatting rules:
1. **Formatting:** We use Allman bracing style (`.clang-format`). You should run formatting locally (or rely on the `make format` workflow).
2. **Linting:** We strictly enforce `.clang-tidy`. Ensure your changes do not introduce warnings.
3. **Scoped Variables:** Keep variables scoped tightly, inside `if` statements if they are not used elsewhere.
4. **Headers:** We use `#pragma once` as our include guard.

## 5. GitHub Traceability Protocol
Before starting a major implementation, the agent should:
1. Run `gh issue list` to see what is currently pending.
2. Run `gh issue create --title "..." --body "..."` to document the planned feature or bugfix if not already created.
3. Reference the issue number in commit messages or work summaries to ensure full traceability.
4. Keep Epics isolated in epic branches, and create child branches for specific issues.

## 6. AI Guidelines Compliance
As an AI Agent, you must comply with [AI_GUIDELINES.md](AI_GUIDELINES.md):
1. **Declare your usage:** Make sure any Pull Request clearly states that it was agent-generated and describes the steps you took.
2. **Request Review:** You MUST request human review on architectural decisions and complex logic. Do not bypass the human user.
3. **Document Your Work:** Utilize plan artifacts and walkthroughs to summarize your goals and accomplishments.
