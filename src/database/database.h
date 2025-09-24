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
  void close();
  bool isFirstRun() const;

  void addLeague(const std::string &name);
  std::vector<League> getLeagues() const;

  void addFirstName(const std::string &name);
  void addLastName(const std::string &name);
  std::vector<std::string> getFirstNames() const;
  std::vector<std::string> getLastNames() const;

  void addTeam(uint8_t league_id, const std::string &name, int64_t balance);
  std::vector<Team> getTeams(const uint8_t league_id) const;
  void updateTeam(const Team &team);

  void addPlayer(int team_id, const Player &player);
  std::vector<uint32_t> getPlayers(uint16_t team_id) const;

  void saveCalendar(const Calendar &calendar, int season, uint8_t league_id);
  Calendar loadCalendar(int season, uint8_t league_id) const;

  void saveManagedTeamId(int team_id);
  int loadManagedTeamId() const;

  void updateGameState(int season, int week, int managed_team_id);
  void loadGameState(int &season, int &week, int &managed_team_id) const;

  void saveLeaguePoints(const League &league);
  void loadLeaguePoints(League &league) const;
  void resetAllLeaguePoints();

  void ageAllPlayers();
  void updatePlayer(const Player &player);
  void deletePlayer(uint32_t player_id);

  std::vector<uint32_t> getFreeAgents() const;
  std::vector<uint32_t> getAllPlayers() const;
  std::vector<int> getAllTeamIds() const;
  void transferPlayer(uint32_t player_id, uint16_t new_team_id);

private:
  std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db;

  // Helper methods
  void loadSQLFiles();
  void executeStatement(const std::string &query_id,
                        const std::vector<std::string> &params = {});
};
