// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

#include "database/database_connection.h"
#include "global/types.h"
#include "model/finances.h"

class Team;

/**
 * @class FinanceRepository
 * @brief Persists club ledgers (table FinanceLedger, keyed by team and
 * sequence number, append-only).
 */
class FinanceRepository
{
 public:
  explicit FinanceRepository(std::shared_ptr<DatabaseConnection> db_conn);

  /** Loads every ledger, oldest transaction first. */
  std::unordered_map<TeamID, std::vector<FinanceTransaction>> loadAll() const;

  /**
   * Inserts the teams' not yet persisted transactions. Rows that already
   * exist are ignored, so flushing twice before markPersisted() is safe.
   */
  void insertPending(
      const std::vector<std::reference_wrapper<const Team>>& teams) const;

 private:
  std::shared_ptr<DatabaseConnection> db_conn;
};
