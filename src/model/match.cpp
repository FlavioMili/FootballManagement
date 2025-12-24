// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "match.h"
#include "database/gamedata.h"
#include "lineup.h"
#include "player.h"
#include <random>
#include <string>

Match::Match(uint16_t home_id, uint16_t away_id, GameDateValue date,
             MatchType type)
    : home_team_id(home_id), away_team_id(away_id), match_date(date),
      match_type(type), home_score(0), away_score(0) {}

void Match::simulate(const GameData &game_data) {
  _played = true;
  auto home_team_opt = game_data.getTeam(home_team_id);
  auto away_team_opt = game_data.getTeam(away_team_id);
  if (!home_team_opt || !away_team_opt) {
    return; // Or handle error appropriately
  }
  const Team &home_team = home_team_opt->get();
  const Team &away_team = away_team_opt->get();

  const Lineup &home_lineup = home_team.getLineup();
  const Lineup &away_lineup = away_team.getLineup();

  double home_attack = 0, home_midfield = 0, home_defense = 0;
  double away_attack = 0, away_midfield = 0, away_defense = 0;

  const auto &stats_config = game_data.getStatsConfig();

  // Home lineup
  if (Player *gk = home_lineup.getGoalkeeper())
    home_defense += gk->getOverall(stats_config);

  for (int i = 0; i < LINEUP_GRID_SIZE; ++i) {
    if (Player *p = home_lineup.getPlayerAt(i)) {
      std::string role = p->getRole();
      if (role == "Striker")
        home_attack += p->getOverall(stats_config);
      else if (role == "Midfielder")
        home_midfield += p->getOverall(stats_config);
      else if (role == "Defender")
        home_defense += p->getOverall(stats_config);
      else if (role == "Goalkeeper")
        home_defense += p->getOverall(stats_config);
    }
  }

  // Away lineup
  if (Player *gk = away_lineup.getGoalkeeper())
    away_defense += gk->getOverall(stats_config);

  for (int i = 0; i < LINEUP_GRID_SIZE; ++i) {
    if (Player *p = away_lineup.getPlayerAt(i)) {
      std::string role = p->getRole();
      if (role == "Striker")
        away_attack += p->getOverall(stats_config);
      else if (role == "Midfielder")
        away_midfield += p->getOverall(stats_config);
      else if (role == "Defender")
        away_defense += p->getOverall(stats_config);
      else if (role == "Goalkeeper")
        away_defense += p->getOverall(stats_config);
    }
  }

  double home_strength = home_attack * 1.5 + home_midfield + home_defense * 0.8;
  double away_strength = away_attack * 1.5 + away_midfield + away_defense * 0.8;

  double total_strength = home_strength + away_strength;
  double home_chance =
      total_strength > 0 ? home_strength / total_strength : 0.5;

  static std::random_device rd;
  static std::mt19937 gen(rd());
  std::uniform_int_distribution<> goal_dist(0, 5);
  std::uniform_real_distribution<> chance_dist(0.0, 1.0);

  int goals = goal_dist(gen);
  for (int i = 0; i < goals; ++i) {
    if (chance_dist(gen) < home_chance) home_score++;
    else away_score++;
  }
}

uint16_t Match::getHomeTeamId() const { return home_team_id; }
uint16_t Match::getAwayTeamId() const { return away_team_id; }
uint8_t Match::getHomeScore() const { return home_score; }
uint8_t Match::getAwayScore() const { return away_score; }
MatchType Match::getMatchType() const { return match_type; }
const GameDateValue &Match::getDate() const { return match_date; }

bool Match::isPlayed() const { return _played; }
