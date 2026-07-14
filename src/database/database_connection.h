// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <sqlite3.h>

#include <memory>
#include <string>

/**
 * @class DatabaseConnection
 * @brief Manages the SQLite database connection and transaction lifecycle.
 *
 * This class provides a safe RAII wrapper around the raw `sqlite3` connection
 * and offers transactional boundaries and statement execution functions.
 */
class DatabaseConnection
{
 public:
  /**
   * @brief Constructs a new DatabaseConnection.
   * @param db_path The path to the SQLite database file.
   */
  explicit DatabaseConnection(const std::string& db_path);
  ~DatabaseConnection() = default;

  /**
   * @brief Initializes the database schema.
   */
  void initialize() const;

  /**
   * @brief Access the raw sqlite3 connection object.
   * @return A pointer to the raw sqlite3 object.
   */
  sqlite3* getRaw() const { return db.get(); }

  /**
   * @brief Begins a new SQLite transaction.
   */
  void beginTransaction() const;

  /**
   * @brief Commits the current SQLite transaction.
   */
  void commitTransaction() const;

  /**
   * @brief Rolls back the current SQLite transaction.
   */
  void rollbackTransaction() const;

  sqlite3_stmt* prepareStatement(const std::string& sql) const;
  void executeStep(sqlite3_stmt* stmt) const;

 private:
  std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db{nullptr,
                                                        &sqlite3_close};
  void loadSQLFiles() const;
};
