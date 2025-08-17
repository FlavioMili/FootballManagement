#include "match.h"
#include <cstdlib>
#include "player.h"

Match::Match(const Team& home, const Team& away, const StatsConfig& stats_config)
: home_team(home), away_team(away), stats_config(stats_config), home_score(0), away_score(0) {}

void Match::simulate() {
  double home_strength = 0;
  for (const auto& player : home_team.getPlayers()) {
    home_strength += player.getOverall(stats_config);
  }

  double away_strength = 0;
  for (const auto& player : away_team.getPlayers()) {
    away_strength += player.getOverall(stats_config);
  }

  double total_strength = home_strength + away_strength;
  double home_chance = home_strength / total_strength;

  int goals = rand() % 6;
  for (int i = 0; i < goals; ++i) {
    if (static_cast<double> (rand()) / RAND_MAX < home_chance)  {
      home_score++;
    } else {
      away_score++;
    }
  }
}

const Team& Match::getHomeTeam() const {
  return home_team;
}

const Team& Match::getAwayTeam() const {
  return away_team;
}

int Match::getHomeScore() const {
  return home_score;
}

int Match::getAwayScore() const {
  return away_score;
}
