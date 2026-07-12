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
#include <string>

#include "database/database_connection.h"

class GameStateRepository
{
 public:
  explicit GameStateRepository(std::shared_ptr<DatabaseConnection> db_conn);

  bool isFirstRun() const;
  void updateGameState(uint8_t current_season, uint16_t managed_team_id,
                       const std::string& game_date) const;
  bool loadGameState(uint8_t& current_season, uint16_t& managed_team_id,
                     std::string& game_date) const;

 private:
  std::shared_ptr<DatabaseConnection> db_conn;
};
