// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "match.h"

#include <random>
#include <string>

#include "database/gamedata.h"
#include "lineup.h"
#include "model/role_utils.h"
#include "player.h"

Match::Match(TeamID home_id, TeamID away_id, GameDateValue date, MatchType type)
    : home_team_id(home_id),
      away_team_id(away_id),
      match_date(date),
      match_type(type),
      home_score(0),
      away_score(0)
{
}

void Match::simulate(const GameData& game_data)
{
  _played = true;
  auto home_team_opt = game_data.getTeam(home_team_id);
  auto away_team_opt = game_data.getTeam(away_team_id);
  if (!home_team_opt || !away_team_opt)
  {
    return;  // Or handle error appropriately
  }
  const Team& home_team = home_team_opt->get();
  const Team& away_team = away_team_opt->get();

  auto getAvgOvr = [&](const Lineup& l)
  {
    float sum = 0.0f;
    int count = 0;
    if (auto gk = l.getGoalkeeper())
    {
      sum += gk->getOverall(game_data.getStatsConfig());
      count++;
    }
    for (auto& p : l.getOutfieldPlayers())
    {
      if (p.player)
      {
        sum += p.player->getOverall(game_data.getStatsConfig());
        count++;
      }
    }
    return count > 0 ? sum / count : 50.0f;
  };

  float homeOvr = getAvgOvr(home_team.getLineup());
  float awayOvr = getAvgOvr(away_team.getLineup());

  std::mt19937 rng(std::random_device{}());

  float homeExpected =
      std::max(0.1f, (homeOvr / 100.0f) * 2.5f + (homeOvr - awayOvr) / 20.0f);
  float awayExpected =
      std::max(0.1f, (awayOvr / 100.0f) * 2.0f + (awayOvr - homeOvr) / 20.0f);

  std::poisson_distribution<int> homeGoals(homeExpected);
  std::poisson_distribution<int> awayGoals(awayExpected);

  home_score = homeGoals(rng);
  away_score = awayGoals(rng);
}

void Match::setPlayedResult(uint8_t h, uint8_t a)
{
  home_score = h;
  away_score = a;
  _played = true;
}

uint16_t Match::getHomeTeamId() const { return home_team_id; }
uint16_t Match::getAwayTeamId() const { return away_team_id; }
uint8_t Match::getHomeScore() const { return home_score; }
uint8_t Match::getAwayScore() const { return away_score; }
MatchType Match::getMatchType() const { return match_type; }
const GameDateValue& Match::getDate() const { return match_date; }

bool Match::isPlayed() const { return _played; }
