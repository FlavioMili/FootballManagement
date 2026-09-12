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
   * @brief Loads the predefined players and completes every club's squad
   * with generated players (see WorldGeneration). Club profiles must be
   * assigned first; generation is deterministic for the world seed.
   * @return A vector of generated Player objects.
   */
  static std::vector<Player> generatePlayers(const class GameData& gamedata);
};
