// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <memory>

#include "database/database_connection.h"
#include "model/scouting.h"

/**
 * @class ScoutingRepository
 * @brief Persists the scouting department (tables ScoutingState,
 * ScoutingKnowledge, ScoutAssignments, ScoutReports, RecruitmentFocus and
 * ScoutShortlist).
 */
class ScoutingRepository
{
 public:
  explicit ScoutingRepository(std::shared_ptr<DatabaseConnection> db_conn);

  /** Loads the stored state; false (and an empty state) if none exists. */
  bool load(ScoutingState& state) const;

  /** Replaces the stored state (call inside the caller's transaction). */
  void save(const ScoutingState& state) const;

 private:
  std::shared_ptr<DatabaseConnection> db_conn;
};
