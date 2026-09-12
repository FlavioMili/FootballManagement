// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "team_repository.h"

#include <sqlite3.h>

#include <algorithm>
#include <nlohmann/json.hpp>
#include <stdexcept>

#include "database/SQLLoader.h"
#include "database/repositories/finance_repository.h"

namespace
{
constexpr int LINEUP_FORMAT_VERSION = 1;

std::string serializeStrategy(const Strategy& strategy)
{
  const StrategySliders sliders = strategy.getSliders();
  return nlohmann::json{{"pressing", sliders.pressing},
                        {"risk_taking", sliders.riskTaking},
                        {"offensive_bias", sliders.offensiveBias},
                        {"width_usage", sliders.widthUsage},
                        {"compactness", sliders.compactness}}
      .dump();
}

Strategy deserializeStrategy(const unsigned char* strategyText)
{
  Strategy strategy;
  if (!strategyText) return strategy;
  const auto value = nlohmann::json::parse(
      reinterpret_cast<const char*>(strategyText), nullptr, false);
  if (!value.is_object()) return strategy;

  const StrategySliders defaults = strategy.getSliders();
  try
  {
    strategy.setAllSliders(
        {value.value("pressing", defaults.pressing),
         value.value("risk_taking", defaults.riskTaking),
         value.value("offensive_bias", defaults.offensiveBias),
         value.value("width_usage", defaults.widthUsage),
         value.value("compactness", defaults.compactness)});
  }
  catch (const nlohmann::json::exception&)
  {
    return Strategy{};
  }
  return strategy;
}

std::string serializeLineup(const Lineup& lineup)
{
  nlohmann::json value;
  value["version"] = LINEUP_FORMAT_VERSION;
  value["goalkeeper"] = lineup.getGoalkeeper()
                            ? nlohmann::json(lineup.getGoalkeeper()->getId())
                            : nlohmann::json(nullptr);
  value["outfield"] = nlohmann::json::array();
  for (const auto& positioned : lineup.getOutfieldPlayers())
  {
    if (!positioned.player) continue;
    value["outfield"].push_back({{"player_id", positioned.player->getId()},
                                 {"x", positioned.position.x},
                                 {"y", positioned.position.y}});
  }
  value["reserves"] = nlohmann::json::array();
  for (const Player* reserve : lineup.getReserves())
  {
    if (reserve) value["reserves"].push_back(reserve->getId());
  }
  // Captain and set-piece takers by SetPieceDuty (0 = automatic); older
  // saves simply lack the key.
  value["set_pieces"] = lineup.getDesignations();
  return value.dump();
}

StoredLineup deserializeLineup(const unsigned char* lineupText)
{
  StoredLineup lineup;
  if (!lineupText) return lineup;
  const auto value = nlohmann::json::parse(
      reinterpret_cast<const char*>(lineupText), nullptr, false);
  if (!value.is_object()) return lineup;

  try
  {
    if (value.value("version", 0) != LINEUP_FORMAT_VERSION) return lineup;
    lineup.persisted = true;
    if (value.contains("goalkeeper") &&
        value["goalkeeper"].is_number_unsigned())
      lineup.goalkeeperId = value["goalkeeper"].get<PlayerID>();
    if (value.contains("outfield") && value["outfield"].is_array())
    {
      for (const auto& positioned : value["outfield"])
      {
        if (!positioned.is_object()) continue;
        lineup.outfield.push_back({positioned.at("player_id").get<PlayerID>(),
                                   {positioned.at("x").get<float>(),
                                    positioned.at("y").get<float>()}});
      }
    }
    if (value.contains("reserves") && value["reserves"].is_array())
    {
      for (const auto& reserve : value["reserves"])
        lineup.reserves.push_back(reserve.get<PlayerID>());
    }
    if (value.contains("set_pieces") && value["set_pieces"].is_array())
    {
      const auto& duties = value["set_pieces"];
      for (size_t duty = 0;
           duty < std::min(duties.size(), lineup.designations.size()); ++duty)
        if (duties[duty].is_number_unsigned())
          lineup.designations[duty] = duties[duty].get<PlayerID>();
    }
  }
  catch (const nlohmann::json::exception&)
  {
    return StoredLineup{};
  }
  return lineup;
}
}  // namespace

