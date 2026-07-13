// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "team_repository.h"

#include <sqlite3.h>

#include <stdexcept>

#include "database/SQLLoader.h"

TeamRepository::TeamRepository(std::shared_ptr<DatabaseConnection> conn)
    : db_conn(conn)
{
}

std::vector<Team> TeamRepository::loadAllTeams() const
{
  std::vector<Team> teams;
  sqlite3_stmt* stmt = db_conn->prepareStatement(
      "SELECT id, league_id, name, balance FROM Teams");

  while (sqlite3_step(stmt) == SQLITE_ROW)
  {
    int id = sqlite3_column_int(stmt, 0);
    int league_id = sqlite3_column_int(stmt, 1);
    const unsigned char* name_text = sqlite3_column_text(stmt, 2);
    std::string name =
        name_text ? reinterpret_cast<const char*>(name_text) : "";
    std::int64_t balance = sqlite3_column_int64(stmt, 3);
    teams.emplace_back(id, league_id, name, balance);
  }

  sqlite3_finalize(stmt);
  return teams;
}

void TeamRepository::bindTeamParams(sqlite3_stmt* stmt, const Team& team,
                                    int startIndex) const
{
  sqlite3_bind_int(stmt, startIndex++, team.getLeagueId());
  sqlite3_bind_text(stmt, startIndex++, team.getName().c_str(), -1,
                    SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, startIndex++, team.getFinances().getBalance());
  sqlite3_bind_text(stmt, startIndex++, "{}", -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, startIndex++, "{}", -1, SQLITE_TRANSIENT);
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
