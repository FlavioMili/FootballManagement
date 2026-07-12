// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "league_repository.h"

#include <sqlite3.h>

#include <stdexcept>

#include "database/SQLLoader.h"

LeagueRepository::LeagueRepository(std::shared_ptr<DatabaseConnection> conn)
    : db_conn(conn)
{
}

std::vector<League> LeagueRepository::loadAllLeagues() const
{
  sqlite3_stmt* stmt = db_conn->prepareStatement(SQLLoader::getQuery(Query::SELECT_LEAGUES));
  std::vector<League> leagues;

  while (sqlite3_step(stmt) == SQLITE_ROW)
  {
    int id = sqlite3_column_int(stmt, 0);
    const unsigned char* name_text = sqlite3_column_text(stmt, 1);
    std::string name =
        name_text ? reinterpret_cast<const char*>(name_text) : "";
    leagues.emplace_back(id, name);
  }

  sqlite3_finalize(stmt);

  for (auto& league : leagues)
  {
    loadTeamsForLeague(league);
    loadLeaguePoints(league);
  }

  return leagues;
}

void LeagueRepository::loadTeamsForLeague(League& league) const
{
  sqlite3_stmt* stmt = db_conn->prepareStatement("SELECT id FROM Teams WHERE league_id = ?");

  sqlite3_bind_int(stmt, 1, league.getId());

  while (sqlite3_step(stmt) == SQLITE_ROW)
  {
    int team_id = sqlite3_column_int(stmt, 0);
    league.addTeamID(static_cast<uint16_t>(team_id));
  }

  sqlite3_finalize(stmt);
}

void LeagueRepository::insertLeague(const League& league) const
{
  sqlite3_stmt* stmt = db_conn->prepareStatement(SQLLoader::getQuery(Query::INSERT_LEAGUE));

  sqlite3_bind_text(stmt, 1, league.getName().data(), -1, SQLITE_TRANSIENT);

  db_conn->executeStep(stmt);
  sqlite3_finalize(stmt);
}

void LeagueRepository::insertLeagueWithId(const League& league) const
{
  sqlite3_stmt* stmt = db_conn->prepareStatement(SQLLoader::getQuery(Query::INSERT_LEAGUE_WITH_ID));

  sqlite3_bind_int(stmt, 1, league.getId());
  sqlite3_bind_text(stmt, 2, league.getName().c_str(), -1, SQLITE_TRANSIENT);

  if (league.getParentLeagueID())
  {
    uint8_t parentID = *league.getParentLeagueID();
    sqlite3_bind_int(stmt, 3, parentID);
  }

  db_conn->executeStep(stmt);
  sqlite3_finalize(stmt);
}

void LeagueRepository::saveLeaguePoints(const League& league) const
{
  sqlite3_stmt* stmt = db_conn->prepareStatement(SQLLoader::getQuery(Query::UPSERT_LEAGUE_POINTS));

  for (const auto& pair : league.getLeaderboard())
  {
    sqlite3_bind_int(stmt, 1, league.getId());
    sqlite3_bind_int(stmt, 2, pair.first);
    sqlite3_bind_int(stmt, 3, pair.second);

    db_conn->executeStep(stmt);

    sqlite3_clear_bindings(stmt);
    sqlite3_reset(stmt);
  }

  sqlite3_finalize(stmt);
}

void LeagueRepository::loadLeaguePoints(League& league) const
{
  sqlite3_stmt* stmt = db_conn->prepareStatement(SQLLoader::getQuery(Query::SELECT_LEAGUE_POINTS));

  sqlite3_bind_int(stmt, 1, league.getId());

  while (sqlite3_step(stmt) == SQLITE_ROW)
  {
    TeamID team_id = static_cast<uint16_t>(sqlite3_column_int(stmt, 0));
    uint8_t points = static_cast<uint8_t>(sqlite3_column_int(stmt, 1));
    league.setPoints(team_id, points);
  }

  sqlite3_finalize(stmt);
}

void LeagueRepository::resetAllLeaguePoints() const
{
  sqlite3_stmt* stmt = db_conn->prepareStatement(SQLLoader::getQuery(Query::RESET_ALL_LEAGUE_POINTS));

  db_conn->executeStep(stmt);
  sqlite3_finalize(stmt);
}
