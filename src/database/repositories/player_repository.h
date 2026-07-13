// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <memory>
#include <vector>

#include "database/database_connection.h"
#include "model/player.h"

struct sqlite3_stmt;

class PlayerRepository
{
 public:
  explicit PlayerRepository(std::shared_ptr<DatabaseConnection> db_conn);

  std::vector<Player> loadAllPlayers() const;
  void insertPlayer(const Player& player) const;
  void insertPlayerWithId(const Player& player) const;
  void insertPlayers(
      const std::vector<std::reference_wrapper<const Player>>& players) const;

  void updatePlayer(const Player& player) const;
  void updatePlayers(
      const std::vector<std::reference_wrapper<const Player>>& players) const;

  void deletePlayer(PlayerID player_id) const;
  void transferPlayer(PlayerID player_id, uint16_t new_team_id) const;

 private:
  std::shared_ptr<DatabaseConnection> db_conn;

  void bindPlayerParams(sqlite3_stmt* stmt, const Player& player,
                        int startIndex) const;
};
