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
#include <utility>

#include "database/SQLLoader.h"
#include "global/logger.h"
#include "model/match_report.h"

namespace
{
constexpr const char* INSERT_FIXTURE_SQL =
    "INSERT OR REPLACE INTO Fixtures (game_date, home_team_id, away_team_id, "
    "match_type, home_goals, away_goals, played, competition_id, stage, "
    "extra_time, home_penalties, away_penalties) "
    "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

constexpr const char* REPORT_COLUMNS =
    "game_date, home_team_id, away_team_id, season, match_type, "
    "competition_id, stage, home_goals, away_goals, extra_time, "
    "home_penalties, away_penalties, attendance, stats, events, players";

std::string columnText(sqlite3_stmt* stmt, int column)
{
  const unsigned char* text = sqlite3_column_text(stmt, column);
  return text ? reinterpret_cast<const char*>(text) : std::string();
}

void bindFixture(sqlite3_stmt* stmt, const Match& match)
{
  sqlite3_bind_text(stmt, 1, match.getDate().toString().c_str(), -1,
                    SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 2, match.getHomeTeamId());
  sqlite3_bind_int(stmt, 3, match.getAwayTeamId());
  sqlite3_bind_int(stmt, 4, std::to_underlying(match.getMatchType()));
  sqlite3_bind_int(stmt, 5, match.getHomeScore());
  sqlite3_bind_int(stmt, 6, match.getAwayScore());
  sqlite3_bind_int(stmt, 7, match.isPlayed() ? 1 : 0);
  sqlite3_bind_int(stmt, 8, match.getCompetitionId());
  sqlite3_bind_int(stmt, 9, match.getStage());
  sqlite3_bind_int(stmt, 10, match.wentToExtraTime() ? 1 : 0);
  if (match.wentToPenalties())
  {
    sqlite3_bind_int(stmt, 11, match.getHomePenalties());
    sqlite3_bind_int(stmt, 12, match.getAwayPenalties());
  }
  else
  {
    sqlite3_bind_null(stmt, 11);
    sqlite3_bind_null(stmt, 12);
  }
}

MatchReport readReport(sqlite3_stmt* stmt)
{
  MatchReport report;
  report.date = GameDateValue::fromString(columnText(stmt, 0));
  report.home_team_id = static_cast<TeamID>(sqlite3_column_int(stmt, 1));
  report.away_team_id = static_cast<TeamID>(sqlite3_column_int(stmt, 2));
  report.season = static_cast<uint16_t>(sqlite3_column_int(stmt, 3));
  report.match_type = static_cast<MatchType>(sqlite3_column_int(stmt, 4));
  report.competition_id = static_cast<LeagueID>(sqlite3_column_int(stmt, 5));
  report.stage = static_cast<uint8_t>(sqlite3_column_int(stmt, 6));
  report.home_goals = static_cast<uint8_t>(sqlite3_column_int(stmt, 7));
  report.away_goals = static_cast<uint8_t>(sqlite3_column_int(stmt, 8));
  report.extra_time = sqlite3_column_int(stmt, 9) != 0;
  report.penalties = sqlite3_column_type(stmt, 10) != SQLITE_NULL;
  report.home_penalties = static_cast<uint8_t>(sqlite3_column_int(stmt, 10));
  report.away_penalties = static_cast<uint8_t>(sqlite3_column_int(stmt, 11));
  report.attendance = static_cast<uint32_t>(sqlite3_column_int64(stmt, 12));
  report.statsFromJson(columnText(stmt, 13));
  report.eventsFromJson(columnText(stmt, 14));
  report.playersFromJson(columnText(stmt, 15));
  return report;
}
}  // namespace

FixtureRepository::FixtureRepository(std::shared_ptr<DatabaseConnection> conn)
    : db_conn(conn)
{
}

