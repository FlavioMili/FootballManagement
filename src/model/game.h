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
#include "model/competition_manager.h"
#include "model/gamedate.h"
#include "model/match.h"
#include "model/match_report.h"
#include "model/transfer_market.h"
#include "model/world_simulation.h"

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
   * @param conn Shared pointer to the DatabaseConnection.
   */
  explicit Game(std::shared_ptr<class GameData> gd,
                std::shared_ptr<DatabaseConnection> conn);

  /**
   * @brief Advances the game time by one day, simulating any matches or events
   * scheduled for the current date.
   */
  void advanceDay();

  /** Records a managed match exactly once and applies its consequences. */
  bool setMatchResult(const GameDateValue& date, TeamID home_id, TeamID away_id,
                      uint8_t home_score, uint8_t away_score);

  /**
   * Records a managed match with its structured report (e.g. built from the
   * live MatchEngine). A drawn cup tie is settled by extra time and
   * penalties unless the report already carries them.
   */
  bool setMatchResult(const GameDateValue& date, TeamID home_id, TeamID away_id,
                      MatchReport report);

  /** Standings, cups, reports, player season stats and season history. */
  const CompetitionManager& getCompetitions() const { return competitions; }

  /** Inbox, board, injuries, finances and development between matches. */
  WorldSimulation& getWorld() { return world; }
  const WorldSimulation& getWorld() const { return world; }

  /** Deals, loans, pre-contracts, scheduled payments and transfer history. */
  TransferMarket& getTransfers() { return transfers; }
  const TransferMarket& getTransfers() const { return transfers; }

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
  Calendar& getCalendar();

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

  // Matchday simulation helper
  void simulateMatches(std::vector<Match>& matches,
                       bool include_managed = false);

  std::shared_ptr<DatabaseConnection> db_conn;
  std::shared_ptr<class GameData> gamedata;
  Calendar calendar;
  CompetitionManager competitions;
  WorldSimulation world;
  TransferMarket transfers;
  GameDateValue currentDate;
  uint8_t current_season = 1;
  uint16_t managed_team_id;
};
