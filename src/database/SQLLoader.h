// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <string>
#include <unordered_map>
#include "global/queries.h"

class SQLLoader {
 public:
  // Load SQL file and parse queries
  static void loadQueriesFromFile();

  // Load schema from file
  static std::string loadSchemaFromFile();

  // Get query by ID
  static const std::string& getQuery(const Query query);

 private:
  SQLLoader() = default;

  static std::unordered_map<Query, std::string> queries_;

  // Helpers
  static std::string readFile(const std::string& filepath);
  static void parseQueries(const std::string& content);
  static std::string trim(const std::string& str);
};
