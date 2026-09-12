// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "database/database_connection.h"
#include "model/calendar.h"
#include "model/match.h"

struct MatchReport;

/**
 * @class FixtureRepository
 * @brief Repository class for managing Match and Calendar entities in the
 * database, plus the per-fixture match reports.
 */
class FixtureRepository
{
 public:
  /**
   * @brief Construct a new Fixture Repository object.
   * @param db_conn Shared pointer to the database connection.
   */
  explicit FixtureRepository(std::shared_ptr<DatabaseConnection> db_conn);

  /**
   * @brief Load all matches from the database.
   * @return A vector of Match objects.
   */
  std::vector<Match> loadAllMatches() const;

  /**
   * @brief Insert a match fixture into the database.
   * @param match The Match object to insert.
   */
  void insertFixture(const Match& match) const;

  /**
   * @brief Save the calendar to the database.
   * @param calendar The Calendar object to save.
   */
  void saveCalendar(const Calendar& calendar) const;

  /**
   * @brief Load the calendar from the database.
   * @param calendar The Calendar object to populate.
   */
  void loadCalendar(Calendar& calendar) const;

  /** @brief Inserts or replaces match reports (keyed by date and teams). */
  void saveMatchReports(const std::vector<MatchReport>& reports) const;

  /** @brief Loads the report of one fixture, if it was stored. */
  std::optional<MatchReport> loadMatchReport(const GameDateValue& date,
                                             TeamID home_id,
                                             TeamID away_id) const;

  /** @brief Loads all stored reports of a team in a season, by date. */
  std::vector<MatchReport> loadTeamMatchReports(uint16_t season,
                                                TeamID team_id) const;

 private:
  std::shared_ptr<DatabaseConnection> db_conn;
};
