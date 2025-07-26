#pragma once

#include "team.h"

struct Matchup {
  int home_team_id;
  int away_team_id;
};

class Match {
 public:
  Match(const Team& home, const Team& away);

  void simulate();

  const Team& getHomeTeam() const;
  const Team& getAwayTeam() const;
  int getHomeScore() const;
  int getAwayScore() const;

 private:
  const Team& home_team;
  const Team& away_team;
  int home_score;
  int away_score;
};
