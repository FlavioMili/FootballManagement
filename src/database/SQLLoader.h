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

class SQLLoader {
 public:
  static SQLLoader& getInstance();

  // Load SQL file and parse queries
  void loadQueriesFromFile(const std::string& filepath);

  // Load schema from file
  std::string loadSchemaFromFile(const std::string& filepath);

  // Get query by ID
  const std::string& getQuery(const std::string& query_id) const;

  // Check if query exists
  bool hasQuery(const std::string& query_id) const;

 private:
  SQLLoader() = default;
  std::unordered_map<std::string, std::string> queries_;

  // Helper methods
  std::string readFile(const std::string& filepath);
  void parseQueries(const std::string& content);
  std::string trim(const std::string& str);
};