TeamRepository::TeamRepository(std::shared_ptr<DatabaseConnection> conn)
    : db_conn(conn)
{
}

std::vector<Team> TeamRepository::loadAllTeams() const
{
  std::vector<Team> teams;
  sqlite3_stmt* stmt = db_conn->prepareStatement(
      "SELECT id, league_id, name, balance, strategy, reputation, "
      "stadium_capacity, ticket_price, training_facilities, youth_facilities, "
      "transfer_budget, wage_budget, recent_form FROM Teams");

  while (sqlite3_step(stmt) == SQLITE_ROW)
  {
    int id = sqlite3_column_int(stmt, 0);
    int league_id = sqlite3_column_int(stmt, 1);
    const unsigned char* name_text = sqlite3_column_text(stmt, 2);
    std::string name =
        name_text ? reinterpret_cast<const char*>(name_text) : "";
    std::int64_t balance = sqlite3_column_int64(stmt, 3);
    Strategy strategy = deserializeStrategy(sqlite3_column_text(stmt, 4));
    Team& team = teams.emplace_back(id, league_id, name, balance,
                                    std::vector<PlayerID>{}, strategy);
    // Legacy saves have zeros here; GameData migrates those clubs.
    ClubProfile profile;
    profile.reputation = static_cast<std::uint8_t>(sqlite3_column_int(stmt, 5));
    profile.stadium_capacity =
        static_cast<std::uint32_t>(sqlite3_column_int64(stmt, 6));
    profile.ticket_price =
        static_cast<std::uint32_t>(sqlite3_column_int64(stmt, 7));
    profile.training_facilities =
        static_cast<std::uint8_t>(sqlite3_column_int(stmt, 8));
    profile.youth_facilities =
        static_cast<std::uint8_t>(sqlite3_column_int(stmt, 9));
    team.setProfile(profile);
    team.getFinances().setTransferBudget(sqlite3_column_int64(stmt, 10));
    team.getFinances().setWageBudget(sqlite3_column_int64(stmt, 11));
    const unsigned char* form_text = sqlite3_column_text(stmt, 12);
    if (form_text) team.setRecentForm(reinterpret_cast<const char*>(form_text));
  }

  sqlite3_finalize(stmt);
  return teams;
}

std::unordered_map<TeamID, StoredLineup> TeamRepository::loadAllLineups() const
{
  std::unordered_map<TeamID, StoredLineup> lineups;
  sqlite3_stmt* stmt =
      db_conn->prepareStatement("SELECT id, lineup FROM Teams");
  while (sqlite3_step(stmt) == SQLITE_ROW)
  {
    const auto teamId = static_cast<TeamID>(sqlite3_column_int(stmt, 0));
    lineups.emplace(teamId, deserializeLineup(sqlite3_column_text(stmt, 1)));
  }
  sqlite3_finalize(stmt);
  return lineups;
}

void TeamRepository::bindTeamParams(sqlite3_stmt* stmt, const Team& team,
                                    int startIndex) const
{
  sqlite3_bind_int(stmt, startIndex++, team.getLeagueId());
  sqlite3_bind_text(stmt, startIndex++, team.getName().c_str(), -1,
                    SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, startIndex++, team.getFinances().getBalance());
  const std::string strategy = serializeStrategy(team.getStrategy());
  sqlite3_bind_text(stmt, startIndex++, strategy.c_str(), -1, SQLITE_TRANSIENT);
  const std::string lineup = serializeLineup(team.getLineup());
  sqlite3_bind_text(stmt, startIndex++, lineup.c_str(), -1, SQLITE_TRANSIENT);
}