void FixtureRepository::ensureSchema() const
{
  // Forward-compatible migration for saves created before competitions were
  // stored. Duplicate-column errors are intentionally ignored.
  static constexpr const char* MIGRATIONS[] = {
      "ALTER TABLE Fixtures ADD COLUMN competition_id INTEGER NOT NULL "
      "DEFAULT 0;",
      "ALTER TABLE Fixtures ADD COLUMN stage INTEGER NOT NULL DEFAULT 0;",
      "ALTER TABLE Fixtures ADD COLUMN extra_time INTEGER NOT NULL DEFAULT 0;",
      "ALTER TABLE Fixtures ADD COLUMN home_penalties INTEGER;",
      "ALTER TABLE Fixtures ADD COLUMN away_penalties INTEGER;"};
  for (const char* migration : MIGRATIONS)
    sqlite3_exec(db_conn->getRaw(), migration, nullptr, nullptr, nullptr);

  // Same definition as assets/db/schema.sql, for saves that predate it.
  sqlite3_exec(
      db_conn->getRaw(),
      "CREATE TABLE IF NOT EXISTS MatchReports (game_date TEXT NOT NULL, "
      "home_team_id INTEGER NOT NULL, away_team_id INTEGER NOT NULL, season "
      "INTEGER NOT NULL, match_type INTEGER NOT NULL DEFAULT 0, "
      "competition_id INTEGER NOT NULL DEFAULT 0, stage INTEGER NOT NULL "
      "DEFAULT 0, home_goals INTEGER NOT NULL DEFAULT 0, away_goals INTEGER "
      "NOT NULL DEFAULT 0, extra_time INTEGER NOT NULL DEFAULT 0, "
      "home_penalties INTEGER, away_penalties INTEGER, attendance INTEGER NOT "
      "NULL DEFAULT 0, stats TEXT NOT NULL DEFAULT '{}', events TEXT NOT NULL "
      "DEFAULT '[]', players TEXT NOT NULL DEFAULT '[]', PRIMARY "
      "KEY(game_date, home_team_id, away_team_id));"
      "CREATE INDEX IF NOT EXISTS idx_match_reports_season ON "
      "MatchReports(season);",
      nullptr, nullptr, nullptr);
}

void FixtureRepository::insertFixture(const Match& match) const
{
  sqlite3_stmt* stmt = db_conn->prepareStatement(INSERT_FIXTURE_SQL);
  bindFixture(stmt, match);
  db_conn->executeStep(stmt);
  sqlite3_finalize(stmt);
}

std::vector<Match> FixtureRepository::loadAllMatches() const
{
  ensureSchema();
  std::vector<Match> matches;
  sqlite3_stmt* stmt = db_conn->prepareStatement(
      "SELECT home_team_id, away_team_id, game_date, match_type, home_goals, "
      "away_goals, played, competition_id, stage, extra_time, home_penalties, "
      "away_penalties FROM Fixtures;");

  while (sqlite3_step(stmt) == SQLITE_ROW)
  {
    uint16_t home_id = static_cast<uint16_t>(sqlite3_column_int(stmt, 0));
    uint16_t away_id = static_cast<uint16_t>(sqlite3_column_int(stmt, 1));
    std::string date_str = columnText(stmt, 2);
    auto match_type = static_cast<MatchType>(sqlite3_column_int(stmt, 3));

    Match match(home_id, away_id, GameDateValue::fromString(date_str),
                match_type, static_cast<LeagueID>(sqlite3_column_int(stmt, 7)),
                static_cast<uint8_t>(sqlite3_column_int(stmt, 8)));
    if (sqlite3_column_int(stmt, 6) != 0)
    {
      std::optional<std::pair<uint8_t, uint8_t>> penalties;
      if (sqlite3_column_type(stmt, 10) != SQLITE_NULL)
        penalties.emplace(static_cast<uint8_t>(sqlite3_column_int(stmt, 10)),
                          static_cast<uint8_t>(sqlite3_column_int(stmt, 11)));
      match.setKnockoutResult(
          static_cast<uint8_t>(sqlite3_column_int(stmt, 4)),
          static_cast<uint8_t>(sqlite3_column_int(stmt, 5)),
          sqlite3_column_int(stmt, 9) != 0, penalties);
    }
    matches.push_back(std::move(match));
  }

  sqlite3_finalize(stmt);
  return matches;
}

