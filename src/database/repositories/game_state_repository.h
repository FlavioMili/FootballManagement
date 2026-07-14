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

/**
 * @class GameStateRepository
 * @brief Repository class for managing the game state in the database.
 */
class GameStateRepository
{
 public:
  /**
   * @brief Construct a new Game State Repository object.
   * @param db_conn Shared pointer to the database connection.
   */
  explicit GameStateRepository(std::shared_ptr<DatabaseConnection> db_conn);

  /**
   * @brief Check if this is the first time the game is run.
   * @return True if it is the first run, false otherwise.
   */
  bool isFirstRun() const;

  /**
   * @brief Update the current game state in the database.
   * @param current_season The current season number.
   * @param managed_team_id The ID of the team managed by the player.
   * @param game_date The current date in the game as a string.
   */
  void updateGameState(uint8_t current_season, uint16_t managed_team_id,
                       const std::string& game_date) const;

  /**
   * @brief Load the game state from the database.
   * @param current_season Reference to store the current season number.
   * @param managed_team_id Reference to store the managed team ID.
   * @param game_date Reference to store the current game date.
   * @return True if the game state was successfully loaded, false otherwise.
   */
  bool loadGameState(uint8_t& current_season, uint16_t& managed_team_id,
                     std::string& game_date) const;

 private:
  std::shared_ptr<DatabaseConnection> db_conn;
};
