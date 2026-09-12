// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <memory>
#include <unordered_map>

#include "database/database_connection.h"
#include "model/training.h"

/**
 * @class TrainingRepository
 * @brief Persists club training plans and familiarity (table TeamTraining)
 * and player focus and workload (table PlayerTraining).
 */
class TrainingRepository
{
 public:
  explicit TrainingRepository(std::shared_ptr<DatabaseConnection> db_conn);

  /** Loads every stored club plan. */
  std::unordered_map<TeamID, TeamTrainingPlan> loadPlans() const;

  /** Loads every stored player state. */
  std::unordered_map<PlayerID, PlayerTrainingState> loadPlayers() const;

  /** Replaces the stored plans and player states (caller's transaction). */
  void replaceAll(const TrainingRegistry& registry) const;

 private:
  std::shared_ptr<DatabaseConnection> db_conn;
};
