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
  sqlite3_stmt* stmt;
  const std::string& sql = "SELECT id, league_id, name, balance FROM Teams";

  if (sqlite3_prepare_v2(db_conn->getRaw(), sql.c_str(), -1, &stmt, 0) !=
      SQLITE_OK)
  {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db_conn->getRaw())));
  }

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

void TeamRepository::insertTeam(const Team& team) const
{
  sqlite3_stmt* stmt;
  const std::string& sql = SQLLoader::getQuery(Query::INSERT_TEAM);

  if (sqlite3_prepare_v2(db_conn->getRaw(), sql.c_str(), -1, &stmt, 0) !=
      SQLITE_OK)
  {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db_conn->getRaw())));
  }

  sqlite3_bind_int(stmt, 1, team.getLeagueId());
  sqlite3_bind_text(stmt, 2, team.getName().c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 3, team.getFinances().getBalance());
  sqlite3_bind_text(stmt, 4, "{}", -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 5, "{}", -1, SQLITE_TRANSIENT);

  if (sqlite3_step(stmt) != SQLITE_DONE)
  {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute statement: " +
                             std::string(sqlite3_errmsg(db_conn->getRaw())));
  }

  sqlite3_finalize(stmt);
}

void TeamRepository::insertTeamWithId(const Team& team) const
{
  sqlite3_stmt* stmt;
  const std::string& sql = SQLLoader::getQuery(Query::INSERT_TEAM_WITH_ID);

  if (sqlite3_prepare_v2(db_conn->getRaw(), sql.c_str(), -1, &stmt, 0) !=
      SQLITE_OK)
  {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db_conn->getRaw())));
  }

  sqlite3_bind_int(stmt, 1, team.getId());
  sqlite3_bind_int(stmt, 2, team.getLeagueId());
  sqlite3_bind_text(stmt, 3, team.getName().c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 4, team.getFinances().getBalance());
  sqlite3_bind_text(stmt, 5, "{}", -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 6, "{}", -1, SQLITE_TRANSIENT);

  if (sqlite3_step(stmt) != SQLITE_DONE)
  {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute statement: " +
                             std::string(sqlite3_errmsg(db_conn->getRaw())));
  }

  sqlite3_finalize(stmt);
}
