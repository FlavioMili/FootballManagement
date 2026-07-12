// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "fixture_repository.h"

#include <sqlite3.h>

#include <stdexcept>

#include "database/SQLLoader.h"
#include "global/logger.h"

FixtureRepository::FixtureRepository(std::shared_ptr<DatabaseConnection> conn)
    : db_conn(conn)
{
}

void FixtureRepository::insertFixture(const Match& match) const
{
  const std::string& sql = SQLLoader::getQuery(Query::INSERT_FIXTURE);
  sqlite3_stmt* stmt;
  if (sqlite3_prepare_v2(db_conn->getRaw(), sql.c_str(), -1, &stmt, nullptr) ==
      SQLITE_OK)
  {
    sqlite3_bind_text(stmt, 1, match.getDate().toString().c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, match.getHomeTeamId());
    sqlite3_bind_int(stmt, 3, match.getAwayTeamId());

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
      sqlite3_finalize(stmt);
      throw std::runtime_error("Failed to execute statement: " +
                               std::string(sqlite3_errmsg(db_conn->getRaw())));
    }

    sqlite3_finalize(stmt);
  }
  else
  {
    Logger::error("Failed to prepare insertFixture statement: " +
                  std::string(sqlite3_errmsg(db_conn->getRaw())));
  }
}

std::vector<Match> FixtureRepository::loadAllMatches() const
{
  std::vector<Match> matches;
  std::string sql =
      "SELECT home_team_id, away_team_id, game_date, match_type FROM Fixtures;";
  sqlite3_stmt* stmt;

  if (sqlite3_prepare_v2(db_conn->getRaw(), sql.c_str(), -1, &stmt, nullptr) !=
      SQLITE_OK)
  {
    Logger::error("Failed to prepare statement: " +
                  std::string(sqlite3_errmsg(db_conn->getRaw())));
    return matches;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW)
  {
    uint16_t home_id = static_cast<uint16_t>(sqlite3_column_int(stmt, 0));
    uint16_t away_id = static_cast<uint16_t>(sqlite3_column_int(stmt, 1));
    std::string date_str =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    auto match_type = static_cast<MatchType>(sqlite3_column_int(stmt, 3));

    matches.emplace_back(home_id, away_id, GameDateValue::fromString(date_str),
                         match_type);
  }

  sqlite3_finalize(stmt);
  return matches;
}

void FixtureRepository::saveCalendar(const Calendar& calendar) const
{
  for (const auto& pair : calendar.getFullCalendar())
  {
    for (const auto& match : pair.second)
    {
      insertFixture(match);
    }
  }
}

void FixtureRepository::loadCalendar(Calendar& calendar) const
{
  auto matches = loadAllMatches();
  for (const auto& match : matches)
  {
    calendar.addMatch(match);
  }
}
