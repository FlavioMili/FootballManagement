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
#include <limits>
#include <nlohmann/json.hpp>
#include <utility>

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

namespace
{
bool restoreLineup(Team& team, const StoredLineup& stored,
                   const std::unordered_map<PlayerID, Player>& players)
{
  if (!stored.persisted) return false;
  Lineup& lineup = team.getLineup();
  lineup.clear();

  const auto resolvePlayer = [&](PlayerID playerId) -> const Player*
  {
    const auto found = players.find(playerId);
    if (found == players.end() || found->second.getTeamId() != team.getId())
      return nullptr;
    return &found->second;
  };

  if (stored.goalkeeperId)
  {
    const Player* goalkeeper = resolvePlayer(*stored.goalkeeperId);
    if (!goalkeeper) return false;
    lineup.setGoalkeeper(goalkeeper);
  }
  for (const StoredPositionedPlayer& positioned : stored.outfield)
  {
    const Player* player = resolvePlayer(positioned.playerId);
    if (!player) return false;
    lineup.addOutfieldPlayer(player, positioned.position);
  }
  std::vector<const Player*> reserves;
  reserves.reserve(stored.reserves.size());
  for (PlayerID playerId : stored.reserves)
  {
    const Player* player = resolvePlayer(playerId);
    if (!player) return false;
    reserves.push_back(player);
  }
  lineup.setReserves(reserves);
  return true;
}
}  // namespace

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
  for (auto& [id, team] : _teams)
  {
    if (id != FREE_AGENTS_TEAM_ID) _teamsVec.push_back(team);
  }

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
  try
  {
    _teams.try_emplace(FREE_AGENTS_TEAM_ID, FREE_AGENTS_TEAM_ID,
                       std::numeric_limits<LeagueID>::max(), "Free agents", -1);
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

    // DataGenerator::generatePlayers depends on GameData::getTeamsVector()
    // being populated!
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
    teamRepo.updateTeamsState(_teamsVec);

    db_conn->commitTransaction();
  }
  catch (...)
  {
    db_conn->rollbackTransaction();
    throw;
  }
}

void GameData::loadExistingData()
{
  TeamRepository teamRepo(db_conn);
  LeagueRepository leagueRepo(db_conn);
  PlayerRepository playerRepo(db_conn);

  auto leagues_from_db = leagueRepo.loadAllLeagues();
  auto all_teams = teamRepo.loadAllTeams();
  const auto storedLineups = teamRepo.loadAllLineups();

  std::map<uint8_t, std::vector<TeamID>> league_teams_map;
  for (const auto& team : all_teams)
  {
    league_teams_map[team.getLeagueId()].push_back(team.getId());
  }

  _teams.reserve(all_teams.size());
  _teamPlayers.reserve(all_teams.size());
  for (auto& team : all_teams)
  {
    const TeamID teamId = team.getId();
    _teams.try_emplace(teamId, std::move(team));
  }

  for (auto& league_from_db : leagues_from_db)
  {
    for (TeamID tid : league_teams_map[league_from_db.getId()])
    {
      league_from_db.addTeamID(tid);
    }
    const LeagueID leagueId = league_from_db.getId();
    _leagues.try_emplace(leagueId, std::move(league_from_db));
  }

  auto players = playerRepo.loadAllPlayers();
  _players.reserve(players.size());
  for (auto& player : players)
  {
    const PlayerID playerId = player.getId();
    const TeamID teamId = player.getTeamId();
    auto it = _players.try_emplace(playerId, std::move(player)).first;
    _teamPlayers[teamId].push_back(it->second);

    // Add to Team's player list
    auto team_it = _teams.find(teamId);
    if (team_it != _teams.end())
    {
      team_it->second.addPlayerID(playerId);
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
    const auto stored = storedLineups.find(id);
    if (stored == storedLineups.end() ||
        !restoreLineup(team, stored->second, _players))
    {
      team.generateStartingXI(*this, stats_config);
    }
  }

  Logger::debug("Loaded all data from database.");
}

bool GameData::saveToDB() const
{
  if (!db_conn) return false;

  db_conn->beginTransaction();
  try
  {
    PlayerRepository playerRepo(db_conn);
    TeamRepository teamRepo(db_conn);
    playerRepo.updatePlayers(_playersVec);
    teamRepo.updateTeamsState(_teamsVec);
    db_conn->commitTransaction();
    return true;
  }
  catch (const std::exception& exception)
  {
    db_conn->rollbackTransaction();
    Logger::error("Failed to save game data: " + std::string(exception.what()));
    return false;
  }
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

std::vector<PlayerID> GameData::advanceContractsAndReleasePlayers()
{
  std::vector<std::pair<PlayerID, TeamID>> expiringPlayers;
  expiringPlayers.reserve(_players.size());
  for (auto& [id, player] : _players)
  {
    if (player.getTeamId() != FREE_AGENTS_TEAM_ID &&
        player.advanceContractYear())
    {
      expiringPlayers.emplace_back(id, player.getTeamId());
    }
  }

  std::vector<PlayerID> releasedPlayers;
  releasedPlayers.reserve(expiringPlayers.size());
  std::vector<TeamID> affectedTeams;
  affectedTeams.reserve(expiringPlayers.size());
  for (const auto& [playerId, oldTeamId] : expiringPlayers)
  {
    if (auto team = getTeam(oldTeamId)) team->get().removePlayerID(playerId);
    if (auto freeAgents = getTeam(FREE_AGENTS_TEAM_ID))
      freeAgents->get().addPlayerID(playerId);
    transferPlayer(playerId, FREE_AGENTS_TEAM_ID);
    _players.at(playerId).setTransferStatus(TransferStatus::NotListed);
    releasedPlayers.push_back(playerId);
    if (!std::ranges::contains(affectedTeams, oldTeamId))
      affectedTeams.push_back(oldTeamId);
  }

  for (TeamID teamId : affectedTeams)
  {
    if (auto team = getTeam(teamId))
      team->get().generateStartingXI(*this, stats_config);
  }
  return releasedPlayers;
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
  if (player_it == _players.end())
  {
    return false;
  }

  const TeamID team_id = player_it->second.getTeamId();
  auto& team_players = _teamPlayers[team_id];
  std::erase_if(team_players,
                [id](const auto& ref) { return ref.get().getId() == id; });
  std::erase_if(_playersVec,
                [id](const auto& ref) { return ref.get().getId() == id; });
  _players.erase(player_it);
  return true;
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
  if (listing.highest_bidder_id)
  {
    sqlite3_bind_int(stmt, 4, static_cast<int>(*listing.highest_bidder_id));
  }
  else
  {
    sqlite3_bind_null(stmt, 4);
  }
  sqlite3_bind_int64(stmt, 5, static_cast<sqlite3_int64>(listing.highest_bid));

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

  // Forward-compatible migration for save files created before bids were
  // persisted. Duplicate-column errors are intentionally ignored.
  sqlite3_exec(db_conn->getRaw(),
               "ALTER TABLE TransferList ADD COLUMN highest_bidder_id "
               "INTEGER;",
               nullptr, nullptr, nullptr);
  sqlite3_exec(db_conn->getRaw(),
               "ALTER TABLE TransferList ADD COLUMN highest_bid INTEGER NOT "
               "NULL DEFAULT 0;",
               nullptr, nullptr, nullptr);

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
    if (sqlite3_column_type(stmt, 3) != SQLITE_NULL)
    {
      listing.highest_bidder_id =
          static_cast<TeamID>(sqlite3_column_int(stmt, 3));
    }
    listing.highest_bid = static_cast<uint32_t>(sqlite3_column_int64(stmt, 4));

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
