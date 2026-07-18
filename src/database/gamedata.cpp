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

#include "database/SQLLoader.h"
#include "database/database_connection.h"
#include "database/datagenerator.h"
#include "database/repositories/game_state_repository.h"
#include "database/repositories/league_repository.h"
#include "database/repositories/player_repository.h"
#include "database/repositories/team_repository.h"
#include "global/logger.h"
#include "global/paths.h"
#include "global/queries.h"
#include "model/transfer_listing.h"

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

  bool is_first_run = gameStateRepo.isFirstRun();

  Logger::debug("GameData::loadFromDB called. is_first_run: " +
                std::to_string(is_first_run));

  if (is_first_run)
  {
    generateAndSaveInitialData();
  }
  else
  {
    loadExistingData();
  }

  // Build vectors from the maps
  _leaguesVec.clear();
  _leaguesVec.reserve(_leagues.size());
  for (auto& [id, league] : _leagues) _leaguesVec.push_back(league);

  _teamsVec.clear();
  _teamsVec.reserve(_teams.size());
  for (auto& [id, team] : _teams) _teamsVec.push_back(team);

  _playersVec.clear();
  _playersVec.reserve(_players.size());
  for (auto& [id, player] : _players) _playersVec.push_back(player);

  return true;
}

void GameData::generateAndSaveInitialData()
{
  TeamRepository teamRepo(db_conn);
  LeagueRepository leagueRepo(db_conn);
  PlayerRepository playerRepo(db_conn);

  db_conn->initialize();
  sqlite3_exec(
      db_conn->getRaw(),
      "DELETE FROM Players; DELETE FROM Teams; DELETE FROM Leagues; DELETE "
      "FROM GameState; DELETE FROM LeaguePoints; DELETE FROM Fixtures;",
      nullptr, nullptr, nullptr);
  sqlite3_exec(db_conn->getRaw(),
               "INSERT OR IGNORE INTO Teams (id, league_id, name, balance) "
               "VALUES (0, -1, 'Free agents', -1);",
               nullptr, nullptr, nullptr);
  Logger::debug("Database initialized. Generating data.");

  auto leagues_data = DataGenerator::generateLeagues();
  auto all_teams = DataGenerator::generateTeams();

  db_conn->beginTransaction();
  for (const auto& team : all_teams)
  {
    _teams.try_emplace(team.getId(), team);
  }

  // Populate _teamsVec so we can use insertTeamsWithId
  _teamsVec.clear();
  _teamsVec.reserve(_teams.size());
  for (auto& [id, team] : _teams) _teamsVec.push_back(team);

  teamRepo.insertTeamsWithId(_teamsVec);

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

  // DataGenerator::generatePlayers depends on GameData::getTeamsVector() being
  // populated!
  _teamsVec.clear();
  _teamsVec.reserve(_teams.size());
  for (auto& [id, team] : _teams) _teamsVec.push_back(team);

  auto players = DataGenerator::generatePlayers(*this);
  for (const auto& player : players)
  {
    auto it = _players.try_emplace(player.getId(), player).first;
    _teamPlayers[player.getTeamId()].push_back(it->second);

    // Add to Team's player list
    auto team_it = _teams.find(player.getTeamId());
    if (team_it != _teams.end())
    {
      team_it->second.addPlayerID(player.getId());
    }
  }

  _playersVec.clear();
  _playersVec.reserve(_players.size());
  for (auto& [id, player] : _players) _playersVec.push_back(player);

  playerRepo.insertPlayers(_playersVec);

  for (auto& [id, team] : _teams)
  {
    team.generateStartingXI(*this, stats_config);
  }

  db_conn->commitTransaction();
}

void GameData::loadExistingData()
{
  // Clean up any duplicate players that might have been inserted in previous
  // runs due to initialization bugs
  sqlite3_exec(db_conn->getRaw(),
               "DELETE FROM Players WHERE id NOT IN (SELECT MIN(id) FROM "
               "Players GROUP BY team_id, first_name, last_name);",
               nullptr, nullptr, nullptr);

  TeamRepository teamRepo(db_conn);
  LeagueRepository leagueRepo(db_conn);
  PlayerRepository playerRepo(db_conn);

  auto leagues_from_db = leagueRepo.loadAllLeagues();
  auto all_teams = teamRepo.loadAllTeams();

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
    auto it = _players.try_emplace(player.getId(), player).first;
    _teamPlayers[player.getTeamId()].push_back(it->second);

    // Add to Team's player list
    auto team_it = _teams.find(player.getTeamId());
    if (team_it != _teams.end())
    {
      team_it->second.addPlayerID(player.getId());
    }
  }

  _teamsVec.clear();
  _teamsVec.reserve(_teams.size());
  for (auto& [id, team] : _teams) _teamsVec.push_back(team);

  _playersVec.clear();
  _playersVec.reserve(_players.size());
  for (auto& [id, player] : _players) _playersVec.push_back(player);

  for (auto& [id, team] : _teams)
  {
    team.generateStartingXI(*this, stats_config);
  }

  Logger::debug("Loaded all data from database.");
}

