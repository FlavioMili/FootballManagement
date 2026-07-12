// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "database/gamedata.h"

#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>

#include "database/database_connection.h"
#include "database/datagenerator.h"
#include "database/repositories/game_state_repository.h"
#include "database/repositories/league_repository.h"
#include "database/repositories/player_repository.h"
#include "database/repositories/team_repository.h"
#include "global/logger.h"
#include "global/paths.h"

GameData& GameData::instance()
{
  static GameData instance;
  return instance;
}

GameData::GameData() = default;

// ---------------- DB ----------------
bool GameData::loadFromDB(std::shared_ptr<DatabaseConnection> database_ptr)
{
  _leagues.clear();
  _teams.clear();
  _players.clear();
  _leaguesVec.clear();
  _teamsVec.clear();
  _playersVec.clear();
  _teamPlayers.clear();

  loadStatsConfig();
  db_conn = database_ptr;

  GameStateRepository gameStateRepo(db_conn);
  TeamRepository teamRepo(db_conn);
  LeagueRepository leagueRepo(db_conn);
  PlayerRepository playerRepo(db_conn);

  bool is_first_run = gameStateRepo.isFirstRun();

  Logger::debug("GameData::loadFromDB called. is_first_run: " +
                std::to_string(is_first_run));

  std::vector<Team> all_teams;
  if (is_first_run)
  {
    db_conn->initialize();
    Logger::debug("Database initialized. Generating data.");

    auto leagues_data = DataGenerator::generateLeagues();
    all_teams = DataGenerator::generateTeams();

    db_conn->beginTransaction();
    for (const auto& team : all_teams)
    {
      teamRepo.insertTeamWithId(team);
      _teams.try_emplace(team.getId(), team);
    }
    _teamsVec.reserve(_teams.size());
    for (auto& p : _teams) _teamsVec.push_back(p.second);

    std::map<uint8_t, std::vector<TeamID>> league_teams_map;
    for (const auto& team : all_teams)
    {
      league_teams_map[team.getLeagueId()].push_back(team.getId());
    }

    for (const auto& league_data : leagues_data)
    {
      leagueRepo.insertLeagueWithId(league_data);
      _leagues.try_emplace(league_data.getId(),
                           League(league_data.getId(), league_data.getName(),
                                  league_teams_map[league_data.getId()]));
    }

    auto players = DataGenerator::generatePlayers();
    for (const auto& player : players)
    {
      playerRepo.insertPlayer(player);
      _players.try_emplace(player.getId(), player);
      _teamPlayers[player.getTeamId()].push_back(player.getId());
    }
    db_conn->commitTransaction();
  }
  else
  {
    auto leagues_from_db = leagueRepo.loadAllLeagues();
    all_teams = teamRepo.loadAllTeams();

    for (const auto& team : all_teams)
    {
      _teams.try_emplace(team.getId(), team);
    }

    std::map<uint8_t, std::vector<TeamID>> league_teams_map;
    for (const auto& team : all_teams)
    {
      league_teams_map[team.getLeagueId()].push_back(team.getId());
    }

    for (auto& league_from_db : leagues_from_db)
    {
      for (TeamID tid : league_teams_map[league_from_db.getId()])
      {
        league_from_db.addTeamID(tid);
      }
      _leagues.try_emplace(league_from_db.getId(), league_from_db);
    }

    auto players = playerRepo.loadAllPlayers();
    for (const auto& player : players)
    {
      _players.try_emplace(player.getId(), player);
      _teamPlayers[player.getTeamId()].push_back(player.getId());
    }
    Logger::debug("Loaded all data from database.");
  }

  // Build vectors from the maps
  _leaguesVec.clear();
  _leaguesVec.reserve(_leagues.size());
  for (auto& p : _leagues) _leaguesVec.push_back(p.second);

  _teamsVec.clear();
  _teamsVec.reserve(_teams.size());
  for (auto& p : _teams) _teamsVec.push_back(p.second);

  _playersVec.clear();
  _playersVec.reserve(_players.size());
  for (auto& p : _players) _playersVec.push_back(p.second);

  return true;
}

bool GameData::saveToDB() const
{
  if (!db_conn) return false;

  db_conn->beginTransaction();
  PlayerRepository playerRepo(db_conn);

  for (const auto& player_ref : _playersVec)
  {
    playerRepo.updatePlayer(player_ref.get());
  }

  db_conn->commitTransaction();
  return true;
}

// ---------------- League ----------------
void GameData::addLeague(LeagueID id, const League& league)
{
  _leagues.try_emplace(id, league);
  if (!_leaguesVec.empty()) _leaguesVec.push_back(_leagues.at(id));
}

std::optional<std::reference_wrapper<const League>> GameData::getLeague(
    LeagueID id) const
{
  if (auto it = _leagues.find(id); it != _leagues.end()) return it->second;
  return std::nullopt;
}

