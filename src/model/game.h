// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "database/database.h"
#include "global/stats_config.h"
#include "model/calendar.h"
#include "model/match.h"

class Game {
public:
  Game();

  void simulateWeek();
  void startNewSeason();
  void loadData();
  void advanceWeek();
  void endSeason();
  void handleSeasonTransition();

  int getCurrentSeason() const;
  int getCurrentWeek() const;

  void updateStandings(const Match &match);

  void saveGame();

private:
  std::shared_ptr<Database> db;
  StatsConfig stats_config;
  std::map<size_t, Calendar> league_calendars;

  uint16_t managed_team_id = 0;
  uint16_t managed_league_id = 0;
  uint8_t current_season;
  uint8_t current_week;
  std::string current_date;

  void loadConfigs();
  void initializeDatabase();
  void ensureManagedTeamAssigned();

  void generateAllCalendars();
  void trainPlayers(const std::vector<uint32_t> &player_ids);
};
