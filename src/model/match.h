// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include "team.h"
#include "global/stats_config.h"
#include <cstdint>

struct Matchup {
  uint32_t home_team_id;
  uint32_t away_team_id;
};

/**
  * The match class manages the simulation of matches
  * This is one of the most important classses for the
  * game experience, it will be updated very often until
  * a good solution is not found.
  *
  * The idea is to eventually have some parameters so that
  * The players could possibly add mods and adapt the game 
  * engine to their liking. 
  * I think this would also bring more help from the community
  * to improve the game simulation.
*/
class Match {
 public:
  Match(const Team& home, const Team& away,
        const StatsConfig& _stats_config);

  void simulate();

  const Team& getHomeTeam() const;
  const Team& getAwayTeam() const;
  int getHomeScore() const;
  int getAwayScore() const;

 private:
  const Team& home_team;
  const Team& away_team;
  const StatsConfig& stats_config;
  int home_score;
  int away_score;
};
