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
#include "model/inbox.h"

/**
 * @class InboxRepository
 * @brief Persists the managed club's inbox (table InboxMessages).
 */
class InboxRepository
{
 public:
  explicit InboxRepository(std::shared_ptr<DatabaseConnection> db_conn);

  /** Loads every message ordered by id. */
  std::vector<InboxMessage> loadAll() const;

  /** Replaces the stored messages (the inbox is capped, so this is cheap). */
  void replaceAll(const std::vector<InboxMessage>& messages) const;

 private:
  std::shared_ptr<DatabaseConnection> db_conn;
};
