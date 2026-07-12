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
#include "model/league.h"

class LeagueRepository
{
 public:
  explicit LeagueRepository(std::shared_ptr<DatabaseConnection> db_conn);

  std::vector<League> loadAllLeagues() const;
  void insertLeague(const League& league) const;
  void insertLeagueWithId(const League& league) const;

  void saveLeaguePoints(const League& league) const;
  void loadLeaguePoints(League& league) const;
  void resetAllLeaguePoints() const;

 private:
  std::shared_ptr<DatabaseConnection> db_conn;
  void loadTeamsForLeague(League& league) const;
};
