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

#include "match_engine.h"

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

  MatchEngine engine(home_team.getLineup(), away_team.getLineup(),
                     home_team.getStrategy(), away_team.getStrategy(),
                     game_data.getStatsConfig());

  // Physics tick rate for background sim: ~60 ticks per real second (so 1 tick
  // = 1 sim minute) Wait, if TIME_SCALE=60, then dt=1.0f means 1 sim minute.
  // Let's use a smaller dt for stability: 0.1s.
  // 90 minutes * 60 seconds = 5400 simulation seconds.
  // 5400 / (0.1 * 60) = 900 ticks.
  const float dt = 0.1f;
  while (engine.getMatchTimeMinutes() < 90.0f)
  {
    engine.update(dt);
  }

  home_score = engine.getHomeScore();
  away_score = engine.getAwayScore();
}

uint16_t Match::getHomeTeamId() const { return home_team_id; }
uint16_t Match::getAwayTeamId() const { return away_team_id; }
uint8_t Match::getHomeScore() const { return home_score; }
uint8_t Match::getAwayScore() const { return away_score; }
MatchType Match::getMatchType() const { return match_type; }
const GameDateValue& Match::getDate() const { return match_date; }

bool Match::isPlayed() const { return _played; }
