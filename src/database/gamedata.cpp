// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "database/gamedata.h"

#include "database/database.h"
#include "database/datagenerator.h"
#include "global/logger.h"
#include "global/paths.h"
#include "json.hpp"
#include <algorithm>
#include <fstream>

GameData &GameData::instance() {
  static GameData instance;
  return instance;
}

GameData::GameData() {
  // Constructor is kept empty, initialization is done in loadFromDB
}

// ---------------- DB ----------------
bool GameData::loadFromDB(std::shared_ptr<Database> database_ptr) {
  loadStatsConfig();
  db = database_ptr;
  is_first_run = db->isFirstRun();

  Logger::debug("GameData::loadFromDB called. is_first_run: " +
                std::to_string(is_first_run));

  std::vector<Team> all_teams;
  if (is_first_run) {
    database_ptr->initialize();
    Logger::debug("Database initialized. Generating data.");

    auto leagues_data = DataGenerator::generateLeagues();
    all_teams = DataGenerator::generateTeams();

    for (const auto &team : all_teams) {
      database_ptr->insertTeamWithId(team);
      _teams.emplace(team.getId(), team);
    }
    _teamsVec.reserve(_teams.size());
    for (auto &p : _teams)
      _teamsVec.push_back(p.second);

    std::map<uint8_t, std::vector<uint16_t>> league_teams_map;
    for (const auto &team : all_teams) {
      league_teams_map[team.getLeagueId()].push_back(team.getId());
    }

    for (const auto &league_data : leagues_data) {
      database_ptr->insertLeagueWithId(league_data);
      _leagues.emplace(league_data.getId(),
                       League(league_data.getId(), league_data.getName(),
                              league_teams_map[league_data.getId()]));
    }

    auto players = DataGenerator::generatePlayers();
    for (const auto &player : players) {
      database_ptr->insertPlayer(player);
      _players.emplace(player.getId(), player);
    }

  } else {
    auto leagues_from_db = database_ptr->loadAllLeagues();
    all_teams = database_ptr->loadAllTeams();

    for (const auto &team : all_teams) {
      _teams.emplace(team.getId(), team);
    }

    std::map<uint8_t, std::vector<uint16_t>> league_teams_map;
    for (const auto &team : all_teams) {
      league_teams_map[team.getLeagueId()].push_back(team.getId());
    }

    for (const auto &league_from_db : leagues_from_db) {
      _leagues.emplace(league_from_db.getId(),
                       League(league_from_db.getId(), league_from_db.getName(),
                              league_teams_map[league_from_db.getId()]));
    }

    auto players = database_ptr->loadAllPlayers();
    for (const auto &player : players) {
      _players.emplace(player.getId(), player);
    }
    Logger::debug("Loaded all data from database.");
  }

  // Build vectors from the maps
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
  for (const auto &pair : _leagues) {
    db->insertLeague(pair.second);
  }
  for (const auto &pair : _teams) {
    db->insertTeam(pair.second);
  }
  for (const auto &pair : _players) {
    db->updatePlayer(pair.second);
  }
  return true;
}

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

std::unordered_map<uint8_t, League> &GameData::getLeagues() { return _leagues; }

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

std::optional<std::reference_wrapper<Team>> GameData::getTeam(uint16_t id) {
  if (auto it = _teams.find(id); it != _teams.end())
    return it->second;
  return std::nullopt;
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

std::unordered_map<uint16_t, Team> &GameData::getTeams() { return _teams; }
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

std::vector<std::reference_wrapper<const Player>>
GameData::getPlayersForTeam(uint16_t team_id) const {
  std::vector<std::reference_wrapper<const Player>> players;
  for (const auto &player : _playersVec) {
    if (player.get().getTeamId() == team_id) {
      players.push_back(player);
    }
  }
  return players;
}

bool GameData::removePlayer(uint32_t id) {
  bool erased = _players.erase(id) > 0;
  if (erased && !_playersVec.empty()) {
    _playersVec.erase(
        std::remove_if(_playersVec.begin(), _playersVec.end(),
                       [id](const Player &p) { return p.getId() == id; }),
        _playersVec.end());
  }
  return erased;
}

uint16_t GameData::getManagedTeamId() const { return managed_team_id; }

void GameData::setManagedTeamId(uint16_t id) { managed_team_id = id; }

void from_json(const nlohmann::json &j, RoleFocus &rf) {
  j.at("stats").get_to(rf.stats);
  j.at("weights").get_to(rf.weights);
}

void from_json(const nlohmann::json &j, StatsConfig &sc) {
  j.at("role_focus").get_to(sc.role_focus);
  j.at("possible_stats").get_to(sc.possible_stats);
}

void GameData::loadStatsConfig() {
  std::ifstream f(STATS_CONFIG_PATH);
  if (!f.is_open()) {
    throw std::runtime_error("FATAL: Could not open stats config file");
  }
  nlohmann::json stats_config_json = nlohmann::json::parse(f);
  stats_config = stats_config_json.get<StatsConfig>();
}

const StatsConfig &GameData::getStatsConfig() const { return stats_config; }

bool GameData::isFirstRun() { return is_first_run; }
