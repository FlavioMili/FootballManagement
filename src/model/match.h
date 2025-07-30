#pragma once

#include "team.h"
#include "stats_config.h"

struct Matchup {
  int home_team_id;
  int away_team_id;
};

class Match {
 public:
  Match(const Team& home, const Team& away, const StatsConfig& stats_config);

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
