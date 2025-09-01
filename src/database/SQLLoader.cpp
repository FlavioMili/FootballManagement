// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "SQLLoader.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <iostream>

SQLLoader& SQLLoader::getInstance() {
  static SQLLoader instance;
  return instance;
}

void SQLLoader::loadQueriesFromFile(const std::string& filepath) {
  std::string content = readFile(filepath);
  parseQueries(content);
}

std::string SQLLoader::loadSchemaFromFile(const std::string& filepath) {
  return readFile(filepath);
}

const std::string& SQLLoader::getQuery(const std::string& query_id) const {
  auto it = queries_.find(query_id);
  if (it == queries_.end()) {
    throw std::runtime_error("Query not found: " + query_id);
  }
  return it->second;
}

bool SQLLoader::hasQuery(const std::string& query_id) const {
  return queries_.contains(query_id);
}

std::string SQLLoader::readFile(const std::string& filepath) {
  std::ifstream file(filepath);
  if (!file.is_open()) {
    throw std::runtime_error("Cannot open file: " + filepath);
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

void SQLLoader::parseQueries(const std::string& content) {
  // Split content into lines
  std::istringstream stream(content);
  std::string line;
  std::string current_query_id;
  std::string current_query;
  bool in_query = false;

  while (std::getline(stream, line)) {
    line = trim(line);

    // Skip empty lines and comments (unless it's a query ID comment)
    if (line.empty() || (line.substr(0, 2) == "--" && line.find("@QUERY_ID:") == std::string::npos)) {
      continue;
    }

    // Check if this line contains a query ID
    if (line.find("-- @QUERY_ID:") != std::string::npos) {
      // Save previous query if exists
      if (!current_query_id.empty() && !current_query.empty()) {
        queries_[current_query_id] = trim(current_query);
      }

      // Extract new query ID
      size_t start = line.find("@QUERY_ID:") + 10;
      current_query_id = trim(line.substr(start));
      current_query.clear();
      in_query = true;
      continue;
    }

    // If we're in a query, accumulate the SQL
    if (in_query && !line.empty()) {
      if (!current_query.empty()) {
        current_query += " ";
      }
      current_query += line;
    }
  }

  // Save the last query
  if (!current_query_id.empty() && !current_query.empty()) {
    queries_[current_query_id] = trim(current_query);
  }

  // Debug line 
  // std::cout << "Loaded " << queries_.size() << " queries from file." << std::endl;
}

std::string SQLLoader::trim(const std::string& str) {
  size_t start = str.find_first_not_of(" \t\n\r");
  if (start == std::string::npos) return "";

  size_t end = str.find_last_not_of(" \t\n\r");
  return str.substr(start, end - start + 1);
}
