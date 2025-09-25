// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include "league.h"
#include "player.h"
#include "team.h"
#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>
#include <vector>

class GameData {
 public:
  // Singleton access
  static GameData &instance() {
    static GameData inst;
    return inst;
  }

  GameData(const GameData &) = delete;
  GameData(GameData &&) = delete;
  GameData &operator=(const GameData &) = delete;
  GameData &operator=(GameData &&) = delete;

  // DB
  bool loadFromDB();
  bool saveToDB() const;

  // League
  void addLeague(uint8_t id, const League &league);
  std::optional<std::reference_wrapper<const League>>
  getLeague(uint8_t id) const;
  const std::unordered_map<uint8_t, League> &getLeagues() const;
  std::unordered_map<uint8_t, League> &getLeagues();
  const std::vector<std::reference_wrapper<const League>> &
  getLeaguesVector() const;

  // Team
  void addTeam(uint16_t id, const Team &team);
  std::optional<std::reference_wrapper<const Team>> getTeam(uint16_t id) const;
  const std::unordered_map<uint16_t, Team> &getTeams() const;
  std::unordered_map<uint16_t, Team> &getTeams();
  const std::vector<std::reference_wrapper<const Team>> &getTeamsVector() const;

  // Player
  void addPlayer(uint32_t id, const Player &player);
  std::optional<std::reference_wrapper<const Player>>
  getPlayer(uint32_t id) const;
  const std::unordered_map<uint32_t, Player> &getPlayers() const;
  std::unordered_map<uint32_t, Player> &getPlayers();
  const std::vector<std::reference_wrapper<const Player>> &
  getPlayersVector() const;
  bool removePlayer(uint32_t id);

 private:
  GameData();

  std::unordered_map<uint8_t, League> _leagues;
  std::unordered_map<uint16_t, Team> _teams;
  std::unordered_map<uint32_t, Player> _players;

  std::vector<std::reference_wrapper<const League>> _leaguesVec;
  std::vector<std::reference_wrapper<const Team>> _teamsVec;
  std::vector<std::reference_wrapper<const Player>> _playersVec;
};
