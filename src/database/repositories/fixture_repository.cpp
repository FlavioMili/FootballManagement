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
  sqlite3_stmt* stmt =
      db_conn->prepareStatement(SQLLoader::getQuery(Query::INSERT_FIXTURE));

  sqlite3_bind_text(stmt, 1, match.getDate().toString().c_str(), -1,
                    SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 2, match.getHomeTeamId());
  sqlite3_bind_int(stmt, 3, match.getAwayTeamId());
  sqlite3_bind_int(stmt, 4, static_cast<int>(match.getMatchType()));

  db_conn->executeStep(stmt);
  sqlite3_finalize(stmt);
}

std::vector<Match> FixtureRepository::loadAllMatches() const
{
  std::vector<Match> matches;
  sqlite3_stmt* stmt = db_conn->prepareStatement(
      "SELECT home_team_id, away_team_id, game_date, match_type FROM "
      "Fixtures;");

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
  sqlite3_stmt* stmt = db_conn->prepareStatement(
      SQLLoader::getQuery(Query::DELETE_ALL_FIXTURES));
  db_conn->executeStep(stmt);
  sqlite3_finalize(stmt);

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
