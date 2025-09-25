// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "gamedata.h"
#include "database.h"
#include "datagenerator.h"
#include "global/paths.h"
#include <algorithm>

GameData::GameData() { loadFromDB(); }

// ---------------- League ----------------
void GameData::addLeague(uint8_t id, const League &league) {
  _leagues.emplace(id, league);
  if (!_leaguesVec.empty())
    _leaguesVec.push_back(_leagues.at(id));
}

std::optional<std::reference_wrapper<const League>>
GameData::getLeague(uint8_t id) const {
  if (auto it = _leagues.find(id); it != _leagues.end())
    return it->second;
  return std::nullopt;
}

const std::unordered_map<uint8_t, League> &GameData::getLeagues() const {
  return _leagues;
}

std::unordered_map<uint8_t, League> &GameData::getLeagues() {
  return _leagues;
}

const std::vector<std::reference_wrapper<const League>> &
GameData::getLeaguesVector() const {
  return _leaguesVec;
}

// ---------------- Team ----------------
void GameData::addTeam(uint16_t id, const Team &team) {
  _teams.emplace(id, team);
  if (!_teamsVec.empty())
    _teamsVec.push_back(_teams.at(id));
}

std::optional<std::reference_wrapper<const Team>>
GameData::getTeam(uint16_t id) const {
  if (auto it = _teams.find(id); it != _teams.end())
    return it->second;
  return std::nullopt;
}

const std::unordered_map<uint16_t, Team> &GameData::getTeams() const {
  return _teams;
}

std::unordered_map<uint16_t, Team> &GameData::getTeams() {
  return _teams;
}
const std::vector<std::reference_wrapper<const Team>> &
GameData::getTeamsVector() const {
  return _teamsVec;
}

// ---------------- Player ----------------
void GameData::addPlayer(uint32_t id, const Player &player) {
  _players.emplace(id, player);
  if (!_playersVec.empty())
    _playersVec.push_back(_players.at(id));
}

std::optional<std::reference_wrapper<const Player>>
GameData::getPlayer(uint32_t id) const {
  if (auto it = _players.find(id); it != _players.end())
    return it->second;
  return std::nullopt;
}

const std::unordered_map<uint32_t, Player> &GameData::getPlayers() const {
  return _players;
}

std::unordered_map<uint32_t, Player> &GameData::getPlayers() {
  return _players;
}
const std::vector<std::reference_wrapper<const Player>> &
GameData::getPlayersVector() const {
  return _playersVec;
}

bool GameData::removePlayer(uint32_t id) {
  bool erased = _players.erase(id) > 0;
  if (erased && !_playersVec.empty()) {
    // Remove from vector
    _playersVec.erase(std::remove_if(_playersVec.begin(), _playersVec.end(),
                                     [id](const Player &p) {
                                       return p.getId() == id;
                                     }),
                      _playersVec.end());
  }
  return erased;
}

// ---------------- DB ----------------
bool GameData::loadFromDB() {
  Database db;
  if (db.isFirstRun()) {
    auto leagues = DataGenerator::generateLeagues("assets/user_made_data/leagues/leagues.json");
    for (const auto &league : leagues) {
      db.insertLeagueWithId(league);
      _leagues.emplace(league.getId(), league);
    }

    auto teams = DataGenerator::generateTeams("assets/user_made_data/teams/italian_league.json");
    for (const auto &team : teams) {
      db.insertTeamWithId(team);
      _teams.emplace(team.getId(), team);
    }

    auto players = DataGenerator::generatePlayers("assets/user_made_data/players/lecce_players.json");
    for (const auto &player : players) {
      db.insertPlayerWithId(player);
      _players.emplace(player.getId(), player);
    }
  } else {
    auto leagues = db.loadAllLeagues();
    for (const auto &league : leagues) {
      _leagues.emplace(league.getId(), league);
    }

    auto teams = db.loadAllTeams();
    for (const auto &team : teams) {
      _teams.emplace(team.getId(), team);
    }

    auto players = db.loadAllPlayers();
    for (const auto &player : players) {
      _players.emplace(player.getId(), player);
    }
  }

  // Build vectors once
  _leaguesVec.clear();
  _leaguesVec.reserve(_leagues.size());
  for (auto &p : _leagues)
    _leaguesVec.push_back(p.second);

  _teamsVec.clear();
  _teamsVec.reserve(_teams.size());
  for (auto &p : _teams)
    _teamsVec.push_back(p.second);

  _playersVec.clear();
  _playersVec.reserve(_players.size());
  for (auto &p : _players)
    _playersVec.push_back(p.second);

  return true;
}

bool GameData::saveToDB() const {
  Database db;
  for (const auto &pair : _leagues) {
    db.insertLeague(pair.second);
  }
  for (const auto &pair : _teams) {
    db.insertTeam(pair.second);
  }
  for (const auto &pair : _players) {
    db.updatePlayer(pair.second);
  }
  return true;
}
