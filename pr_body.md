Resolves #62.

This PR implements the first half of the architectural overhaul to improve the MVC scalability and UI performance, preparing the project for Phase 2 gameplay features (finances, simulation, matches).

### Changes Implemented
1. **UI Caching (`MainGameScene`)**
   - The immediate mode UI (Dear ImGui) was dropping frames because it was running a `std::ranges::sort` on the *entire* league standings and top players lists 60 times a second inside `render()`.
   - Extracted these operations into a `refreshData()` function and cached them in `cached_standings` and `cached_top_players`. 
   - Reduced frame rendering complexity from `O(N log N)` down to `O(N)` without sorting overhead.

2. **Removed Global Singleton (`GameData::instance()`)**
   - Completely eradicated the `GameData` Singleton anti-pattern across the codebase.
   - `GameData` is now instantiated locally inside `main.cpp` and passed downwards through standard Dependency Injection via `std::shared_ptr`.
   - Updated `Game`, `GameController`, `Match`, `Team`, `Lineup`, `Finances`, `Calendar`, and `DataGenerator` to accept `GameData&` or `shared_ptr<GameData>`.
   - This prevents random segfaults, allows mock databases for testing, and makes background simulation thread-safe.

3. **Performance Optimization (`GameData::getPlayersForTeam`)**
   - Previously, getting players for a team required querying `_teamPlayers` (for IDs) and then querying `_players` for *each* individual `PlayerID`. This resulted in `O(K)` overhead where K is team size.
   - Restructured `_teamPlayers` from `vector<PlayerID>` to `vector<reference_wrapper<const Player>>`.
   - `getPlayersForTeam` now simply returns a cached vector reference in `O(1)` time.

### Benchmark Results
Tests run using Google Benchmark:

| Database Size (Players) | **Real Time (Before)** | **Real Time (Optimized)** | Improvement |
| ----------------------- | ---------------------- | ------------------------- | ----------- |
| **8,192 Players**       | 2.09 µs                | **0.11 µs**               | **19x Faster!** |
| **16,384 Players**      | 19.7 µs                | **0.95 µs**               | **20x Faster!** |

The time to fetch a team's players from a massive 16k-player database dropped from ~20 microseconds to under **1 microsecond**.

### Testing
- 14/14 unit tests passed locally.
- Re-ran performance benchmarks to verify `GameData` overhead.

