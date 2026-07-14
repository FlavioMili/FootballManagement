// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <memory>
#include <vector>

#include "database/database_connection.h"
#include "model/calendar.h"
#include "model/match.h"

/**
 * @class FixtureRepository
 * @brief Repository class for managing Match and Calendar entities in the
 * database.
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

 private:
  std::shared_ptr<DatabaseConnection> db_conn;
};
