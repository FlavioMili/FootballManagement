// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "gamedate.h"
#include "global/stats_config.h"
#include "global/types.h"
#include "model/league.h"
#include "model/player.h"
#include "model/team.h"

class DatabaseConnection;

/**
 * @class GameData
 * @brief Singleton class that acts as an in-memory cache for the database
 * state.
 *
 * GameData is responsible for loading the initial game state from SQLite,
 * holding all core entities (Leagues, Teams, Players), and facilitating
 * high-performance read/write access during gameplay without hitting the disk.
 */
class GameData
{
 public:
  /**
   * @brief Constructs a new GameData instance.
   */
  GameData();

  GameData(const GameData&) = delete;
  GameData& operator=(const GameData&) = delete;

  // ---------------- DB ----------------
  /**
   * @brief Loads the initial game state from the database.
   * @param database_ptr Shared pointer to the active DatabaseConnection.
   * @return True if successful, false otherwise.
   */
  bool loadFromDB(std::shared_ptr<DatabaseConnection> database_ptr);

  /**
   * @brief Saves the current in-memory state back to the database.
   * @return True if successful, false otherwise.
   */
  bool saveToDB() const;

  // ---------------- StatsConfig ----------------
  const StatsConfig& getStatsConfig() const;

  // ---------------- League ----------------
  void addLeague(LeagueID id, const League& league);

  std::optional<std::reference_wrapper<const League>> getLeague(
      LeagueID id) const;

  const std::unordered_map<LeagueID, League>& getLeagues() const;

  std::unordered_map<LeagueID, League>& getLeagues();

  const std::vector<std::reference_wrapper<const League>>& getLeaguesVector()
      const;

  // ---------------- Team ----------------
  void addTeam(TeamID id, const Team& team);

  std::optional<std::reference_wrapper<Team>> getTeam(TeamID id);

  std::optional<std::reference_wrapper<const Team>> getTeam(TeamID id) const;

  const std::unordered_map<TeamID, Team>& getTeams() const;

  std::unordered_map<TeamID, Team>& getTeams();

  const std::vector<std::reference_wrapper<const Team>>& getTeamsVector() const;

  void ageAllPlayers();

  void addPlayer(PlayerID id, const Player& player);
  std::optional<std::reference_wrapper<const Player>> getPlayer(
      PlayerID id) const;
  const std::unordered_map<PlayerID, Player>& getPlayers() const;
  std::unordered_map<PlayerID, Player>& getPlayers();
  const std::vector<std::reference_wrapper<const Player>>& getPlayersVector()
      const;
  std::vector<std::reference_wrapper<const Player>> getPlayersForTeam(
      TeamID team_id) const;
  bool removePlayer(PlayerID id);
  void transferPlayer(PlayerID id, TeamID new_team_id);

 private:
  std::unordered_map<LeagueID, League> _leagues;
  std::vector<std::reference_wrapper<const League>> _leaguesVec;
  std::unordered_map<TeamID, Team> _teams;
  std::vector<std::reference_wrapper<const Team>> _teamsVec;
  std::unordered_map<PlayerID, Player> _players;
  std::vector<std::reference_wrapper<const Player>> _playersVec;
  std::unordered_map<TeamID, std::vector<PlayerID>> _teamPlayers;
  StatsConfig stats_config;
  std::shared_ptr<DatabaseConnection> db_conn;

  void loadStatsConfig();
  void generateAndSaveInitialData();
  void loadExistingData();
};
