// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include "league.h"
#include "player.h"
#include "team.h"
#include <cstdint>
#include <string>
#include <vector>

class DataGenerator {
 public:
  static std::vector<League> generateLeagues();
  static std::vector<Team> generateTeams();
  static std::vector<Player> generatePlayers();

 private:
  static std::vector<std::string> first_names;
  static std::vector<std::string> last_names;

  static void loadNames();
  static Player generateRandomPlayer(uint16_t team_id);
};
