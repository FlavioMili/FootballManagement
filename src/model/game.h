// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include "database/database.h"
#include "global/stats_config.h"
#include "match.h"
#include "model/calendar.h"
#include "model/gamedate.h"

class Game {
 public:
  Game();

  // Simulation
  void startNewSeason();
  void loadData();
  void advanceDay(); // Advance simulation by 1 day
  void endSeason();
  void handleSeasonTransition();

  // Accessors
  int getCurrentSeason() const;
  GameDateValue getCurrentDate() const;

  // Update league standings after a match
  void updateStandings(const Match &match);

  // Save / load game state
  void saveGame();

 private:
  std::shared_ptr<Database> db;
  StatsConfig stats_config;

  // League calendars mapped by league ID
  std::map<size_t, Calendar> league_calendars;

  // Current season
  uint8_t current_season;

  // Initialization helpers
  void loadConfigs();
  void initializeDatabase();
  void ensureManagedTeamAssigned();
  void generateAllCalendars();

  // Player training
  void trainPlayers(const std::vector<uint32_t> &player_ids);

  // Matchday simulation helper
  void simulateMatches(const std::vector<Matchup> &matches);
};
