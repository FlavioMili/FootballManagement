#include "match.h"
#include "player.h"

Match::Match(Team& home, Team& away)
: home_team(home), away_team(away), home_score(0), away_score(0) {}

/**
* TODO : improve the simulation, it is one of the 
* fundamentals of the game.
*/
void Match::simulate() {
   double home_strength = 0;
   for (const auto& player : home_team.getPlayers()) {
      home_strength += player.getOverall();
   }

   double away_strength = 0;
   for (const auto& player : away_team.getPlayers()) {
      away_strength += player.getOverall();
   }

   double total_strength = home_strength + away_strength;
   double home_chance = home_strength / total_strength;

   int goals = rand() % 6;
   for (int i = 0; i < goals; ++i) {
      if ((double)rand() / RAND_MAX < home_chance) {
         home_score++;
      } else {
         away_score++;
      }
   }
}

Team& Match::getHomeTeam() const {
   return home_team;
}

Team& Match::getAwayTeam() const {
   return away_team;
}

int Match::getHomeScore() const {
   return home_score;
}

int Match::getAwayScore() const {
   return away_score;
}
