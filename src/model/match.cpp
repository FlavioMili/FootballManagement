#include "match.h"
#include <cstdlib>
#include "player.h"
#include "lineup.h"

Match::Match(const Team& home, const Team& away, const StatsConfig& stats_config)
: home_team(home), away_team(away), stats_config(stats_config), home_score(0), away_score(0) {}

// TODO this needs to become a smarter simulation, considering a heat map
// randomising where the game is taking place and moving to step by step
void Match::simulate() {
  const Lineup& home_lineup = home_team.getLineup();
  const Lineup& away_lineup = away_team.getLineup();

  double home_attack = 0, home_midfield = 0, home_defense = 0;
  double away_attack = 0, away_midfield = 0, away_defense = 0;

  // Home lineup
  if (Player* gk = home_lineup.getGoalkeeper())
    home_defense += gk->getOverall(stats_config);

  for (int i = 0; i < LINEUP_GRID_SIZE; ++i) {
    if (Player* p = home_lineup.getPlayerAt(i)) {
      std::string role = p->getRole();

      // TODO this one will not be hard coded anymore, it is just a placeholder
      if (role == "Striker") home_attack += p->getOverall(stats_config);
      else if (role == "Midfielder") home_midfield += p->getOverall(stats_config);
      else if (role == "Defender") home_defense += p->getOverall(stats_config);
      else if (role == "Goalkeeper") home_defense += p->getOverall(stats_config);
    }
  }

  // Away lineup
  if (Player* gk = away_lineup.getGoalkeeper())
    away_defense += gk->getOverall(stats_config);

  for (int i = 0; i < LINEUP_GRID_SIZE; ++i) {
    if (Player* p = away_lineup.getPlayerAt(i)) {
      std::string role = p->getRole();
      if (role == "Striker") away_attack += p->getOverall(stats_config);
      else if (role == "Midfielder") away_midfield += p->getOverall(stats_config);
      else if (role == "Defender") away_defense += p->getOverall(stats_config);
      else if (role == "Goalkeeper") away_defense += p->getOverall(stats_config);
    }
  }

  // Simple strength calculation
  double home_strength = home_attack * 1.5 + home_midfield + home_defense * 0.8;
  double away_strength = away_attack * 1.5 + away_midfield + away_defense * 0.8;

  double total_strength = home_strength + away_strength;
  double home_chance = total_strength > 0 ? home_strength / total_strength : 0.5;

  int goals = rand() % 6;
  for (int i = 0; i < goals; ++i) {
    if (static_cast<double>(rand()) / RAND_MAX < home_chance)
      home_score++;
    else
      away_score++;
  }
}

const Team& Match::getHomeTeam() const { return home_team; }
const Team& Match::getAwayTeam() const { return away_team; }
int Match::getHomeScore() const { return home_score; }
int Match::getAwayScore() const { return away_score; }
