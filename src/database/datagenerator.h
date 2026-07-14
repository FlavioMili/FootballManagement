// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "league.h"
#include "player.h"
#include "team.h"

/**
 * @class DataGenerator
 * @brief Class responsible for generating initial data for leagues, teams, and
 * players.
 */
class DataGenerator
{
 public:
  /**
   * @brief Generate a collection of leagues.
   * @return A vector of generated League objects.
   */
  static std::vector<League> generateLeagues();

  /**
   * @brief Generate a collection of teams.
   * @return A vector of generated Team objects.
   */
  static std::vector<Team> generateTeams();

  /**
   * @brief Generate a collection of players.
   * @return A vector of generated Player objects.
   */
  static std::vector<Player> generatePlayers(class GameData& gamedata);

 private:
  static std::vector<std::string> first_names;
  static std::vector<std::string> last_names;

  static void loadNames();
  static Player generateRandomPlayer(class GameData& gamedata, TeamID team_id);
};
