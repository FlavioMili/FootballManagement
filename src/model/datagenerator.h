#pragma once

#include <string>
#include <vector>
#include "database.h"
#include "json.hpp"

class DataGenerator {
 public:
  explicit DataGenerator(Database& db,
     const nlohmann::json& stats_config,
     const std::vector<std::string>& league_names,
     const std::vector<std::string>& team_names);
  void generateAll();

 private:
  void generatePlayers(int team_id, const std::string& role, int count);
  void generateTeams(int league_id, const std::vector<std::string>& names);
  void generateLeagues(const std::vector<std::string>& names);

  Database& db;
  const nlohmann::json& stats_config;
  const std::vector<std::string>& league_names;
  const std::vector<std::string>& team_names;
  std::vector<std::string> first_names;
  std::vector<std::string> last_names;
};
