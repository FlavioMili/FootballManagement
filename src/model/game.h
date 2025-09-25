// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include "json.hpp"
#include <cstddef>
#include <cstdint>
#include <fstream>
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

  bool hasSelectedTeam() const;

  void saveGame();

private:
  std::shared_ptr<Database> db;
  StatsConfig stats_config;
  nlohmann::json raw_stats_config_json;
  std::map<size_t, Calendar> league_calendars;

  uint8_t current_season;
  uint8_t current_week;
  std::string current_date;

  void loadConfigs();
  void initializeDatabase();
  void ensureManagedTeamAssigned();

  void generateAllCalendars();
  void trainPlayers(const std::vector<uint32_t> &player_ids);

  template <typename T> T loadJsonFile(const std::string &path) {
    std::ifstream f(path);
    if (!f.is_open()) {
      throw std::runtime_error("FATAL: Could not open " + path);
    }
    return nlohmann::json::parse(f).get<T>();
  }

  template <typename T>
  T loadJsonFileKey(const std::string &path, const std::string &key) {
    std::ifstream f(path);
    if (!f.is_open()) {
      throw std::runtime_error("FATAL: Could not open " + path);
    }
    return nlohmann::json::parse(f).at(key).get<T>();
  }
};
