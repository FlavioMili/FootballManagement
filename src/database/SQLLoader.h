// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <string>
#include <unordered_map>

#include "global/queries.h"

/**
 * @class SQLLoader
 * @brief Utility class for loading SQL files and parsing queries.
 */
class SQLLoader
{
 public:
  /**
   * @brief Load SQL file and parse queries.
   */
  static void loadQueriesFromFile();

  /**
   * @brief Load database schema from file.
   * @return A string containing the database schema.
   */
  static std::string loadSchemaFromFile();

  /**
   * @brief Get a query by its ID.
   * @param query The identifier of the query to retrieve.
   * @return A constant reference to the query string.
   */
  static const std::string& getQuery(const Query query);

 private:
  SQLLoader() = default;

  static std::unordered_map<Query, std::string> queries_;

  // Helpers
  static std::string readFile(const std::string& filepath);
  static void parseQueries(const std::string& content);
  static std::string trim(const std::string& str);
};
