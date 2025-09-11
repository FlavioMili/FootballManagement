// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <string>
#include <vector>
#include "database.h"
#include "json.hpp"

/*
  * This class is used to generate data
  * during the first run and at the end of seasons
  * or also if needed during the game.
* TODO add generation of single league, team or player 
* this would make it possible to add in-game editors.
  */
class DataGenerator {
 public:
  explicit DataGenerator(std::shared_ptr<Database> db,
                 const nlohmann::json& stats_config,
                 const std::vector<std::string>& league_names,
                 const std::vector<std::string>& team_names);
  void generateAll();

 private:
  void generatePlayers(int team_id, const std::string& role, int count);
  void generateTeams(int league_id, const std::vector<std::string>& names);
  void generateLeagues(const std::vector<std::string>& names);

  std::shared_ptr<Database> db;
  const nlohmann::json& stats_config;
  const std::vector<std::string>& league_names;
  const std::vector<std::string>& team_names;
  std::vector<std::string> first_names;
  std::vector<std::string> last_names;
};