bool GameData::saveToDB() const
{
  if (!db_conn) return false;

  db_conn->beginTransaction();
  PlayerRepository playerRepo(db_conn);

  playerRepo.updatePlayers(_playersVec);

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
  auto it = _players.try_emplace(id, player).first;
  if (!_playersVec.empty())
  {
    _playersVec.push_back(it->second);
  }
  _teamPlayers[player.getTeamId()].push_back(it->second);
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
  for (auto& [id, player] : _players)
  {
    player.agePlayer();
  }
}

const std::vector<std::reference_wrapper<const Player>>&
GameData::getPlayersForTeam(TeamID team_id) const
{
  static const std::vector<std::reference_wrapper<const Player>> empty_vec;
  auto it = _teamPlayers.find(team_id);
  if (it != _teamPlayers.end())
  {
    return it->second;
  }
  return empty_vec;
}

bool GameData::removePlayer(PlayerID id)
{
  auto player_it = _players.find(id);
  if (player_it != _players.end())
  {
    TeamID team_id = player_it->second.getTeamId();
    auto& team_players = _teamPlayers[team_id];
    std::erase_if(team_players,
                  [id](const auto& ref) { return ref.get().getId() == id; });
  }

  bool erased = _players.erase(id) > 0;
  if (erased && !_playersVec.empty())
  {
    auto it = std::ranges::find_if(
        _playersVec, [id](const Player& p) { return p.getId() == id; });
    if (it != _playersVec.end())
    {
      if (it != _playersVec.end() - 1)
      {
        std::swap(*it, _playersVec.back());
      }
      _playersVec.pop_back();
    }
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
  std::erase_if(old_team_vec,
                [id](const auto& ref) { return ref.get().getId() == id; });

  _teamPlayers[new_team_id].push_back(it->second);
}

// ---------------- Transfer Market ----------------
void GameData::saveTransferListing(const TransferListing& listing) const
{
  sqlite3_stmt* stmt = db_conn->prepareStatement(
      SQLLoader::getQuery(Query::UPSERT_TRANSFER_LISTING));

  sqlite3_bind_int(stmt, 1, static_cast<int>(listing.player_id));
  sqlite3_bind_int(stmt, 2, static_cast<int>(listing.asking_price));
  std::string date_str = listing.listing_date.toString();
  sqlite3_bind_text(stmt, 3, date_str.c_str(), -1, SQLITE_TRANSIENT);

  db_conn->executeStep(stmt);
  sqlite3_finalize(stmt);
}

void GameData::deleteTransferListing(PlayerID player_id) const
{
  sqlite3_stmt* stmt = db_conn->prepareStatement(
      SQLLoader::getQuery(Query::DELETE_TRANSFER_LISTING));

  sqlite3_bind_int(stmt, 1, static_cast<int>(player_id));

  db_conn->executeStep(stmt);
  sqlite3_finalize(stmt);
}

std::unordered_map<PlayerID, TransferListing>
GameData::loadAllTransferListings() const
{
  std::unordered_map<PlayerID, TransferListing> listings;

  sqlite3_stmt* stmt = db_conn->prepareStatement(
      SQLLoader::getQuery(Query::LOAD_ALL_TRANSFER_LISTINGS));

  while (sqlite3_step(stmt) == SQLITE_ROW)
  {
    auto pid = static_cast<PlayerID>(sqlite3_column_int(stmt, 0));
    auto price = static_cast<uint32_t>(sqlite3_column_int(stmt, 1));
    auto* date_str =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));

    TransferListing listing;
    listing.player_id = pid;
    listing.asking_price = price;
    listing.listing_date =
        GameDateValue::fromString(date_str ? date_str : "2025-07-01");

    listings[pid] = listing;
  }

  sqlite3_finalize(stmt);
  return listings;
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
