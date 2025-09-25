// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include "model/league.h"
#include "model/player.h"
#include "model/team.h"
#include "global/stats_config.h"
#include <vector>
#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>
#include <memory>

class Database;

class GameData {
public:
  static GameData &instance();

  GameData(const GameData &) = delete;
  GameData &operator=(const GameData &) = delete;

  // ---------------- DB ----------------
  bool loadFromDB(std::shared_ptr<Database> database_ptr, bool is_first_run);
  bool saveToDB() const;

  // ---------------- StatsConfig ----------------
  void loadStatsConfig();
  const StatsConfig &getStatsConfig() const;

  // ---------------- League ----------------
  void addLeague(uint8_t id, const League &league);
  std::optional<std::reference_wrapper<const League>> getLeague(uint8_t id) const;
  const std::unordered_map<uint8_t, League> &getLeagues() const;
  std::unordered_map<uint8_t, League> &getLeagues();
  const std::vector<std::reference_wrapper<const League>> &getLeaguesVector() const;

  // ---------------- Team ----------------
  void addTeam(uint16_t id, const Team &team);
  std::optional<std::reference_wrapper<Team>> getTeam(uint16_t id);
  std::optional<std::reference_wrapper<const Team>> getTeam(uint16_t id) const;
  const std::unordered_map<uint16_t, Team> &getTeams() const;
  std::unordered_map<uint16_t, Team> &getTeams();
  const std::vector<std::reference_wrapper<const Team>> &getTeamsVector() const;

  // ---------------- Player ----------------
  void addPlayer(uint32_t id, const Player &player);
  std::optional<std::reference_wrapper<const Player>> getPlayer(uint32_t id) const;
  const std::unordered_map<uint32_t, Player> &getPlayers() const;
  std::unordered_map<uint32_t, Player> &getPlayers();
  const std::vector<std::reference_wrapper<const Player>> &getPlayersVector() const;
  std::vector<std::reference_wrapper<const Player>> getPlayersForTeam(uint16_t team_id) const;
  bool removePlayer(uint32_t id);

  uint16_t getManagedTeamId() const;
  void setManagedTeamId(uint16_t id);

private:
  GameData();

  std::unordered_map<uint8_t, League> _leagues;
  std::vector<std::reference_wrapper<const League>> _leaguesVec;
  std::unordered_map<uint16_t, Team> _teams;
  std::vector<std::reference_wrapper<const Team>> _teamsVec;
  std::unordered_map<uint32_t, Player> _players;
  std::vector<std::reference_wrapper<const Player>> _playersVec;
  uint16_t managed_team_id;
  StatsConfig stats_config;
};