void FixtureRepository::saveCalendar(const Calendar& calendar) const
{
  sqlite3_stmt* stmt_delete = db_conn->prepareStatement(
      SQLLoader::getQuery(Query::DELETE_ALL_FIXTURES));
  db_conn->executeStep(stmt_delete);
  sqlite3_finalize(stmt_delete);

  sqlite3_stmt* stmt = db_conn->prepareStatement(INSERT_FIXTURE_SQL);
  for (const auto& [matchDay, matches] : calendar.getFullCalendar())
  {
    for (const auto& match : matches)
    {
      bindFixture(stmt, match);
      db_conn->executeStep(stmt);
      sqlite3_clear_bindings(stmt);
      sqlite3_reset(stmt);
    }
  }
  sqlite3_finalize(stmt);
}

void FixtureRepository::loadCalendar(Calendar& calendar) const
{
  auto matches = loadAllMatches();
  for (const auto& match : matches)
  {
    calendar.addMatch(match);
  }
}

void FixtureRepository::saveMatchReports(
    const std::vector<MatchReport>& reports) const
{
  if (reports.empty()) return;
  sqlite3_stmt* stmt = db_conn->prepareStatement(
      std::string("INSERT OR REPLACE INTO MatchReports (") + REPORT_COLUMNS +
      ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);");
  for (const MatchReport& report : reports)
  {
    sqlite3_bind_text(stmt, 1, report.date.toString().c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, report.home_team_id);
    sqlite3_bind_int(stmt, 3, report.away_team_id);
    sqlite3_bind_int(stmt, 4, report.season);
    sqlite3_bind_int(stmt, 5, std::to_underlying(report.match_type));
    sqlite3_bind_int(stmt, 6, report.competition_id);
    sqlite3_bind_int(stmt, 7, report.stage);
    sqlite3_bind_int(stmt, 8, report.home_goals);
    sqlite3_bind_int(stmt, 9, report.away_goals);
    sqlite3_bind_int(stmt, 10, report.extra_time ? 1 : 0);
    if (report.penalties)
    {
      sqlite3_bind_int(stmt, 11, report.home_penalties);
      sqlite3_bind_int(stmt, 12, report.away_penalties);
    }
    else
    {
      sqlite3_bind_null(stmt, 11);
      sqlite3_bind_null(stmt, 12);
    }
    sqlite3_bind_int64(stmt, 13, report.attendance);
    sqlite3_bind_text(stmt, 14, report.statsToJson().c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 15, report.eventsToJson().c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 16, report.playersToJson().c_str(), -1,
                      SQLITE_TRANSIENT);
    db_conn->executeStep(stmt);
    sqlite3_clear_bindings(stmt);
    sqlite3_reset(stmt);
  }
  sqlite3_finalize(stmt);
}

std::optional<MatchReport> FixtureRepository::loadMatchReport(
    const GameDateValue& date, TeamID home_id, TeamID away_id) const
{
  sqlite3_stmt* stmt = db_conn->prepareStatement(
      std::string("SELECT ") + REPORT_COLUMNS +
      " FROM MatchReports WHERE game_date = ? AND home_team_id = ? AND "
      "away_team_id = ?;");
  sqlite3_bind_text(stmt, 1, date.toString().c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 2, home_id);
  sqlite3_bind_int(stmt, 3, away_id);
  std::optional<MatchReport> report;
  if (sqlite3_step(stmt) == SQLITE_ROW) report = readReport(stmt);
  sqlite3_finalize(stmt);
  return report;
}

std::vector<MatchReport> FixtureRepository::loadTeamMatchReports(
    uint16_t season, TeamID team_id) const
{
  sqlite3_stmt* stmt = db_conn->prepareStatement(
      std::string("SELECT ") + REPORT_COLUMNS +
      " FROM MatchReports WHERE season = ? AND (home_team_id = ? OR "
      "away_team_id = ?) ORDER BY game_date;");
  sqlite3_bind_int(stmt, 1, season);
  sqlite3_bind_int(stmt, 2, team_id);
  sqlite3_bind_int(stmt, 3, team_id);
  std::vector<MatchReport> reports;
  while (sqlite3_step(stmt) == SQLITE_ROW) reports.push_back(readReport(stmt));
  sqlite3_finalize(stmt);
  return reports;
}
