// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <sqlite3.h>

#include <string>

#include "database/database_connection.h"

/**
 * Small helpers for the state tables that are rewritten on every save
 * (DELETE + INSERT inside the caller's transaction).
 */
namespace SqliteRows
{
template <typename T>
T column(sqlite3_stmt* stmt, int index)
{
  return static_cast<T>(sqlite3_column_int64(stmt, index));
}

inline float columnFloat(sqlite3_stmt* stmt, int index)
{
  return static_cast<float>(sqlite3_column_double(stmt, index));
}

inline std::string columnText(sqlite3_stmt* stmt, int index)
{
  const unsigned char* text = sqlite3_column_text(stmt, index);
  return text ? std::string(reinterpret_cast<const char*>(text))
              : std::string();
}

inline void bindText(sqlite3_stmt* stmt, int index, const std::string& text)
{
  sqlite3_bind_text(stmt, index, text.c_str(), -1, SQLITE_TRANSIENT);
}

/** Runs @p read for every row of @p sql. */
template <typename Read>
void forEach(const DatabaseConnection& db, const char* sql, Read read)
{
  sqlite3_stmt* stmt = db.prepareStatement(sql);
  while (sqlite3_step(stmt) == SQLITE_ROW) read(stmt);
  sqlite3_finalize(stmt);
}

/** Inserts every item of @p items with one prepared statement. */
template <typename Range, typename Bind>
void insertAll(const DatabaseConnection& db, const char* sql,
               const Range& items, Bind bind)
{
  sqlite3_stmt* stmt = db.prepareStatement(sql);
  for (const auto& item : items)
  {
    bind(stmt, item);
    db.executeStep(stmt);
    sqlite3_reset(stmt);
    sqlite3_clear_bindings(stmt);
  }
  sqlite3_finalize(stmt);
}

/** Empties a table owned by the caller. */
inline void clearTable(const DatabaseConnection& db, const char* table)
{
  sqlite3_exec(db.getRaw(), (std::string("DELETE FROM ") + table + ";").c_str(),
               nullptr, nullptr, nullptr);
}
}  // namespace SqliteRows
