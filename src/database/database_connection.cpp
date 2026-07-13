// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "database_connection.h"

#include <sqlite3.h>

#include <iostream>
#include <stdexcept>

#include "SQLLoader.h"
#include "database_exception.h"
#include "global/logger.h"

DatabaseConnection::DatabaseConnection(const std::string& db_path)
{
  sqlite3* raw_db = nullptr;
  if (sqlite3_open(db_path.c_str(), &raw_db) != SQLITE_OK)
  {
    std::string err = sqlite3_errmsg(raw_db);
    sqlite3_close(raw_db);
    throw DatabaseException("Failed to open database: " + db_path +
                            " Error: " + err);
  }

  db.reset(raw_db);
  sqlite3_exec(db.get(), "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);

  loadSQLFiles();
}

void DatabaseConnection::loadSQLFiles() const
{
  try
  {
    SQLLoader::loadQueriesFromFile();
  }
  catch (const std::exception& e)
  {
    Logger::error("Failed to load SQL queries: " + std::string(e.what()) +
                  '\n');
    throw;
  }
}

void DatabaseConnection::initialize() const
{
  Logger::debug("DatabaseConnection::initialize called.");
  try
  {
    std::string schema_sql = SQLLoader::loadSchemaFromFile();
    char* err_msg = nullptr;

    if (sqlite3_exec(db.get(), schema_sql.c_str(), nullptr, nullptr,
                     &err_msg) != SQLITE_OK)
    {
      std::string error_str =
          "SQL error during schema initialization: " + std::string(err_msg);
      sqlite3_free(err_msg);
      Logger::error(error_str);
    }
    else
    {
      Logger::debug("Database schema initialized successfully.\n");
    }
  }
  catch (const std::exception& e)
  {
    Logger::error("Failed to initialize database: " + std::string(e.what()) +
                  "\n");
    throw;
  }
}

void DatabaseConnection::beginTransaction() const
{
  char* err_msg = nullptr;
  if (sqlite3_exec(db.get(), "BEGIN TRANSACTION;", nullptr, nullptr,
                   &err_msg) != SQLITE_OK)
  {
    std::string err = err_msg ? err_msg : "Unknown error";
    if (err_msg) sqlite3_free(err_msg);
    throw DatabaseException("Failed to begin transaction: " + err);
  }
}

void DatabaseConnection::commitTransaction() const
{
  char* err_msg = nullptr;
  if (sqlite3_exec(db.get(), "COMMIT;", nullptr, nullptr, &err_msg) !=
      SQLITE_OK)
  {
    std::string err = err_msg ? err_msg : "Unknown error";
    if (err_msg) sqlite3_free(err_msg);
    throw DatabaseException("Failed to commit transaction: " + err);
  }
}

void DatabaseConnection::rollbackTransaction() const
{
  char* err_msg = nullptr;
  if (sqlite3_exec(db.get(), "ROLLBACK;", nullptr, nullptr, &err_msg) !=
      SQLITE_OK)
  {
    std::string err = err_msg ? err_msg : "Unknown error";
    if (err_msg) sqlite3_free(err_msg);
    throw DatabaseException("Failed to rollback transaction: " + err);
  }
}

sqlite3_stmt* DatabaseConnection::prepareStatement(const std::string& sql) const
{
  sqlite3_stmt* stmt;
  if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, nullptr) !=
      SQLITE_OK)
  {
    throw DatabaseException("Failed to prepare statement: " +
                            std::string(sqlite3_errmsg(db.get())));
  }
  return stmt;
}

void DatabaseConnection::executeStep(sqlite3_stmt* stmt) const
{
  if (sqlite3_step(stmt) != SQLITE_DONE)
  {
    std::string err = sqlite3_errmsg(db.get());
    sqlite3_finalize(stmt);
    throw DatabaseException("Failed to execute statement: " + err);
  }
}