const std::unordered_map<LeagueID, League>& GameData::getLeagues() const
{
  return _leagues;
}

std::unordered_map<LeagueID, League>& GameData::getLeagues()
{
  return _leagues;
}

const std::vector<std::reference_wrapper<const League>>&
GameData::getLeaguesVector() const
{
  return _leaguesVec;
}

// ---------------- Team ----------------
void GameData::addTeam(TeamID id, const Team& team)
{
  _teams.try_emplace(id, team);
  if (!_teamsVec.empty()) _teamsVec.push_back(_teams.at(id));
}

std::optional<std::reference_wrapper<Team>> GameData::getTeam(TeamID id)
{
  if (auto it = _teams.find(id); it != _teams.end()) return it->second;
  return std::nullopt;
}

std::optional<std::reference_wrapper<const Team>> GameData::getTeam(
    TeamID id) const
{
  if (auto it = _teams.find(id); it != _teams.end()) return it->second;
  return std::nullopt;
}

const std::unordered_map<TeamID, Team>& GameData::getTeams() const
{
  return _teams;
}

std::unordered_map<TeamID, Team>& GameData::getTeams() { return _teams; }

const std::vector<std::reference_wrapper<const Team>>&
GameData::getTeamsVector() const
{
  return _teamsVec;
}

// ---------------- Player ----------------
void GameData::addPlayer(PlayerID id, const Player& player)
{
  _players.try_emplace(id, player);
  if (!_playersVec.empty())
  {
    _playersVec.push_back(_players.at(id));
  }
  _teamPlayers[player.getTeamId()].push_back(id);
}

std::optional<std::reference_wrapper<const Player>> GameData::getPlayer(
    PlayerID id) const
{
  if (auto it = _players.find(id); it != _players.end()) return it->second;
  return std::nullopt;
}

const std::unordered_map<PlayerID, Player>& GameData::getPlayers() const
{
  return _players;
}

std::unordered_map<PlayerID, Player>& GameData::getPlayers()
{
  return _players;
}
const std::vector<std::reference_wrapper<const Player>>&
GameData::getPlayersVector() const
{
  return _playersVec;
}

void GameData::ageAllPlayers()
{
  for (auto& pair : _players)
  {
    pair.second.agePlayer();
  }
}

std::vector<std::reference_wrapper<const Player>> GameData::getPlayersForTeam(
    TeamID team_id) const
{
  std::vector<std::reference_wrapper<const Player>> players;
  auto it = _teamPlayers.find(team_id);
  if (it != _teamPlayers.end())
  {
    players.reserve(it->second.size());
    for (PlayerID pid : it->second)
    {
      auto player_it = _players.find(pid);
      if (player_it != _players.end())
      {
        players.push_back(player_it->second);
      }
    }
  }
  return players;
}

bool GameData::removePlayer(PlayerID id)
{
  auto player_it = _players.find(id);
  if (player_it != _players.end())
  {
    TeamID team_id = player_it->second.getTeamId();
    auto& team_players = _teamPlayers[team_id];
    team_players.erase(
        std::remove(team_players.begin(), team_players.end(), id),
        team_players.end());
  }

  bool erased = _players.erase(id) > 0;
  if (erased && !_playersVec.empty())
  {
    _playersVec.erase(
        std::remove_if(_playersVec.begin(), _playersVec.end(),
                       [id](const Player& p) { return p.getId() == id; }),
        _playersVec.end());
  }
  return erased;
}

void GameData::transferPlayer(PlayerID id, TeamID new_team_id)
{
  auto it = _players.find(id);
  if (it == _players.end()) return;

  TeamID old_team_id = it->second.getTeamId();
  if (old_team_id == new_team_id) return;

  it->second.setTeamId(new_team_id);

  auto& old_team_vec = _teamPlayers[old_team_id];
  old_team_vec.erase(std::remove(old_team_vec.begin(), old_team_vec.end(), id),
                     old_team_vec.end());

  _teamPlayers[new_team_id].push_back(id);
}

void from_json(const nlohmann::json& j, RoleFocus& rf)
{
  j.at("stats").get_to(rf.stats);
  j.at("weights").get_to(rf.weights);
}

void from_json(const nlohmann::json& j, StatsConfig& sc)
{
  j.at("role_focus").get_to(sc.role_focus);
  j.at("possible_stats").get_to(sc.possible_stats);
}

void GameData::loadStatsConfig()
{
  std::ifstream f(STATS_CONFIG_PATH);
  if (!f.is_open())
  {
    throw std::runtime_error("FATAL: Could not open stats config file");
  }
  nlohmann::json stats_config_json = nlohmann::json::parse(f);
  stats_config = stats_config_json.get<StatsConfig>();
}

const StatsConfig& GameData::getStatsConfig() const { return stats_config; }
