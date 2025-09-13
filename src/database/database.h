// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include "global.h"
#include "player.h"
#include "team.h"
#include "league.h"
#include "calendar.h"

class Database {
 public:
  explicit Database(const std::string& db_path, 
      const std::string& schema_path = SCHEMA_PATH,
      const std::string& queries_path = QUERIES_PATH);
  ~Database();

  void initialize();
  void close();
  bool isFirstRun() const;

  void addLeague(const std::string& name);
  std::vector<League> getLeagues() const;

  void addFirstName(const std::string& name);
  void addLastName(const std::string& name);
  std::vector<std::string> getFirstNames() const;
  std::vector<std::string> getLastNames() const;

  void addTeam(int league_id, const std::string& name, uint64_t balance);
  std::vector<Team> getTeams(const int league_id) const;
  void updateTeam(const Team& team);

  void addPlayer(int team_id, const Player& player);
  std::vector<Player> getPlayers(int team_id) const;

  void saveCalendar(const Calendar& calendar, int season, int league_id);
  Calendar loadCalendar(int season, int league_id) const;

  void saveManagedTeamId(int team_id);
  int loadManagedTeamId() const;

  void updateGameState(int season, int week, int managed_team_id);
  void loadGameState(int& season, int& week, int& managed_team_id) const;

  void saveLeaguePoints(const League& league);
  void loadLeaguePoints(League& league) const;
  void resetAllLeaguePoints();

  void ageAllPlayers();
  void updatePlayer(const Player& player);
  void deletePlayer(int player_id);

  std::vector<Player> getFreeAgents() const;
  std::vector<Player> getAllPlayers() const;
  std::vector<int> getAllTeamIds() const;
  void transferPlayer(int player_id, int new_team_id);

 private:
  struct Impl;
  std::unique_ptr<Impl> pImpl;

  std::string schema_path_;
  std::string queries_path_;

  // Helper methods
  void loadSQLFiles();
  void executeStatement(const std::string& query_id, 
        const std::vector<std::string>& params = {});
};
