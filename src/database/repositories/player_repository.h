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

/**
 * @class PlayerRepository
 * @brief Repository class for managing Player entities in the database.
 */
class PlayerRepository
{
 public:
  /**
   * @brief Construct a new Player Repository object.
   * @param db_conn Shared pointer to the database connection.
   */
  explicit PlayerRepository(std::shared_ptr<DatabaseConnection> db_conn);

  /**
   * @brief Load all players from the database.
   * @return A vector of Player objects.
   */
  std::vector<Player> loadAllPlayers() const;

  /**
   * @brief Insert a new player into the database.
   * @param player The Player object to insert.
   */
  void insertPlayer(const Player& player) const;

  /**
   * @brief Insert a player with a specific ID into the database.
   * @param player The Player object to insert.
   */
  void insertPlayerWithId(const Player& player) const;

  /**
   * @brief Insert multiple players into the database.
   * @param players A vector of reference wrappers to Player objects.
   */
  void insertPlayers(
      const std::vector<std::reference_wrapper<const Player>>& players) const;

  /**
   * @brief Update an existing player in the database.
   * @param player The Player object with updated details.
   */
  void updatePlayer(const Player& player) const;

  /**
   * @brief Update multiple players in the database.
   * @param players A vector of reference wrappers to Player objects.
   */
  void updatePlayers(
      const std::vector<std::reference_wrapper<const Player>>& players) const;

  /**
   * @brief Delete a player from the database by ID.
   * @param player_id The ID of the player to delete.
   */
  void deletePlayer(PlayerID player_id) const;

  /**
   * @brief Transfer a player to a new team in the database.
   * @param player_id The ID of the player to transfer.
   * @param new_team_id The ID of the new team.
   */
  void transferPlayer(PlayerID player_id, uint16_t new_team_id) const;

 private:
  std::shared_ptr<DatabaseConnection> db_conn;

  void bindPlayerParams(sqlite3_stmt* stmt, const Player& player,
                        int startIndex) const;
};
