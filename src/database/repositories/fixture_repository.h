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

class FixtureRepository
{
 public:
  explicit FixtureRepository(std::shared_ptr<DatabaseConnection> db_conn);

  std::vector<Match> loadAllMatches() const;
  void insertFixture(const Match& match) const;

  void saveCalendar(const Calendar& calendar) const;
  void loadCalendar(Calendar& calendar) const;

 private:
  std::shared_ptr<DatabaseConnection> db_conn;
};
