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
#include "model/staff.h"

/**
 * @class StaffRepository
 * @brief Persists every staff member (table Staff; team_id 0 = market).
 */
class StaffRepository
{
 public:
  explicit StaffRepository(std::shared_ptr<DatabaseConnection> db_conn);

  /** Loads all staff (empty for saves from before staff existed). */
  std::vector<StaffMember> loadAll() const;

  /** Replaces the stored staff (inside the caller's transaction). */
  void replaceAll(const StaffRoster& roster) const;

 private:
  std::shared_ptr<DatabaseConnection> db_conn;
};
