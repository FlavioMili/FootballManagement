// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include "league.h"
#include "match.h"
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

  // Fixtures
  void insertFixture(const Match &match, uint8_t league_id);
  void saveMatches(const std::vector<Match> &matches);
  std::vector<Match> loadAllMatches();


  // Game State
  bool isFirstRun();
  void updateGameState(uint8_t current_season, uint16_t managed_team_id,
                       const std::string &game_date);
  bool loadGameState(uint8_t &current_season, uint16_t &managed_team_id,
                     std::string &game_date) const;

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
  void loadTeamsForLeague(League &league) const;
};
