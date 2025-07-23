#pragma once

#include "team.h"

struct Matchup {
   int home_team_id;
   int away_team_id;
};

class Match {
public:
   Match(Team& home, Team& away);

   void simulate();

   Team& getHomeTeam() const;
   Team& getAwayTeam() const;
   int getHomeScore() const;
   int getAwayScore() const;

private:
   Team& home_team;
   Team& away_team;
   int home_score;
   int away_score;
};
