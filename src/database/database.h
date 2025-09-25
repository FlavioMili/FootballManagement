// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include "calendar.h"
#include "global/paths.h"
#include "league.h"
#include "player.h"
#include "team.h"
#include <cstdint>
#include <memory>
#include <sqlite3.h>
#include <string>
#include <vector>

class Database {
 public:
  explicit Database();
  ~Database() = default;

  void initialize();
  bool isFirstRun() const;

  // Data Loading
  std::vector<League> loadAllLeagues();
  std::vector<Team> loadAllTeams();
  std::vector<Player> loadAllPlayers();

  // Data Insertion
  void insertLeague(const League &league);
  void insertTeam(const Team &team);
  void insertPlayer(const Player &player);
  void insertLeagueWithId(const League &league);
  void insertTeamWithId(const Team &team);
  void insertPlayerWithId(const Player &player);

  // Data-generation related
  void addFirstName(const std::string &name);
  void addLastName(const std::string &name);
  std::vector<std::string> getFirstNames() const;
  std::vector<std::string> getLastNames() const;

  // Calendar
  void saveCalendar(const Calendar &calendar, int season, uint8_t league_id);
  Calendar loadCalendar(int season, uint8_t league_id) const;

  // Game State
  void saveManagedTeamId(int team_id);
  int loadManagedTeamId() const;
  void updateGameState(int season, int week, int managed_team_id);
  void loadGameState(int &season, int &week, int &managed_team_id) const;

  // League Points
  void saveLeaguePoints(const League &league);
  void loadLeaguePoints(League &league) const;
  void resetAllLeaguePoints();

  // Player specific actions
  void ageAllPlayers();
  void updatePlayer(const Player &player);
  void deletePlayer(uint32_t player_id);
  void transferPlayer(uint32_t player_id, uint16_t new_team_id);

 private:
  std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db;

  void loadSQLFiles();
};
