// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <memory>

#include "database/database_connection.h"
#include "global/types.h"
#include "model/board.h"

/**
 * @class WorldStateRepository
 * @brief Persists the world seed, the player id counter (table WorldState)
 * and the managed club's board state (table BoardState).
 */
class WorldStateRepository
{
 public:
  explicit WorldStateRepository(std::shared_ptr<DatabaseConnection> db_conn);

  /** Loads the world state; false when the save predates it. */
  bool loadWorldState(std::uint64_t& seed, PlayerID& next_player_id) const;

  /** Writes the world state. */
  void saveWorldState(std::uint64_t seed, PlayerID next_player_id) const;

  /** Loads the board state; false when none is stored. */
  bool loadBoard(BoardState& board) const;

  /** Writes the board state (a single row). */
  void saveBoard(const BoardState& board) const;

 private:
  std::shared_ptr<DatabaseConnection> db_conn;
};
