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

class DatabaseConnection
{
 public:
  explicit DatabaseConnection(const std::string& db_path);
  ~DatabaseConnection() = default;

  void initialize() const;
  sqlite3* getRaw() const { return db.get(); }

  void beginTransaction() const;
  void commitTransaction() const;
  void rollbackTransaction() const;

  sqlite3_stmt* prepareStatement(const std::string& sql) const;
  void executeStep(sqlite3_stmt* stmt) const;

 private:
  std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db{nullptr,
                                                        &sqlite3_close};
  void loadSQLFiles() const;
};
