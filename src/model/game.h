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
#include <vector>

#include "database/database_connection.h"
#include "model/calendar.h"
#include "model/gamedate.h"
#include "model/match.h"

/**
 * @class Game
 * @brief Represents the core game state and logic.
 *
 * The Game class orchestrates the simulation mechanics, handling the
 * progression of time, saving and loading the game state, and triggering match
 * simulations.
 */
class Game
{
 public:
  /**
   * @brief Constructs a new Game object.
   * @param gd Shared pointer to the GameData instance.
   * @param db_path Path to the SQLite database file for this game.
   */
  explicit Game(std::shared_ptr<class GameData> gd, const std::string& db_path);

  /**
   * @brief Advances the game time by one day, simulating any matches or events
   * scheduled for the current date.
   */
  void advanceDay();

  /**
   * @brief Retrieves the current in-game date.
   * @return A constant reference to the current GameDateValue.
   */
  const GameDateValue& getCurrentDate() const;

  /**
   * @brief Retrieves the calendar containing all fixtures.
   * @return A constant reference to the game Calendar.
   */
  const Calendar& getCalendar() const;

  /**
   * @brief Retrieves the current season number.
   * @return The current season as an integer.
   */
  int getCurrentSeason() const;

  /**
   * @brief Retrieves the ID of the team currently managed by the user.
   * @return The managed team's ID.
   */
  uint16_t getManagedTeamId() const;

  /**
   * @brief Sets the team managed by the user.
   * @param id The ID of the team to manage.
   */
  void setManagedTeamId(uint16_t id);

  /**
   * @brief Saves the current game state, including calendar and game variables,
   * to the database.
   */
  void saveGame();

 private:
  void loadGame();
  void endSeason();
  void handleSeasonTransition();
  void startNewSeason();

  // Player training
  void trainPlayers(const std::vector<uint32_t>& player_ids);

  // Matchday simulation helper
  void simulateMatches(std::vector<Match>& matches);
  void updateStandings(const Match& match);

  std::shared_ptr<DatabaseConnection> db_conn;
  std::shared_ptr<class GameData> gamedata;
  Calendar calendar;
  GameDateValue currentDate;
  uint8_t current_season = 1;
  uint16_t managed_team_id;
};
