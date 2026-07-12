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
#include "model/team.h"

class TeamRepository
{
 public:
  explicit TeamRepository(std::shared_ptr<DatabaseConnection> db_conn);

  std::vector<Team> loadAllTeams() const;
  void insertTeam(const Team& team) const;
  void insertTeamWithId(const Team& team) const;

 private:
  std::shared_ptr<DatabaseConnection> db_conn;
};
