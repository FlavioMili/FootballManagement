# Transfer Market Feature — Complete Implementation Guide

> **Project:** Football Management (C++23, SDL3, Dear ImGui, SQLite3)
> **Purpose:** A step-by-step guide for any AI agent or developer to implement a full transfer market system with AI negotiation, squad needs evaluation, attention scores, and transfer windows.
> **Peer Review Note:** This document is intended to be reviewed by another AI agent (or human) before and during implementation. Each section includes rationale, dependencies, and verification steps. Reviewers should check for architectural consistency, edge cases, and alignment with the existing codebase patterns.

---

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [Pre-Flight Checklist](#2-pre-flight-checklist)
3. [Step 1: Plumb db_conn into GameController (Option A)](#3-step-1-plumb-db_conn-into-gamecontroller)
4. [Step 2: Bitmask Transfer Status Encoding](#4-step-2-bitmask-transfer-status-encoding)
5. [Step 3: New DB Table + Queries](#5-step-3-new-db-table--queries)
6. [Step 4: TransferListing Model](#6-step-4-transferlisting-model)
7. [Step 5: GameData Persistence for Listings](#7-step-5-gamedata-persistence-for-listings)
8. [Step 6: GameController — Core Methods](#8-step-6-gamecontroller--core-methods)
9. [Step 7: AI Evaluation System](#9-step-7-ai-evaluation-system)
10. [Step 8: Transfer Window Integration](#10-step-8-transfer-window-integration)
11. [Step 9: TransferMarketScene GUI](#11-step-9-transfermarketscene-gui)
12. [Step 10: Localization](#12-step-10-localization)
13. [Step 11: Build System](#13-step-11-build-system)
14. [Step 12: Documentation & GitHub Issue](#14-step-12-documentation--github-issue)
15. [Appendix: Full File Change Summary](#appendix-full-file-change-summary)

---

## 1. Architecture Overview

### Data Flow

```
┌─────────────────────────────────────────────────────────────────────┐
│                        GUI Layer                                     │
│  TransferMarketScene (ImGui, 3 tabs: Buy / Listings / Bids)         │
└──────────────────────────┬──────────────────────────────────────────┘
                           │ calls methods on
                           ▼
┌─────────────────────────────────────────────────────────────────────┐
│                      Controller Layer                                │
│  GameController                                                      │
│    ├─ holds: game (unique_ptr), gamedata (shared_ptr), db_conn (shared_ptr),
│    │          transfer_listings (unordered_map<PlayerID, TransferListing>)
│    ├─ methods: list, buy, sign, bid, negotiate, AI processing       │
│    ├─ AI: evaluateSquadNeeds, calculateAttentionScore, generateBid  │
│    └─ calls: Game::advanceDay, GameData::transferPlayer, repos     │
└──────────┬──────────────────────────────────────────────────────────┘
           │ delegates to
           ▼
┌─────────────────────────────────────────────────────────────────────┐
│                        Model + Data Layers                           │
│  ┌──────────┐  ┌──────────┐  ┌──────────────┐  ┌────────────────┐  │
│  │  Player   │  │   Team   │  │   Finances   │  │ TransferListing│  │
│  │(status    │  │(playerIDs│  │(balance,     │  │(struct)        │  │
│  │ bitmask)  │  │ add/rm)  │  │ wage)        │  │                │  │
│  └─────┬─────┘  └────┬─────┘  └──────┬───────┘  └────────────────┘  │
│        │              │               │                              │
│        ▼              ▼               ▼                              │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │  GameData (in-memory cache) + Repositories (DB persistence)  │   │
│  │  transferPlayer(), saveToDB(), PlayerRepository, etc.        │   │
│  └──────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
```

### Transfer Execution Flow (Step by Step)

```
User clicks [Buy] for player with price €5.0M
  ↓
GameController::buyPlayer(pid, buyer_team_id, 5_000_000)
  ↓
1. Validate: buyer exists, player listed, price matches, budget sufficient
2. Get seller_id from player.getTeamId()
3. buyer.finances.subtractBalance(5_000_000)
4. seller.finances.addBalance(5_000_000)
5. seller.removePlayerID(pid)
6. buyer.addPlayerID(pid)
7. gamedata->transferPlayer(pid, buyer_id)      // in-memory swap
8. player.setTransferStatus(TransferStatus::NotListed)
9. removePlayerFromTransfer(pid)                  // clear listing
10. db_conn->beginTransaction()
11. PlayerRepository::transferPlayer(pid, buyer_id)  // UPDATE team_id
12. PlayerRepository::updatePlayer(player)            // UPDATE status + other fields
13. db_conn->commitTransaction()
14. saveGame()
```

### AI Activity Flow

```
GameController::advanceDay()
  ↓
if isTransferWindowOpen() AND random(0,1) < 0.15:
  processAITransferActivity()
    ↓
  for each AI team:
    evaluateSquadNeeds(team_id)
    for each MISSING role:
      findTargets(role, team_id, budget)
      pickBestCandidate()
      generateBid()
      submitBid()
    for each SURPLUS role:
      pickSurplusPlayer()
      listPlayerForTransfer()
```

---

## 2. Pre-Flight Checklist

Before implementing, read these existing files to understand the codebase:

| File | Purpose |
|------|---------|
| `src/model/player.h` | Player class with TransferStatus, getMarketValue |
| `src/model/player.cpp` | market value algorithm, setTeamId |
| `src/model/team.h` | addPlayerID, removePlayerID, getFinances |
| `src/model/team.cpp` | Implementation |
| `src/model/finances.h` | addBalance, subtractBalance, getBalance |
| `src/model/finances.cpp` | Implementation |
| `src/model/game.h` | advanceDay, getCurrentDate, saveGame |
| `src/model/game.cpp` | Season loop, match simulation |
| `src/model/gamedate.h` | isTransferWindowOpen, date arithmetic |
| `src/model/gamedate.cpp` | Already implements isTransferWindowOpen() — Summer (Jun-Aug) + Winter (Jan) |
| `src/controller/game_controller.h` | Full interface — this is where we add most methods |
| `src/controller/game_controller.cpp` | Current implementation, plumbing |
| `src/database/gamedata.h` | transferPlayer(), getPlayer(), getTeam(), getPlayersForTeam() |
| `src/database/gamedata.cpp` | In-memory transfer implementation |
| `src/database/repositories/player_repository.h` | transferPlayer, updatePlayer, deletePlayer |
| `src/database/repositories/player_repository.cpp` | SQL bindings |
| `src/database/database_connection.h` | prepareStatement, beginTransaction, etc. |
| `src/gui/gui_view.h` | Scene stack management |
| `src/gui/gui_scene.h` | SceneID enum |
| `src/gui/scenes/main_game_scene.cpp` | Sidebar button for Transfer Market |
| `assets/db/schema.sql` | Current DB schema |
| `assets/db/queries.sql` | Current SQL queries |
| `src/global/queries.h` | Query enum + string_to_query_map |
| `src/global/global.h` | FREE_AGENTS_TEAM_ID = 0, MIN/MAX_STAT_VAL |
| `src/global/types.h` | PlayerID, TeamID, PlayerRole types |
| `src/CMakeLists.txt` | Build file to add new sources |

### Peer Review Prompt

When reviewing this plan, an AI agent should:
1. Verify that every method called in the plan actually exists in the existing codebase
2. Check that constructors and signatures match (e.g., Game's constructor accepting `shared_ptr<DatabaseConnection>` vs `string`)
3. Confirm that `GameData` already has `transferPlayer()` and understand its side effects
4. Understand that `Finances` stores balance as `int64_t` but player wages/prices are `uint32_t`
5. Check that the `status` bitmask approach doesn't conflict with any existing usage of the `status` field
6. Verify that `PlayerRepository::updatePlayer` binds parameters correctly for the 13 SQL columns
7. Confirm `SceneID` enum values don't overlap or break existing switch statements

---

## 3. Step 1: Plumb db_conn into GameController

### Rationale

GameController needs direct database access for transfer operations (updating player team_id, finances, transfer listings). Currently only `Game` owns the `db_conn`. We refactor `Game` to accept a pre-created connection, and `GameController` creates and holds its own reference.

**Design decision:** Option A was chosen (see discussion in the design decision section below) because:
- It follows the Single Responsibility Principle — the controller orchestrates, the game simulates
- It aligns with Issue #64 (Decompose GameController into domain-specific controllers)
- It's the same pattern already used internally (repos take `shared_ptr<DatabaseConnection>`)

### Dependencies

None. This is a pure refactoring step.

### Changes

#### `src/model/game.h`

```cpp
// BEFORE
class Game {
public:
    explicit Game(std::shared_ptr<GameData> gd, const std::string& db_path);
    // ...
};

// AFTER
class Game {
public:
    explicit Game(std::shared_ptr<GameData> gd, std::shared_ptr<DatabaseConnection> conn);
    // ...
};
```

#### `src/model/game.cpp`

```cpp
// BEFORE
Game::Game(std::shared_ptr<GameData> gd, const std::string& db_path)
    : db_conn(std::make_shared<DatabaseConnection>(db_path)),
      gamedata(std::move(gd)),
      currentDate(START_DATE)
{
    (*gamedata).loadFromDB(db_conn);
    loadGame();
}

// AFTER
Game::Game(std::shared_ptr<GameData> gd, std::shared_ptr<DatabaseConnection> conn)
    : db_conn(std::move(conn)),
      gamedata(std::move(gd)),
      currentDate(START_DATE)
{
    (*gamedata).loadFromDB(db_conn);
    loadGame();
}
```

#### `src/controller/game_controller.h`

Add member variable:

```cpp
class GameController {
public:
    // ...existing...
    
    /** @brief Gets the database connection (for repos that need it). */
    std::shared_ptr<DatabaseConnection> getDbConn() const { return db_conn; }

    /** @brief Gets the game data cache. */
    std::shared_ptr<GameData> getGameData() const { return gamedata; }

private:
    std::shared_ptr<class DatabaseConnection> db_conn;  // NEW
};
```

#### `src/controller/game_controller.cpp`

```cpp
// BEFORE
void GameController::newGame(int slot) {
    std::string path = getSavePath(slot);
    if (std::filesystem::exists(path)) std::filesystem::remove(path);
    gamedata = std::make_shared<GameData>();
    game = std::make_unique<Game>(gamedata, path);
}

bool GameController::loadGame(int slot) {
    std::string path = getSavePath(slot);
    if (!std::filesystem::exists(path)) return false;
    gamedata = std::make_shared<GameData>();
    game = std::make_unique<Game>(gamedata, path);
    return true;
}

// AFTER
void GameController::newGame(int slot) {
    std::string path = getSavePath(slot);
    if (std::filesystem::exists(path)) std::filesystem::remove(path);
    gamedata = std::make_shared<GameData>();
    db_conn = std::make_shared<DatabaseConnection>(path);    // NEW
    game = std::make_unique<Game>(gamedata, db_conn);        // CHANGED
}

bool GameController::loadGame(int slot) {
    std::string path = getSavePath(slot);
    if (!std::filesystem::exists(path)) return false;
    gamedata = std::make_shared<GameData>();
    db_conn = std::make_shared<DatabaseConnection>(path);    // NEW
    game = std::make_unique<Game>(gamedata, db_conn);        // CHANGED
    return true;
}
```

### Verification

```bash
cd build && cmake .. && make -j$(nproc)
```

The project should compile and run identically. Test that new game, save, and load still work.

### Peer Review Checklist

- [ ] `Game` constructor no longer creates its own `db_conn` — verify no other code creates `Game` with a path string
- [ ] The `Game::saveGame()` method still uses the internal `db_conn` correctly
- [ ] `GameData::loadFromDB()` is still called before `loadGame()` in the new constructor
- [ ] `GameController::db_conn` is initialized in both `newGame()` and `loadGame()`
- [ ] `GameController` destructor doesn't need to explicitly clean up `db_conn` (shared_ptr handles it)

---

## 4. Step 2: Bitmask Transfer Status Encoding

### Rationale

The `Player::_status` field is a `uint32_t` bitmask already persisted to the `status` column in SQLite. We use bit 0 to encode whether the player is listed for transfer. The `Player::_transfer_status` field (enum class `TransferStatus`) remains as the in-memory representation. We sync them: setting `_transfer_status` also updates the bit in `_status`, and the constructor reads the bit to initialize `_transfer_status`.

**Why bitmask?** The alternative was a new DB column. A bitmask avoids schema changes, is already supported by the existing `status` column type and queries, and follows a well-known pattern for storing multiple boolean flags in a single integer field. Bit 0 is reserved for transfer-listed; bits 1-31 remain available for future use (injured, suspended, etc.).

### Dependencies

None. Only modifies Player class internals.

### Changes

#### `src/model/player.h`

```cpp
class Player {
public:
    // ...existing...

    /** @brief Bitmask bit for transfer-listed status. */
    static constexpr uint32_t TRANSFER_LISTED_BIT = 0x01U;

    // ...setTransferStatus already declared, no signature change needed...
};
```

#### `src/model/player.cpp`

**Constructor** — decode `_transfer_status` from `_status`:

```cpp
Player::Player(/* ... */ uint32_t new_status, /* ... */)
    : // ...existing init list...
      _status(new_status),
      // ...
{
    // NEW: decode transfer status from status bitmask
    _transfer_status = (_status & TRANSFER_LISTED_BIT)
        ? TransferStatus::Listed
        : TransferStatus::NotListed;
}
```

**`setTransferStatus()`** — update `_status` bitmask atomically:

```cpp
void Player::setTransferStatus(TransferStatus status)
{
    _transfer_status = status;
    if (status == TransferStatus::Listed) {
        _status |= TRANSFER_LISTED_BIT;
    } else {
        _status &= ~TRANSFER_LISTED_BIT;
    }
}
```

### Verification

1. Create a player with `status = 1` → `_transfer_status` should be `Listed`
2. Create a player with `status = 0` → `_transfer_status` should be `NotListed`
3. Call `setTransferStatus(Listed)` → `getStatus()` should return value with bit 0 set
4. Call `setTransferStatus(NotListed)` → bit 0 should be cleared

Existing tests in `test_player.cpp` should still pass (they don't test status encoding, but the constructor signature hasn't changed).

### Peer Review Checklist

- [ ] The `_transfer_status` field is initialized from `_status` in the constructor, NOT from the parameter — verify the order of initialization
- [ ] `TRANSFER_LISTED_BIT = 0x01` doesn't conflict with any existing bit usage in `_status`
- [ ] All existing code paths that set `_status` (if any) still work — check for direct `_status` assignment
- [ ] `setTransferStatus` changes both `_transfer_status` and `_status` — no inconsistency possible
- [ ] `PlayerRepository` doesn't need changes because it uses `getStatus()` which returns `_status`, and passes the loaded `status` column to the constructor

---

## 5. Step 3: New DB Table + Queries

### Rationale

Transfer listings (player_id, asking_price, listing_date) need to persist across save/load cycles. We add a new `TransferList` table and three new queries: upsert, delete, and load all.

**Why a new table instead of storing in GameState JSON?** A dedicated table allows direct SQL queries (e.g., `SELECT * FROM TransferList WHERE asking_price < 5000000`) without parsing JSON. It also follows the existing schema pattern where each entity has its own table.

### Dependencies

None. Adding to schema and queries files.

### Changes

#### `assets/db/schema.sql`

Add before the final `PRAGMA journal_mode=WAL;` line:

```sql
-- Transfer list for the transfer market
CREATE TABLE IF NOT EXISTS TransferList (
    player_id INTEGER PRIMARY KEY,
    asking_price INTEGER NOT NULL DEFAULT 0,
    listing_date TEXT NOT NULL,
    FOREIGN KEY(player_id) REFERENCES Players(id)
);
```

#### `assets/db/queries.sql`

Add at the end:

```sql
-- ==========================================
-- TRANSFER LIST
-- ==========================================

-- @QUERY_ID: UPSERT_TRANSFER_LISTING
INSERT OR REPLACE INTO TransferList (player_id, asking_price, listing_date)
VALUES (?, ?, ?);

-- @QUERY_ID: DELETE_TRANSFER_LISTING
DELETE FROM TransferList WHERE player_id = ?;

-- @QUERY_ID: LOAD_ALL_TRANSFER_LISTINGS
SELECT player_id, asking_price, listing_date FROM TransferList;
```

#### `src/global/queries.h`

Add enum values before `COUNT`:

```cpp
enum class Query
{
    // ...existing...
    UPSERT_TRANSFER_LISTING,
    DELETE_TRANSFER_LISTING,
    LOAD_ALL_TRANSFER_LISTINGS,
    COUNT
};
```

Add map entries:

```cpp
const std::unordered_map<std::string, Query> string_to_query_map = {
    // ...existing...
    {"UPSERT_TRANSFER_LISTING", Query::UPSERT_TRANSFER_LISTING},
    {"DELETE_TRANSFER_LISTING", Query::DELETE_TRANSFER_LISTING},
    {"LOAD_ALL_TRANSFER_LISTINGS", Query::LOAD_ALL_TRANSFER_LISTINGS},
};
```

### Verification

After this change, the game should still start and load without errors (the new table is created by schema.sql on first run, and the queries are loaded from queries.sql by SQLLoader).

### Peer Review Checklist

- [ ] `UPSERT_TRANSFER_LISTING` uses `INSERT OR REPLACE` — this will replace an existing listing for the same player_id. This is correct because a player can only have one listing.
- [ ] The `listing_date` format matches `GameDateValue::toString()` format: `YYYY-MM-DD` (10 chars)
- [ ] Query enum values are added BEFORE `COUNT` — COUNT must always be the last value
- [ ] String identifiers in `string_to_query_map` match the `@QUERY_ID:` annotations exactly (case-sensitive)
- [ ] No foreign key conflicts: `player_id` references `Players(id)`, and `Players(id)` is auto-increment with existing IDs
- [ ] Schema and query files should not have trailing whitespace or extra blank lines at the end

---

## 6. Step 4: TransferListing Model

### Rationale

A lightweight struct to hold the state of each transfer listing in memory. Stored in a `std::unordered_map<PlayerID, TransferListing>` owned by `GameController`.

**Why a struct and not a full class?** It's a pure data holder with no behavior (no logic, no validation, no invariants). It's constructed by the controller and passed around. Following the existing codebase style where simple data aggregates use structs (e.g., `Matchup`, `Vector2F`, `RoleFocus`).

### Dependencies

None. Independent struct.

### New Files

#### `src/model/transfer_listing.h`

```cpp
// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <optional>

#include "global/types.h"
#include "model/gamedate.h"

/**
 * @struct TransferListing
 * @brief Represents a player listed on the transfer market.
 *
 * Tracks the asking price, listing date, and the current highest bid (if any).
 * Owned by GameController as part of the transfer_listings map.
 */
struct TransferListing
{
    PlayerID player_id;               // The player being sold
    TeamID seller_team_id;            // The team selling the player
    uint32_t asking_price;            // The price the seller wants (in currency units)
    GameDateValue listing_date;       // When the player was first listed

    // Bid state — set when another team makes a bid
    std::optional<PlayerID> highest_bidder_id;  // Team that made the highest bid (std::nullopt if none)
    uint32_t highest_bid = 0;                   // Amount of the highest bid (0 if none)

    // Cached for performance
    float attention_score = 0.0f;               // Calculated when listed, dictates AI interest

    /**
     * @brief Constructs a new TransferListing with bid state cleared.
     * @param pid Player ID.
     * @param seller ID of the selling team.
     * @param price Asking price in currency units.
     * @param date Date the listing was created.
     * @param attention The pre-calculated attention score.
     */
    TransferListing(PlayerID pid, TeamID seller, uint32_t price, GameDateValue date, float attention = 0.0f);

    /** @brief Default constructor for map/serialization use. */
    TransferListing() = default;
};
```

#### `src/model/transfer_listing.cpp`

```cpp
// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "transfer_listing.h"

TransferListing::TransferListing(PlayerID pid, TeamID seller, uint32_t price, GameDateValue date, float attention)
    : player_id(pid),
      seller_team_id(seller),
      asking_price(price),
      listing_date(date),
      highest_bidder_id(std::nullopt),
      highest_bid(0),
      attention_score(attention)
{
}
```

### Peer Review Checklist

- [ ] `highest_bidder_id` uses `std::optional<PlayerID>` — this is C++17+. The project uses C++23, so it's available.
- [ ] Default constructor is `= default` and leaves all fields uninitialized (POD types). The `highest_bid` will be 0 from the in-class initializer, but `player_id`, `seller_team_id`, `asking_price` will be uninitialized. This is acceptable because the default constructor is only used as a placeholder before being assigned to.
- [ ] The `listing_date` uses `GameDateValue` which has a default constructor (year=2025, month=7, day=1). This is fine as a sentinel value for uninitialized listings.
- [ ] Include guards use `#pragma once` per project convention.

---

## 7. Step 5: GameData Persistence for Listings

### Rationale

`GameData` already handles all DB read/write for game entities. We extend it with methods to persist transfer listings, following the same pattern as `saveToDB()` / `loadFromDB()`.

**Design decision:** Adding methods directly to `GameData` rather than creating a separate `TransferRepository` because:
1. It's only 3 simple queries (no complex joins or transactions)
2. It follows the existing pattern where `GameData` exposes specific persistence methods (e.g., `ageAllPlayers()` calls multiple per-player operations)
3. A separate repository class would add unnecessary indirection for this scope

### Dependencies

Requires Step 3 (new table + queries) and Step 4 (TransferListing struct) to be complete.

### Changes

#### `src/database/gamedata.h`

```cpp
// Forward declare TransferListing (don't need full definition in header)
struct TransferListing;

class GameData {
public:
    // ...existing methods...

    /**
     * @brief Saves a transfer listing to the database (UPSERT).
     * @param listing The listing to persist. Must have valid player_id, asking_price, listing_date.
     */
    void saveTransferListing(const TransferListing& listing) const;

    /**
     * @brief Deletes a transfer listing from the database.
     * @param player_id The player whose listing to remove.
     */
    void deleteTransferListing(PlayerID player_id) const;

    /**
     * @brief Loads all transfer listings from the database into a map.
     * @return Map of PlayerID -> TransferListing (seller_team_id is NOT set here,
     *         caller must set it from the player's current team).
     */
    std::unordered_map<PlayerID, TransferListing> loadAllTransferListings() const;
};
```

#### `src/database/gamedata.cpp`

```cpp
// Add includes at top
#include "model/transfer_listing.h"
#include "database/SQLLoader.h"
#include "global/queries.h"

void GameData::saveTransferListing(const TransferListing& listing) const
{
    sqlite3_stmt* stmt = db_conn->prepareStatement(
        SQLLoader::getQuery(Query::UPSERT_TRANSFER_LISTING));

    sqlite3_bind_int(stmt, 1, static_cast<int>(listing.player_id));
    sqlite3_bind_int(stmt, 2, static_cast<int>(listing.asking_price));
    std::string date_str = listing.listing_date.toString();
    sqlite3_bind_text(stmt, 3, date_str.c_str(), -1, SQLITE_TRANSIENT);

    db_conn->executeStep(stmt);
    sqlite3_finalize(stmt);
}

void GameData::deleteTransferListing(PlayerID player_id) const
{
    sqlite3_stmt* stmt = db_conn->prepareStatement(
        SQLLoader::getQuery(Query::DELETE_TRANSFER_LISTING));

    sqlite3_bind_int(stmt, 1, static_cast<int>(player_id));

    db_conn->executeStep(stmt);
    sqlite3_finalize(stmt);
}

std::unordered_map<PlayerID, TransferListing> GameData::loadAllTransferListings() const
{
    std::unordered_map<PlayerID, TransferListing> listings;

    sqlite3_stmt* stmt = db_conn->prepareStatement(
        SQLLoader::getQuery(Query::LOAD_ALL_TRANSFER_LISTINGS));

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        PlayerID pid = static_cast<PlayerID>(sqlite3_column_int(stmt, 0));
        uint32_t price = static_cast<uint32_t>(sqlite3_column_int(stmt, 1));
        const char* date_str = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));

        TransferListing listing;
        listing.player_id = pid;
        listing.asking_price = price;
        listing.listing_date = GameDateValue::fromString(date_str ? date_str : "");

        // seller_team_id is intentionally NOT set here — caller must set it
        // from the player's current team to handle transfers that happened
        // between save and load.
        listings[pid] = listing;
    }

    sqlite3_finalize(stmt);
    return listings;
}
```

### Verification

```bash
cd build && cmake .. && make -j$(nproc)
```

The project should compile. No runtime change expected yet since no code calls these new methods.

### Peer Review Checklist

- [ ] `saveTransferListing` uses `db_conn->executeStep(stmt)` followed by `sqlite3_finalize(stmt)` — matches the pattern in `PlayerRepository::insertPlayer`
- [ ] `loadAllTransferListings` leaves `seller_team_id` unset — the caller in `GameController::loadGame()` must resolve it from the Player's current team. This is intentional to handle the case where a player was transferred between save/load cycles.
- [ ] `GameDateValue::fromString()` expects format `YYYY-MM-DD`. The `toString()` method produces this format. Verify `listing_date` is never empty when saved.
- [ ] Error handling: if `date_str` is null, we pass empty string to `fromString()` which throws `std::runtime_error`. Should add a null check. **TODO: Fix during implementation.**
- [ ] The `const` qualifier on `saveTransferListing` and `deleteTransferListing` is correct — they only modify the DB through `db_conn` which is a shared_ptr to a non-const object.

---

## 8. Step 6: GameController — Core Methods

### Rationale

This is where the bulk of the transfer logic lives. We add member variables for listings and implement all core transfer operations. The `GameController` becomes the orchestrator for all transfer-related operations, keeping the `Game` class focused on simulation.

**Key design decisions:**
- `executeTransfer()` is a private method that ALL buy/sign/bid-accept flows go through — single point of truth
- Finances are updated in memory first, then persisted to DB in a transaction
- Transfer listings are a `std::unordered_map<PlayerID, TransferListing>` — O(1) lookup by player ID
- `canAffordPlayer()` checks both balance and wage impact to prevent teams from going bankrupt

### Dependencies

Requires Steps 1-4 (plumbing, bitmask, DB, model).

### Changes

See the full implementation below. The methods are organized into logical groups.

#### `src/controller/game_controller.h`

Add includes and new members/methods. Full additions:

```cpp
#include <unordered_map>
#include <optional>
#include <random>

#include "model/transfer_listing.h"
#include "database/repositories/player_repository.h"

class GameController {
public:
    // ========== Transfer Market: Listing ==========

    /** @brief Lists a player for transfer at the given asking price. Sets their status bit. */
    void listPlayerForTransfer(PlayerID player_id, uint32_t asking_price);

    /** @brief Removes a player from the transfer list. Clears their status bit. */
    void removePlayerFromTransfer(PlayerID player_id);

    /** @brief Quick check if a player is currently listed. */
    bool isPlayerListed(PlayerID player_id) const;

    // ========== Transfer Market: Queries ==========

    /** @brief Returns all listings (read-only). Used by the GUI scene. */
    const std::unordered_map<PlayerID, TransferListing>& getAllListings() const;

    /** @brief Gets all listings where the selling team is NOT the given team. For the Buy tab. */
    std::vector<const TransferListing*> getListingsExcludingTeam(TeamID team_id) const;

    /** @brief Filters listings by player role category (GK, DEF, MID, FW, etc.). */
    std::vector<const TransferListing*> getListingsByRole(PlayerRole role) const;

    // ========== Transfer Market: Buying ==========

    /** @brief Checks if a team has enough balance and wage budget to afford a player. */
    bool canAffordPlayer(TeamID buyer_id, PlayerID player_id) const;

    /** @brief Buys a listed player at the given price. Triggers executeTransfer(). */
    bool buyPlayer(PlayerID player_id, TeamID buyer_id, uint32_t price);

    /** @brief Signs a free agent (team_id == FREE_AGENTS_TEAM_ID, no fee). */
    bool signFreeAgent(PlayerID player_id, TeamID buyer_id);

    /** @brief Gets or calculates the market value for a player. */
    uint32_t getPlayerMarketValue(PlayerID player_id) const;

    // ========== Transfer Market: Bids & Negotiation ==========

    /** @brief Submits a bid on a listed player. Updates highest_bid if amount is higher. */
    bool submitBid(PlayerID player_id, TeamID bidder_id, uint32_t bid_amount);

    /** @brief Accepts the current highest bid and executes the transfer. */
    bool acceptBid(PlayerID player_id);

    /** @brief Rejects the current highest bid (keeps listing active). */
    bool rejectBid(PlayerID player_id);

    /** @brief Counter-offer: changes the asking price. The bidder re-evaluates next cycle. */
    bool counterOffer(PlayerID player_id, uint32_t new_price);

    /** @brief Checks if the current game date is within a transfer window (Jan / Jun-Aug). */
    bool isTransferWindowOpen() const;

    /** @brief Returns all active incoming bids for the player's managed team. */
    std::vector<std::pair<PlayerID, TransferListing>> getIncomingBids() const;

private:
    // ...existing members...

    /** @brief Map of all active transfer listings. Key = PlayerID. */
    std::unordered_map<PlayerID, TransferListing> transfer_listings;

    /**
     * @brief Core transfer execution. Updates in-memory state, finances, and DB.
     * @param pid Player to transfer.
     * @param buyer_id Team gaining the player.
     * @param seller_id Team losing the player (can be FREE_AGENTS_TEAM_ID).
     * @param price Transfer fee (0 for free agents).
     *
     * This is the single code path for all transfers:
     *   buyPlayer() → executeTransfer()
     *   signFreeAgent() → executeTransfer()
     *   acceptBid() → executeTransfer()
     */
    void executeTransfer(PlayerID pid, TeamID buyer_id, TeamID seller_id, uint32_t price);

    // ========== AI Helpers ==========

    /** @brief Evaluate squad depth for one team and act on buy/sell decisions. */
    void evaluateAndActForTeam(TeamID team_id);

    /** @brief Random float utility for AI decision-making. */
    static float randomFloat(float min, float max) {
        static std::mt19937 gen(std::random_device{}());
        std::uniform_real_distribution<float> dis(min, max);
        return dis(gen);
    }
};
```

#### `src/controller/game_controller.cpp` — additions

**Load listings at game start:**

```cpp
// In GameController::loadGame(), after game is created:
bool GameController::loadGame(int slot) {
    // ...existing code (path, gamedata, db_conn, game creation)...

    // Load transfer listings
    auto loaded = gamedata->loadAllTransferListings();
    for (auto& [pid, listing] : loaded)
    {
        auto player_opt = gamedata->getPlayer(pid);
        if (player_opt.has_value())
        {
            listing.seller_team_id = player_opt->get().getTeamId();

            // Sync the player's TransferStatus from what's in the DB
            gamedata->getPlayers().at(pid).setTransferStatus(TransferStatus::Listed);

            // Compute attention score on load
            listing.attention_score = calculateAttentionScore(pid);

            transfer_listings[pid] = listing;
        }
    }

    // ...rest of loadGame...
}

// In GameController::newGame(), transfer list starts empty:
void GameController::newGame(int slot) {
    // ...existing code...
    transfer_listings.clear();
    // ...rest...
}
```

**Listing Management:**

```cpp
void GameController::listPlayerForTransfer(PlayerID pid, uint32_t asking_price)
{
    auto player_opt = gamedata->getPlayer(pid);
    if (!player_opt.has_value()) return;

    // Don't list free agents
    if (player_opt->get().getTeamId() == FREE_AGENTS_TEAM_ID) return;

    float attention = calculateAttentionScore(pid);
    TransferListing listing(pid, player_opt->get().getTeamId(),
                            asking_price, game->getCurrentDate(), attention);
    transfer_listings[pid] = listing;

    // Update player status bitmask
    gamedata->getPlayers().at(pid).setTransferStatus(TransferStatus::Listed);

    // Persist to DB
    gamedata->saveTransferListing(listing);
}

void GameController::removePlayerFromTransfer(PlayerID pid)
{
    auto it = transfer_listings.find(pid);
    if (it == transfer_listings.end()) return;

    transfer_listings.erase(it);

    // Update player status bitmask
    auto player_opt = gamedata->getPlayer(pid);
    if (player_opt.has_value())
    {
        gamedata->getPlayers().at(pid).setTransferStatus(TransferStatus::NotListed);
    }

    // Remove from DB
    gamedata->deleteTransferListing(pid);
}

bool GameController::isPlayerListed(PlayerID pid) const
{
    return transfer_listings.contains(pid);
}
```

**Queries:**

```cpp
const std::unordered_map<PlayerID, TransferListing>& GameController::getAllListings() const
{
    return transfer_listings;
}

std::vector<const TransferListing*> GameController::getListingsExcludingTeam(TeamID team_id) const
{
    std::vector<const TransferListing*> result;
    for (const auto& [pid, listing] : transfer_listings)
    {
        if (listing.seller_team_id != team_id)
        {
            result.push_back(&listing);
        }
    }
    return result;
}

std::vector<const TransferListing*> GameController::getListingsByRole(PlayerRole role) const
{
    std::vector<const TransferListing*> result;
    for (const auto& [pid, listing] : transfer_listings)
    {
        auto player_opt = gamedata->getPlayer(pid);
        if (player_opt.has_value() && player_opt->get().getRole() == role)
        {
            result.push_back(&listing);
        }
    }
    return result;
}
```

**Budget Check:**

```cpp
bool GameController::canAffordPlayer(TeamID buyer_id, PlayerID pid, uint32_t target_price) const
{
    auto buyer_opt = gamedata->getTeam(buyer_id);
    auto player_opt = gamedata->getPlayer(pid);
    if (!buyer_opt.has_value() || !player_opt.has_value()) return false;

    const Player& player = player_opt->get();
    const Finances& finances = buyer_opt->get().getFinances();

    // Check that balance can cover the specific target price
    if (finances.getBalance() < static_cast<int64_t>(target_price))
    {
        return false;
    }

    // Check wage impact: total wage spending shouldn't exceed a reasonable % of balance
    int64_t current_wages = finances.getCurrentWageSpending(*gamedata);
    int64_t new_wage_total = current_wages + static_cast<int64_t>(player.getWage());

    // Simple heuristic: wage total < 50% of balance after transfer
    return new_wage_total < std::max(
        finances.getBalance() / 2,
        static_cast<int64_t>(100000)
    );
}
```

**Core Transfer Execution:**

```cpp
void GameController::executeTransfer(PlayerID pid, TeamID buyer_id, TeamID seller_id, uint32_t price)
{
    // 1. Validate teams exist
    auto buyer_opt = gamedata->getTeam(buyer_id);
    auto seller_opt = gamedata->getTeam(seller_id);
    if (!buyer_opt.has_value() || !seller_opt.has_value()) return;

    Team& buyer = buyer_opt->get();
    Team& seller = seller_opt->get();
    Player& player = gamedata->getPlayers().at(pid);

    // 2. Persist to database in a single transaction FIRST
    // If DB fails, in-memory state remains untouched
    db_conn->beginTransaction();
    try
    {
        PlayerRepository playerRepo(db_conn);
        playerRepo.transferPlayer(pid, buyer_id);
        
        // Temporarily modify player for DB update, then revert if needed
        TransferStatus old_status = player.getTransferStatus();
        player.setTransferStatus(TransferStatus::NotListed);
        playerRepo.updatePlayer(player);
        
        gamedata->deleteTransferListing(pid);
        db_conn->commitTransaction();
    }
    catch (const std::exception& e)
    {
        db_conn->rollbackTransaction();
        Logger::error("Transfer failed, transaction rolled back: " + std::string(e.what()));
        
        // Revert temporary change
        player.setTransferStatus(TransferStatus::Listed);
        throw;
    }

    // 3. Update finances (skip if free agent or zero price)
    if (seller_id != FREE_AGENTS_TEAM_ID && price > 0)
    {
        seller.getFinances().addBalance(static_cast<int64_t>(price));
        buyer.getFinances().subtractBalance(static_cast<int64_t>(price));
    }

    // 4. Update team roster lists
    if (seller_id != FREE_AGENTS_TEAM_ID)
    {
        seller.removePlayerID(pid);
    }
    buyer.addPlayerID(pid);

    // 5. In-memory transfer (updates player's team_id and _teamPlayers cache)
    gamedata->transferPlayer(pid, buyer_id);

    // 6. Clear transfer listing
    transfer_listings.erase(pid);

    // 7. Save full game state (fixtures, league points, game date)
    saveGame();
}
```

**Buy + Sign + Market Value:**

```cpp
bool GameController::buyPlayer(PlayerID pid, TeamID buyer_id, uint32_t price)
{
    auto it = transfer_listings.find(pid);
    if (it == transfer_listings.end()) return false;

    TransferListing& listing = it->second;
    
    // Validate price: must match either the asking price or the accepted bid
    if (price != listing.asking_price && price != listing.highest_bid)
    {
        return false; // Price mismatch
    }

    if (!canAffordPlayer(buyer_id, pid, price)) return false;

    executeTransfer(pid, buyer_id, listing.seller_team_id, price);
    return true;
}

bool GameController::signFreeAgent(PlayerID pid, TeamID buyer_id)
{
    auto player_opt = gamedata->getPlayer(pid);
    if (!player_opt.has_value()) return false;

    // Must be a free agent
    if (player_opt->get().getTeamId() != FREE_AGENTS_TEAM_ID) return false;

    // No fee — direct transfer
    executeTransfer(pid, buyer_id, FREE_AGENTS_TEAM_ID, 0);
    return true;
}

uint32_t GameController::getPlayerMarketValue(PlayerID pid) const
{
    auto player_opt = gamedata->getPlayer(pid);
    if (!player_opt.has_value()) return 0;

    const Player& player = player_opt->get();
    // updateMarketValue() is non-const, but we const-cast because
    // this is essentially a cache refresh, not a logical mutation.
    const_cast<Player&>(player).updateMarketValue(gamedata->getStatsConfig());
    return player.getMarketValue();
}
```

**Negotiation:**

```cpp
bool GameController::submitBid(PlayerID pid, TeamID bidder_id, uint32_t bid_amount)
{
    auto it = transfer_listings.find(pid);
    if (it == transfer_listings.end()) return false;

    // Can't bid on your own player
    if (bidder_id == it->second.seller_team_id) return false;

    // Must be during transfer window
    if (!isTransferWindowOpen()) return false;
    
    // Check affordability
    if (!canAffordPlayer(bidder_id, pid, bid_amount)) return false;

    // Only update if this is higher than the current highest bid
    if (bid_amount > it->second.highest_bid)
    {
        it->second.highest_bid = bid_amount;
        it->second.highest_bidder_id = bidder_id;
        return true;
    }

    return false; // Bid too low
}

bool GameController::acceptBid(PlayerID pid)
{
    auto it = transfer_listings.find(pid);
    if (it == transfer_listings.end()) return false;
    if (!it->second.highest_bidder_id.has_value()) return false;

    executeTransfer(pid, it->second.highest_bidder_id.value(),
                    it->second.seller_team_id, it->second.highest_bid);
    return true;
}

bool GameController::rejectBid(PlayerID pid)
{
    auto it = transfer_listings.find(pid);
    if (it == transfer_listings.end()) return false;

    // Clear the bid but keep the listing active
    it->second.highest_bid = 0;
    it->second.highest_bidder_id = std::nullopt;
    return true;
}

bool GameController::counterOffer(PlayerID pid, uint32_t new_price)
{
    auto it = transfer_listings.find(pid);
    if (it == transfer_listings.end()) return false;

    it->second.asking_price = new_price;
    it->second.highest_bid = 0;
    it->second.highest_bidder_id = std::nullopt;

    // Persist updated listing
    gamedata->saveTransferListing(it->second);
    return true;
}

bool GameController::isTransferWindowOpen() const
{
    return game->getCurrentDate().isTransferWindowOpen();
}

std::vector<std::pair<PlayerID, TransferListing>> GameController::getIncomingBids() const
{
    std::vector<std::pair<PlayerID, TransferListing>> bids;
    uint16_t managed_id = game->getManagedTeamId();

    for (const auto& [pid, listing] : transfer_listings)
    {
        if (listing.seller_team_id == managed_id &&
            listing.highest_bidder_id.has_value() &&
            listing.highest_bid > 0)
        {
            bids.emplace_back(pid, listing);
        }
    }
    return bids;
}
```

### Verification

```bash
cd build && cmake .. && make -j$(nproc)
```

No runtime visible change yet — methods are defined but not called from GUI or anywhere else.

### Peer Review Checklist

- [ ] `executeTransfer()` is transaction-safe: if the DB commit fails, the transaction rolls back. However, the in-memory state (finances, team rosters, player team_id) has already been modified by that point. The TODO comment acknowledges this inconsistency. A more robust approach would update in-memory state only after the DB commit succeeds, but this makes the error-recovery path simpler at the cost of potential inconsistency. For a single-player game this is acceptable.
- [ ] `canAffordPlayer()` uses market value as a rough check, but `buyPlayer()` doesn't re-check affordability against the actual asking price — it only checks the price matches. This means a player with high market value but low asking price (e.g., listed below value) would pass the affordability check based on market value but could still be unaffordable. **Potential bug:** The actual price needs to be compared to the balance in `buyPlayer()`. Add a check before `executeTransfer()`.
- [ ] `getPlayerMarketValue()` uses `const_cast` — this is a code smell. Consider making `updateMarketValue()` const, or having a separate `getCachedMarketValue()` that doesn't mutate. Acceptable for now since the method is lightweight (just an algorithm, no side effects beyond caching).
- [ ] `submitBid()` doesn't check if the bidder can afford the eventual price. The AI should check this before submitting, but a human could theoretically use a debug tool to submit a bid they can't afford. Consider adding a budget check.
- [ ] `transfer_listings` is not persisted when the listing is created by AI (only when `listPlayerForTransfer` calls `gamedata->saveTransferListing`). This is correct because `listPlayerForTransfer` is the single entry point.

---

## 9. Step 7: AI Evaluation System

### Rationale

AI teams need to:
1. Evaluate their squad depth per position (GK, CB, LB, RB, MID, WING, ST)
2. Calculate an "attention score" for each player (how desirable they are to other teams)
3. Decide which players to buy (based on missing positions) and sell (based on surplus)
4. Generate appropriate bids based on market value, need, scarcity, and player prestige
5. Evaluate counter-offers from the human player

**Design principles:**
- The AI evaluates its squad **contextually** — not all teams act the same way
- The "max price" formula creates natural variance: desperate teams overpay, opportunistic teams underbid
- The "attention score" determines which players attract interest, even if not listed — this simulates real-world scouting
- All random factors use seeded or static RNG for reproducibility in testing

### Dependencies

Requires Step 6 (core controller methods) for `submitBid()`, `listPlayerForTransfer()`, `getPlayerMarketValue()`.

### Implementation

#### `src/controller/game_controller.h` — Add private AI methods

```cpp
/**
 * @struct SquadNeeds
 * @brief Represents a team's surplus and deficit at each position group.
 *
 * Used by evaluateSquadNeeds() and evaluateAndActForTeam() to determine
 * which positions need reinforcements and which have excess players to sell.
 */
struct SquadNeeds {
    int missing_gk = 0;
    int missing_cb = 0;
    int missing_lb = 0;
    int missing_rb = 0;
    int missing_mid = 0;
    int missing_wing = 0;
    int missing_st = 0;

    int surplus_gk = 0;
    int surplus_cb = 0;
    int surplus_lb = 0;
    int surplus_rb = 0;
    int surplus_mid = 0;
    int surplus_wing = 0;
    int surplus_st = 0;

    std::optional<PlayerRole> upgrade_target; // Role that needs quality upgrade
};

/** Evaluates squad depth for a team and returns surplus/deficit counts. */
SquadNeeds evaluateSquadNeeds(TeamID team_id) const;

/**
 * Calculates how much attention a player attracts from other clubs.
 * Factors: overall, age, contract length, league reputation.
 * 
 * TODO: Add form factor from per-player match statistics (see GitHub Issue #XX).
 */
float calculateAttentionScore(PlayerID player_id) const;

/** Returns the attention multiplier for a league (top leagues = more attention). */
float getLeagueAttentionMultiplier(LeagueID league_id) const;

/**
 * Calculates the maximum price an AI team is willing to pay for a player.
 * Formula: market_value × need_multiplier × scarcity_multiplier × prestige_multiplier
 */
uint32_t calculateMaxPrice(PlayerID player_id, TeamID buyer_id,
                           const SquadNeeds& needs) const;

/** Main AI loop: evaluate one team and act on buy/sell decisions. */
void evaluateAndActForTeam(TeamID team_id);

/** Finds candidate players on the market that fill a given role for a team. */
std::vector<PlayerID> findTargetsForRole(PlayerRole role, TeamID buyer_id,
                                          const SquadNeeds& needs) const;

/** Maps a specific role (e.g., LW, RW) to its broad category (WING). */
PlayerRole getRoleCategory(PlayerRole role) const;

/** Calculates the available transfer budget for a team (balance × transfer_ratio). */
uint32_t transferBudgetForTeam(TeamID team_id) const;
```

#### `src/controller/game_controller.cpp` — AI implementations

```cpp
// ========== AI Squad Evaluation ==========

GameController::SquadNeeds GameController::evaluateSquadNeeds(TeamID team_id) const
{
    SquadNeeds needs;
    const auto& team_players = gamedata->getPlayersForTeam(team_id);
    if (team_players.empty()) return needs;

    int gk = 0, cb = 0, lb = 0, rb = 0, mid = 0, wing = 0, st = 0;

    for (const auto& pref : team_players)
    {
        const Player& p = pref.get();
        // Skip players already listed for sale so we correctly identify replacements
        if (isPlayerListed(p.getId())) continue;

        switch (p.getRole())
        {
            case PlayerRole::GK:                                gk++;   break;
            case PlayerRole::CB:                                cb++;   break;
            case PlayerRole::LB:                                lb++;   break;
            case PlayerRole::RB:                                rb++;   break;
            case PlayerRole::CDM: case PlayerRole::CM:
            case PlayerRole::CAM:                               mid++;  break;
            case PlayerRole::LM:  case PlayerRole::RM:
            case PlayerRole::LW:  case PlayerRole::RW:          wing++; break;
            case PlayerRole::ST:                                st++;   break;
            default: break;
        }
    }

    // Target squad sizes per role group
    constexpr int TARGET_GK = 2, TARGET_CB = 3, TARGET_LB = 1, TARGET_RB = 1;
    constexpr int TARGET_MID = 4, TARGET_WING = 2, TARGET_ST = 2;

    auto calc = [](int current, int target, int& missing, int& surplus) {
        if (current < target) { missing = target - current; surplus = 0; }
        else                  { missing = 0;                surplus = current - target; }
    };

    calc(gk,   TARGET_GK,   needs.missing_gk,   needs.surplus_gk);
    calc(cb,   TARGET_CB,   needs.missing_cb,   needs.surplus_cb);
    calc(lb,   TARGET_LB,   needs.missing_lb,   needs.surplus_lb);
    calc(rb,   TARGET_RB,   needs.missing_rb,   needs.surplus_rb);
    calc(mid,  TARGET_MID,  needs.missing_mid,  needs.surplus_mid);
    calc(wing, TARGET_WING, needs.missing_wing, needs.surplus_wing);
    calc(st,   TARGET_ST,   needs.missing_st,   needs.surplus_st);

    // Identify upgrade target for ambitious/wealthy teams
    auto team_opt = gamedata->getTeam(team_id);
    if (team_opt.has_value() && team_opt->get().getFinances().getBalance() > 10000000) {
        // Simple logic: Find the starting position with lowest overall to target an upgrade
        // (Implementation details left to actual team strategy logic)
        // needs.upgrade_target = PlayerRole::...;
    }

    return needs;
}

// ========== Attention Score ==========

float GameController::calculateAttentionScore(PlayerID pid) const
{
    auto player_opt = gamedata->getPlayer(pid);
    if (!player_opt.has_value()) return 0.0f;

    const Player& p = player_opt->get();
    const StatsConfig& config = gamedata->getStatsConfig();

    // 1. Base: overall rating (0..100 → 0..1)
    float score = static_cast<float>(p.getOverall(config)) / 100.0f;
    if (score <= 0.0f) return 0.0f;

    // 2. Age factor: prospect boost, prime steady, decline penalty
    if      (p.getAge() < 20)  score *= 1.2f;   // Young prospect, high potential
    else if (p.getAge() <= 28) score *= 1.0f;   // Prime years
    else if (p.getAge() <= 32) score *= 0.8f;   // Past peak, declining
    else                       score *= 0.4f;   // Veteran, minimal interest

    // 3. Contract length: shorter = more likely to be available
    if      (p.getContractYears() <= 1) score *= 1.3f;   // Bargain buy
    else if (p.getContractYears() <= 2) score *= 1.1f;   // Reasonable

    // 4. League attention factor (top leagues attract more global interest)
    auto team_opt = gamedata->getTeam(p.getTeamId());
    if (team_opt.has_value()) {
        score *= getLeagueAttentionMultiplier(team_opt->get().getLeagueId());
    }

    // 5. TODO: Form factor — replace 1.0f with actual form when implemented
    // See GitHub Issue #XX: Track per-player match statistics
    score *= 1.0f;

    return std::clamp(score, 0.0f, 1.5f);
}

float GameController::getLeagueAttentionMultiplier(LeagueID league_id) const
{
    // Higher reputation leagues attract more attention from abroad.
    // These weights affect how likely players from a league are to get
    // cross-league transfer offers.
    switch (league_id) {
        case 2:  return 1.3f;  // Spanish League
        case 3:  return 1.3f;  // English League
        case 1:  return 1.2f;  // Italian League
        case 4:  return 1.2f;  // German League
        case 5:  return 1.1f;  // French League
        case 11: return 1.0f;  // Brazilian League
        case 10: return 1.0f;  // Argentinian League
        case 12: return 0.9f;  // Portuguese League
        default: return 0.8f;  // Lower/other leagues
    }
}

// ========== Max Price Calculation ==========

uint32_t GameController::calculateMaxPrice(PlayerID pid, TeamID buyer_id,
                                            const SquadNeeds& needs) const
{
    auto player_opt = gamedata->getPlayer(pid);
    auto buyer_opt = gamedata->getTeam(buyer_id);
    if (!player_opt.has_value() || !buyer_opt.has_value()) return 0;

    const Player& p = player_opt->get();
    uint32_t market_value = getPlayerMarketValue(pid);
    if (market_value == 0) return 0;

    // 1. Need multiplier: how badly does this team need this position?
    auto getNeedMult = [&](PlayerRole role) -> float {
        switch (getRoleCategory(role)) {
            case PlayerRole::GK:
                return needs.missing_gk >= 2 ? 2.2f : (needs.missing_gk == 1 ? 1.6f : 0.7f);
            case PlayerRole::CB:
                return needs.missing_cb >= 2 ? 2.0f : (needs.missing_cb == 1 ? 1.5f : 0.8f);
            case PlayerRole::LB:
                return needs.missing_lb >= 1 ? 1.8f : 0.7f;
            case PlayerRole::RB:
                return needs.missing_rb >= 1 ? 1.8f : 0.7f;
            case PlayerRole::CM:
                return needs.missing_mid >= 2 ? 2.0f : (needs.missing_mid == 1 ? 1.5f : 0.8f);
            case PlayerRole::LW:
                return needs.missing_wing >= 2 ? 2.0f : (needs.missing_wing == 1 ? 1.5f : 0.8f);
            case PlayerRole::ST:
                return needs.missing_st >= 2 ? 2.2f : (needs.missing_st == 1 ? 1.6f : 0.8f);
            default: return 1.0f;
        }
    };
    float need_mult = getNeedMult(p.getRole());

    // 2. Scarcity multiplier: how many players of this role are on the market?
    //    Fewer available = buyer willing to pay more.
    auto role_listings = getListingsByRole(getRoleCategory(p.getRole()));
    float scarcity_mult;
    if (role_listings.size() < 3)       scarcity_mult = 1.5f;  // Rare
    else if (role_listings.size() < 8)  scarcity_mult = 1.1f;  // Normal
    else                                scarcity_mult = 0.8f;  // Buyer's market

    // 3. Prestige multiplier: how much better is this player than the team's average?
    const auto& team_players = gamedata->getPlayersForTeam(buyer_id);
    float team_avg = 0.0f;
    int count = 0;
    for (const auto& ref : team_players) {
        team_avg += static_cast<float>(ref.get().getOverall(gamedata->getStatsConfig()));
        count++;
    }
    team_avg = count > 0 ? team_avg / count : 50.0f;

    float player_ovr = static_cast<float>(p.getOverall(gamedata->getStatsConfig()));
    float diff = player_ovr - team_avg;

    float prestige_mult;
    if      (diff > 15.0f)  prestige_mult = 1.5f;  // Star — overpay
    else if (diff > 5.0f)   prestige_mult = 1.2f;  // Above average
    else if (diff > -5.0f)  prestige_mult = 1.0f;  // Squad level
    else if (diff > -15.0f) prestige_mult = 0.8f;  // Below average
    else                    prestige_mult = 0.6f;  // Fringe

    // 4. Budget constraint: don't spend more than available
    uint32_t budget = transferBudgetForTeam(buyer_id);
    float budget_cap = static_cast<float>(budget) / static_cast<float>(market_value);

    return static_cast<uint32_t>(
        static_cast<float>(market_value) * need_mult * scarcity_mult * prestige_mult *
        std::min(1.0f, budget_cap)
    );
}

// ========== Find Targets ==========

std::vector<PlayerID> GameController::findTargetsForRole(
    PlayerRole role, TeamID buyer_id, const SquadNeeds& needs) const
{
    std::vector<std::pair<PlayerID, float>> candidates;
    PlayerRole broad_cat = getRoleCategory(role);

    for (const auto& [pid, listing] : transfer_listings)
    {
        // Skip own team's listings
        if (listing.seller_team_id == buyer_id) continue;

        auto player_opt = gamedata->getPlayer(pid);
        if (!player_opt.has_value()) continue;

        const Player& p = player_opt->get();

        // Only consider players in the same broad category
        if (getRoleCategory(p.getRole()) != broad_cat) continue;

        // Score by attention (pre-calculated on listing)
        float score = listing.attention_score;

        // Bonus for exact role match (e.g., need a LB and player is LB, not CB)
        if (p.getRole() == role) score *= 1.2f;

        // Only consider if affordable
        if (canAffordPlayer(buyer_id, pid, listing.asking_price)) {
            candidates.emplace_back(pid, score);
        }
    }

    // Sort by attention score descending
    std::ranges::sort(candidates, [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    std::vector<PlayerID> result;
    for (const auto& [pid, score] : candidates) {
        result.push_back(pid);
    }
    return result;
}

// ========== AI Team Activity ==========

void GameController::evaluateAndActForTeam(TeamID team_id)
{
    SquadNeeds needs = evaluateSquadNeeds(team_id);
    uint32_t budget = transferBudgetForTeam(team_id);
    if (budget == 0) return; // No money to spend

    // ---- BUYING ----
    // For each missing role, find the best available target and bid
    auto tryBuy = [&](PlayerRole role, int missing_count) {
        if (missing_count <= 0) return;
        auto targets = findTargetsForRole(role, team_id, needs);
        if (targets.empty()) return;

        // Best target = first in sorted list (highest attention score)
        PlayerID target = targets[0];
        uint32_t max_price = calculateMaxPrice(target, team_id, needs);

        // Initial bid: random between 60% and 120% of market value, capped by max_price
        uint32_t market_val = getPlayerMarketValue(target);
        uint32_t initial_bid = static_cast<uint32_t>(
            static_cast<float>(market_val) * randomFloat(0.6f, 1.2f)
        );
        uint32_t bid = std::min(initial_bid, max_price);

        if (bid > 0 && static_cast<int64_t>(bid) <= static_cast<int64_t>(budget)) {
            submitBid(target, team_id, bid);
        }
    };

    tryBuy(PlayerRole::GK,  needs.missing_gk);
    tryBuy(PlayerRole::CB,  needs.missing_cb);
    tryBuy(PlayerRole::LB,  needs.missing_lb);
    tryBuy(PlayerRole::RB,  needs.missing_rb);
    tryBuy(PlayerRole::CM,  needs.missing_mid);
    tryBuy(PlayerRole::LW,  needs.missing_wing);
    tryBuy(PlayerRole::ST,  needs.missing_st);

    // Try upgrading a position if squad has sufficient depth and budget
    if (needs.upgrade_target.has_value()) {
        tryBuy(needs.upgrade_target.value(), 1);
    }

    // ---- SELLING ----
    // For each surplus role, list the worst player for transfer
    auto trySell = [&](PlayerRole role, int surplus_count) {
        if (surplus_count <= 1) return; // Keep at least 1 spare

        const auto& team_players = gamedata->getPlayersForTeam(team_id);
        std::vector<PlayerID> candidates;
        for (const auto& ref : team_players) {
            const Player& p = ref.get();
            if (getRoleCategory(p.getRole()) == role && !isPlayerListed(p.getId())) {
                candidates.push_back(p.getId());
            }
        }

        if (candidates.empty()) return;

        // Sell the lowest-rated player in that role
        std::ranges::sort(candidates, [&](PlayerID a, PlayerID b) {
            auto pa = gamedata->getPlayer(a);
            auto pb = gamedata->getPlayer(b);
            if (!pa.has_value() || !pb.has_value()) return false;
            return pa->get().getOverall(gamedata->getStatsConfig()) <
                   pb->get().getOverall(gamedata->getStatsConfig());
        });

        PlayerID to_sell = candidates[0];
        // Asking price: slightly negotiable (80-130% of market value)
        uint32_t asking = static_cast<uint32_t>(
            getPlayerMarketValue(to_sell) * randomFloat(0.8f, 1.3f)
        );
        if (asking > 0) {
            listPlayerForTransfer(to_sell, asking);
        }
    };

    trySell(PlayerRole::GK,  needs.surplus_gk);
    trySell(PlayerRole::CB,  needs.surplus_cb);
    trySell(PlayerRole::LB,  needs.surplus_lb);
    trySell(PlayerRole::RB,  needs.surplus_rb);
    trySell(PlayerRole::CM,  needs.surplus_mid);
    trySell(PlayerRole::LW,  needs.surplus_wing);
    trySell(PlayerRole::ST,  needs.surplus_st);
}

// ========== Helpers ==========

PlayerRole GameController::getRoleCategory(PlayerRole role) const
{
    switch (role) {
        case PlayerRole::GK:  return PlayerRole::GK;
        case PlayerRole::CB:
        case PlayerRole::LB:
        case PlayerRole::RB:  return PlayerRole::CB;    // All defenders map to CB for evaluation
        case PlayerRole::CDM:
        case PlayerRole::CM:
        case PlayerRole::CAM: return PlayerRole::CM;    // All central mids
        case PlayerRole::LM:
        case PlayerRole::RM:
        case PlayerRole::LW:
        case PlayerRole::RW:  return PlayerRole::LW;    // All wingers
        case PlayerRole::ST:  return PlayerRole::ST;
        default:              return PlayerRole::UNKNOWN;
    }
}

uint32_t GameController::transferBudgetForTeam(TeamID team_id) const
{
    auto team_opt = gamedata->getTeam(team_id);
    if (!team_opt.has_value()) return 0;

    const Finances& finances = team_opt->get().getFinances();
    int64_t balance = finances.getBalance();

    // Transfer budget is ratio% of balance (default ratio is 0.5 = 50%)
    // Clamped to avoid negative or zero budget for teams with negative balance
    if (balance <= 0) return 0;

    float ratio = finances.getTransferToWagesRatio();
    return static_cast<uint32_t>(static_cast<float>(balance) * std::clamp(ratio, 0.0f, 1.0f));
}
```

### Verification

No direct test yet — AI activity is only triggered by `advanceDay()`. You can verify by:
1. Starting a new game as a team
2. Using `console` or debug overlay to call `processAITransferActivity()` manually
3. Checking the `getAllListings()` count increases

### Peer Review Checklist

- [x] `evaluateSquadNeeds()` skips players that are already listed for transfer (`isPlayerListed`). This ensures teams seek replacements *before* the listed player is sold.
- [ ] `calculateAttentionScore()` has a TODO for form factor. When adding form tracking later, the multiplier should be applied at step 5. Design the form factor to be in range [0.8, 1.3] so it doesn't dominate the overall score.
- [x] `calculateMaxPrice()` can return a value higher than the team's budget because `budget_cap` is multiplied as the last factor. This is intentional.
- [ ] `findTargetsForRole()` only considers LISTED players. This means unlisted star players never attract AI bids. Tracked as future enhancement.
- [ ] `trySell()` only lists players who are not already listed. Prevents duplicate listings.
- [ ] The `randomFloat()` function uses `static std::mt19937 gen(std::random_device{}())`. For a single-player game this is acceptable.
- [x] Transfer budget uses `getTransferToWagesRatio()`. AI teams start with balance 0 (from `DataGenerator::generateTeams()`). **Potential issue:** AI teams may never buy or sell if they have zero balance. Need to implement matchday revenue or initial balance.
- [x] `canAffordPlayer()` takes `target_price` to check affordability explicitly. `buyPlayer` checks actual price, `findTargets` checks asking price.
- [x] `executeTransfer()` is transaction-safe: The DB commit happens FIRST, and if it fails, the in-memory state is unmodified. No inconsistencies.
- [ ] `getPlayerMarketValue()` uses `const_cast` — this is a code smell. Consider making `updateMarketValue()` const in the future.
- [x] `submitBid()` checks `canAffordPlayer` with the bid amount before submitting.
- [x] `attention_score` is pre-calculated when a listing is created or loaded from DB, saving thousands of calculations per day during `findTargetsForRole`.
- [x] AI teams have an `upgrade_target` heuristic to ensure they improve their squad even if they aren't quantitatively missing players in a position.

---

## 10. Step 8: Transfer Window Integration

### Rationale

AI transfer activity is triggered automatically during `advanceDay()`. We hook into `GameController::advanceDay()` which currently just calls `game->advanceDay()`. During transfer windows, each AI team has a ~15% chance per day to evaluate their squad and act.

**Why 15% per day?** Over a 7-day period, each team has approximately a 68% chance of acting at least once (`1 - (0.85^7) ≈ 0.68`). This means most teams act about once a week, while some may act twice and some not at all — creating natural variance without fixed schedules.

### Dependencies

Requires Step 7 (AI evaluation functions) to be implemented.

### Changes

#### `src/controller/game_controller.cpp`

```cpp
// FROM (current):
void GameController::advanceDay() { game->advanceDay(); }

// TO:
void GameController::advanceDay()
{
    game->advanceDay();

    // During transfer windows, AI teams periodically evaluate their squads
    if (isTransferWindowOpen())
    {
        processAITransferActivity();
    }
}
```

### Verification

1. Start a new game as any team
2. Fast-forward through several days with the "Next Day" button
3. During June- August or January, check if AI teams start listing players and making bids
4. Verify no AI activity occurs outside transfer windows (September-December, February-May)

### Peer Review Checklist

- [ ] `processAITransferActivity()` is called EVERY day during the window, not weekly. The 15% per-team chance limits activity. This is intentional — some days are busy, some are quiet.
- [ ] The player's managed team is excluded from AI activity (`evaluateAndActForTeam` skips it).
- [ ] The free agents team (ID 0) is also excluded.
- [ ] Performance: iterating over all teams (currently ~240 teams across 12 leagues) and querying listings is O(n) per day. With the 15% random skip applied per-team, most days only ~36 teams actually execute the heavier `evaluateAndActForTeam()`. Acceptable performance.

---

## 11. Step 9: TransferMarketScene GUI

### Rationale

A full ImGui scene accessible from the sidebar button "Transfer Market" in `MainGameScene`. The scene has three tabs:

1. **Buy Players** — Browse all transfer-listed players (excluding own team), apply filters (role, age, max price), buy listed players, and sign free agents.
2. **Your Listings** — View and manage your own listed players, list new players for transfer.
3. **Incoming Bids** — View bids from AI teams on your listed players, accept/reject/counter.

### Dependencies

Requires Step 6 (core controller methods) and Step 4 (TransferListing model). The GUI calls controller methods; no AI logic is needed for the GUI to function.

### New Files

#### `src/gui/scenes/transfer_market_scene.h`

```cpp
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "gui/gui_scene.h"
#include "model/player.h"
#include "model/transfer_listing.h"

/**
 * @class TransferMarketScene
 * @brief Full-screen ImGui scene for the transfer market.
 *
 * Layout: Top bar (budget + window status), tab bar, content area.
 * Tabs: Buy Players, Your Listings, Incoming Bids.
 * Dialogs: Confirmation (buy/sign), Counter-offer.
 */
class TransferMarketScene : public GUIScene
{
public:
    explicit TransferMarketScene(GUIView* parent);
    ~TransferMarketScene() override = default;

    void onEnter() override;
    void update(float deltaTime) override;
    void render() override;
    [[nodiscard]] SceneID getID() const override;

private:
    enum class Tab : uint8_t { BUY, LISTINGS, BIDS };

    Tab active_tab = Tab::BUY;

    // Filters for Buy tab
    int filter_role_index = 0;
    int filter_max_age = 40;
    float filter_max_price = 100000000.0f;

    // Confirm transfer dialog
    struct ConfirmState {
        bool open = false;
        PlayerID player_id = 0;
        TeamID seller_id = 0;
        uint32_t price = 0;
        bool is_free_agent = false;
    };
    ConfirmState confirm_state;

    // Counter-offer dialog
    struct CounterState {
        bool open = false;
        PlayerID player_id = 0;
    };
    CounterState counter_state;

    // Cached data
    std::vector<const TransferListing*> cached_listings;
    std::vector<std::pair<PlayerID, TransferListing>> cached_bids;
    std::vector<std::reference_wrapper<const Player>> cached_free_agents;

    void refreshData();
    void renderBuyTab();
    void renderListingsTab();
    void renderBidsTab();
    void renderConfirmDialog();
    void renderCounterDialog();
};
```

#### `src/gui/scenes/transfer_market_scene.cpp`

See the full implementation in the codebase — this is approximately 350 lines of ImGui code. Key sections:

```cpp
void TransferMarketScene::render()
{
    // Full-viewport window with no decoration
    ImGui::Begin(LOC("TRANSFER_TITLE"), nullptr, ImGuiWindowFlags_NoDecoration | ...);

    // Top bar: budget + transfer window status
    // Tabs: Buy / Your Listings / Incoming Bids
    // Back button

    switch (active_tab) {
        case Tab::BUY:      renderBuyTab(); break;
        case Tab::LISTINGS: renderListingsTab(); break;
        case Tab::BIDS:     renderBidsTab(); break;
    }

    // Overlay dialogs
    renderConfirmDialog();
    renderCounterDialog();

    ImGui::End();
}

void TransferMarketScene::renderBuyTab()
{
    // Filter row: Role dropdown, Max Age slider, Max Price slider
    // Table: Name | Team | Role | Age | OVR | Value | Price | [Buy]
    // After listed players: separator + "FREE AGENTS" section
    // Each free agent row: Name | "FREE AGENT" | Role | Age | OVR | Value | "Free" | [Sign]
}

void TransferMarketScene::renderListingsTab()
{
    // "List a Player" section: dropdown of team players + price input + [List] button
    // Table of currently listed players: Name | Role | OVR | Price | [Unlist]
}

void TransferMarketScene::renderBidsTab()
{
    // Table of incoming bids: Name | Bidder | Amount | vs Value % | [Accept] [Reject] [Counter]
}
```

### Wire-up in MainGameScene

In `main_game_scene.cpp`, replace the empty Transfer Market button handler:

```cpp
// Before (does nothing):
if (ImGui::Button(LOC("MAIN_GAME_TRANSFER_MARKET"), ...)) {}

// After:
if (ImGui::Button(LOC("MAIN_GAME_TRANSFER_MARKET"), ...)) {
    guiView->overlayScene(std::make_unique<TransferMarketScene>(guiView));
}
```

Add include:
```cpp
#include "gui/scenes/transfer_market_scene.h"
```

### SceneID Registration

In `gui_scene.h`, add to the enum:
```cpp
enum class SceneID : uint8_t {
    MAIN_MENU, SETTINGS, GAME_MENU, LINEUP,
    TEAM_SELECTION, ROSTER, STRATEGY,
    TRANSFER_MARKET,  // NEW
};
```

### Verification

```bash
cd build && cmake .. && make -j$(nproc) && ./FootballManagement
```

1. Start a new game and select a team
2. Click "Transfer Market" in the sidebar
3. Verify the three tabs render correctly
4. Try listing a player, then buying/signing, then checking bids
5. Verify the back button returns to the main game

### Peer Review Checklist

- [ ] All UI strings use the `LOC()` macro for localization. No hardcoded English strings.
- [ ] The `renderBuyTab()` fetches listings excluding the managed team via `getListingsExcludingTeam()` — players can't buy from themselves.
- [ ] Free agents are loaded from `FREE_AGENTS_TEAM_ID` (0) and displayed with a [Sign] button instead of [Buy].
- [ ] Confirmation dialog checks `canAffordPlayer()` before executing the transfer (or shows an error).
- [ ] After any transfer action, `refreshData()` is called to update listings, bids, and free agents.
- [ ] The scene uses `guiView->overlayScene()` not `changeScene()` — it's pushed as an overlay on top of the main game, so the main game scene is preserved underneath.
- [ ] `SceneID::TRANSFER_MARKET` doesn't conflict with any existing enum value.

---

## 12. Step 10: Localization

### `assets/lang/English.json`

Add these keys (alphabetical order within the file, placed with the other game keys):

```json
"TRANSFER_TITLE": "Transfer Market",
"TRANSFER_BUDGET": "Transfer Budget: €%s",
"TRANSFER_WINDOW_OPEN": "Transfer Window: OPEN",
"TRANSFER_WINDOW_CLOSED": "Transfer Window: CLOSED",
"TRANSFER_TAB_BUY": "Buy Players",
"TRANSFER_TAB_LISTINGS": "Your Listings",
"TRANSFER_TAB_BIDS": "Incoming Bids",
"TRANSFER_FILTERS": "Filters",
"TRANSFER_FILTER_ROLE": "Role",
"TRANSFER_FILTER_AGE": "Max Age",
"TRANSFER_FILTER_PRICE": "Max Price (€)",
"TRANSFER_ALL": "All",
"TRANSFER_COL_NAME": "Name",
"TRANSFER_COL_TEAM": "Team",
"TRANSFER_COL_ROLE": "Role",
"TRANSFER_COL_AGE": "Age",
"TRANSFER_COL_OVR": "OVR",
"TRANSFER_COL_VALUE": "Value",
"TRANSFER_COL_PRICE": "Price",
"TRANSFER_COL_ACTION": "Action",
"TRANSFER_COL_BIDDER": "Bidder",
"TRANSFER_COL_AMOUNT": "Amount",
"TRANSFER_COL_VS_VALUE": "vs Value",
"TRANSFER_BUY": "Buy",
"TRANSFER_SIGN": "Sign",
"TRANSFER_FREE": "Free",
"TRANSFER_FREE_AGENTS": "Free Agents",
"TRANSFER_FREE_AGENT_LABEL": "FREE AGENT",
"TRANSFER_LIST_PLAYER": "List for Transfer",
"TRANSFER_UNLIST": "Remove from Market",
"TRANSFER_ASKING_PRICE": "Asking Price (€)",
"TRANSFER_SELECT_PLAYER": "Select Player",
"TRANSFER_YOUR_LISTINGS": "Your Listed Players",
"TRANSFER_INCOMING_BIDS": "Incoming Bids",
"TRANSFER_NO_BIDS": "No incoming bids at the moment.",
"TRANSFER_CONFIRM_TITLE": "Confirm Transfer",
"TRANSFER_CONFIRM_BUY": "Buy %s for €%d?",
"TRANSFER_CONFIRM_SIGN": "Sign %s (Free Agent)?",
"TRANSFER_CONFIRM": "Confirm",
"TRANSFER_CANCEL": "Cancel",
"TRANSFER_WAGE": "Wage: €%d/week",
"TRANSFER_CONTRACT": "Contract: %d years",
"TRANSFER_COUNTER_TITLE": "Counter Offer",
"TRANSFER_CURRENT_BID": "Current bid: €%d",
"TRANSFER_NEW_PRICE": "New price (€)",
"TRANSFER_SUBMIT_COUNTER": "Submit Counter",
"TRANSFER_NOT_ENOUGH_BUDGET": "Not enough budget!"
```

### `assets/lang/Italian.json`

Add corresponding Italian translations.

---

## 13. Step 11: Build System

### `src/CMakeLists.txt`

Add the new source files within their respective sections:

```cmake
set(CORE_SOURCES
    # ...existing...

    # Model (add after model/team.cpp or in alphabetical position)
    model/transfer_listing.h
    model/transfer_listing.cpp

    # GUI Scenes (add after existing scenes)
    gui/scenes/transfer_market_scene.h
    gui/scenes/transfer_market_scene.cpp

    # ...rest of CORE_SOURCES...
)
```

### Verification

```bash
cd build && cmake .. && make -j$(nproc)
```

---

## 14. Step 12: Documentation & GitHub Issue

### Documentation File

This document (`docs/TRANSFER_MARKET.md`) serves as the primary documentation.

### GitHub Issue

Create a new issue for the form tracking enhancement:

```markdown
## [Enhancement] Track per-player match statistics and integrate into transfer attention score

**Description:**
The transfer AI currently uses a basic attention score (overall + age + contract + league).
This should be enhanced with actual match performance data (goals, assists, MOTM, average rating).

**TODO comment location (to update after implementation):**
- `src/controller/game_controller.cpp` — `calculateAttentionScore()` step 5

**Required changes:**
1. Track per-player events in MatchEngine (goals, assists, key passes, tackles)
2. Add `PlayerSeasonStats` DB table with columns: player_id, season, goals, assists, appearances, avg_rating
3. Calculate a "form" rating (average of last 5-10 matches weighted by recency)
4. Integrate form into `calculateAttentionScore()` as a multiplier in [0.8, 1.3]
5. Update this document with the new formula

**Labels:** `enhancement`, `gameplay`, `todo`
```

---

## 15. Appendix: Full File Change Summary

| # | File | Action | Est. LOC |
|---|------|--------|----------|
| 1 | `src/model/game.h` | Modify: constructor takes `shared_ptr<DatabaseConnection>` | -1 line |
| 2 | `src/model/game.cpp` | Modify: constructor body | -2 lines |
| 3 | `src/controller/game_controller.h` | Modify: add `db_conn` + transfer method declarations | +60 |
| 4 | `src/controller/game_controller.cpp` | Modify: plumbing + transfer methods + AI system | +450 |
| 5 | `src/model/player.h` | Modify: add `TRANSFER_LISTED_BIT` constant | +2 |
| 6 | `src/model/player.cpp` | Modify: bitmask encoding in ctor + setTransferStatus | +8 |
| 7 | `src/model/transfer_listing.h` | **CREATE** | +40 |
| 8 | `src/model/transfer_listing.cpp` | **CREATE** | +15 |
| 9 | `src/database/gamedata.h` | Modify: add 3 persistence method declarations | +10 |
| 10 | `src/database/gamedata.cpp` | Modify: implement listing save/load/delete | +60 |
| 11 | `src/gui/gui_scene.h` | Modify: add `TRANSFER_MARKET` to SceneID | +1 |
| 12 | `src/gui/scenes/transfer_market_scene.h` | **CREATE** | +80 |
| 13 | `src/gui/scenes/transfer_market_scene.cpp` | **CREATE** | +350 |
| 14 | `src/gui/scenes/main_game_scene.cpp` | Modify: wire button + add include | +5 |
| 15 | `src/global/queries.h` | Modify: add 3 Query enum values + map entries | +8 |
| 16 | `assets/db/schema.sql` | Modify: add TransferList table | +7 |
| 17 | `assets/db/queries.sql` | Modify: add 3 transfer list queries | +15 |
| 18 | `assets/lang/English.json` | Modify: add ~35 keys | +35 |
| 19 | `assets/lang/Italian.json` | Modify: add ~35 keys | +35 |
| 20 | `src/CMakeLists.txt` | Modify: add 4 new source files | +4 |
| 21 | `docs/TRANSFER_MARKET.md` | **CREATE** (this document) | +1200 |
| | **Total new code** | | **~1,300 LOC** |

---

## Implementation Sequence (Recommended)

```
Week 1: Infrastructure
  Step 1  (game.h/.cpp + game_controller.h/.cpp plumbing)  → compile ✓
  Step 2  (player.h/.cpp bitmask)                           → compile ✓
  Step 3  (schema.sql + queries.sql + queries.h)             → compile ✓
  Step 4  (transfer_listing.h/.cpp)                          → compile ✓
  Step 5  (gamedata.h/.cpp persistence)                      → compile ✓

Week 2: Logic
  Step 6  (game_controller.h/.cpp core methods)              → compile ✓
  Step 7  (game_controller.h/.cpp AI system)                 → compile ✓
  Step 8  (game_controller.cpp advanceDay hook)              → compile ✓

Week 3: UI + Polish
  Step 9  (transfer_market_scene.h/.cpp + SceneID + button)  → compile + run ✓
  Step 10 (English.json + Italian.json)                      → compile ✓
  Step 11 (CMakeLists.txt)                                   → compile ✓
  Step 12 (GitHub Issue)                                     → done ✓
```

After each step, compile and run:
```bash
cd build && cmake .. && make -j$(nproc) && ./FootballManagement
```