void TeamRepository::insertTeam(const Team& team) const
{
  sqlite3_stmt* stmt =
      db_conn->prepareStatement(SQLLoader::getQuery(Query::INSERT_TEAM));

  bindTeamParams(stmt, team, 1);

  db_conn->executeStep(stmt);
  sqlite3_finalize(stmt);
}

void TeamRepository::insertTeamWithId(const Team& team) const
{
  sqlite3_stmt* stmt = db_conn->prepareStatement(
      SQLLoader::getQuery(Query::INSERT_TEAM_WITH_ID));

  sqlite3_bind_int(stmt, 1, team.getId());
  bindTeamParams(stmt, team, 2);

  db_conn->executeStep(stmt);
  sqlite3_finalize(stmt);
}

void TeamRepository::insertTeamsWithId(
    const std::vector<std::reference_wrapper<const Team>>& teams) const
{
  sqlite3_stmt* stmt = db_conn->prepareStatement(
      SQLLoader::getQuery(Query::INSERT_TEAM_WITH_ID));

  for (const auto& team_ref : teams)
  {
    const Team& team = team_ref.get();
    sqlite3_bind_int(stmt, 1, team.getId());
    bindTeamParams(stmt, team, 2);

    db_conn->executeStep(stmt);
    sqlite3_clear_bindings(stmt);
    sqlite3_reset(stmt);
  }

  sqlite3_finalize(stmt);
}

namespace
{
constexpr const char* UPDATE_TEAM_STATE_SQL =
    "UPDATE Teams SET balance = ?, strategy = ?, lineup = ?, reputation = ?, "
    "stadium_capacity = ?, ticket_price = ?, training_facilities = ?, "
    "youth_facilities = ?, transfer_budget = ?, wage_budget = ?, "
    "recent_form = ? WHERE id = ?;";
}  // namespace

void TeamRepository::updateTeamState(const Team& team) const
{
  updateTeamsState({std::cref(team)});
}

void TeamRepository::updateTeamsState(
    const std::vector<std::reference_wrapper<const Team>>& teams) const
{
  sqlite3_stmt* stmt = db_conn->prepareStatement(UPDATE_TEAM_STATE_SQL);
  for (const auto& team : teams)
  {
    bindTeamStateParams(stmt, team.get());
    db_conn->executeStep(stmt);
    sqlite3_clear_bindings(stmt);
    sqlite3_reset(stmt);
  }
  sqlite3_finalize(stmt);
  // The balance above and the ledger rows stay consistent in one transaction.
  FinanceRepository(db_conn).insertPending(teams);
}

void TeamRepository::bindTeamStateParams(sqlite3_stmt* stmt,
                                         const Team& team) const
{
  const Finances& finances = team.getFinances();
  const ClubProfile& profile = team.getProfile();
  sqlite3_bind_int64(stmt, 1, finances.getBalance());
  const std::string strategy = serializeStrategy(team.getStrategy());
  sqlite3_bind_text(stmt, 2, strategy.c_str(), -1, SQLITE_TRANSIENT);
  const std::string lineup = serializeLineup(team.getLineup());
  sqlite3_bind_text(stmt, 3, lineup.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 4, profile.reputation);
  sqlite3_bind_int64(stmt, 5, profile.stadium_capacity);
  sqlite3_bind_int64(stmt, 6, profile.ticket_price);
  sqlite3_bind_int(stmt, 7, profile.training_facilities);
  sqlite3_bind_int(stmt, 8, profile.youth_facilities);
  sqlite3_bind_int64(stmt, 9, finances.getTransferBudget());
  sqlite3_bind_int64(stmt, 10, finances.getWageBudget());
  sqlite3_bind_text(stmt, 11, team.getRecentForm().c_str(), -1,
                    SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 12, team.getId());
}
