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
 * @brief Persists the managed club's inbox (table InboxMessages) and the
 * inbox screen's filters (table InboxView).
 */
class InboxRepository
{
 public:
  explicit InboxRepository(std::shared_ptr<DatabaseConnection> db_conn);

  /** Loads every message ordered by id. */
  std::vector<InboxMessage> loadAll() const;

  /** Replaces the stored messages (the inbox is capped, so this is cheap). */
  void replaceAll(const std::vector<InboxMessage>& messages) const;

  /** Saved filters of the inbox screen (defaults when none were stored). */
  InboxView loadView() const;

  /** Stores the filters of the inbox screen (table InboxView). */
  void saveView(const InboxView& view) const;

 private:
  std::shared_ptr<DatabaseConnection> db_conn;
};
