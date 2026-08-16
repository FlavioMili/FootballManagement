// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "match.h"

#include <algorithm>
#include <cstdint>

#include "database/gamedata.h"
#include "lineup.h"
#include "model/match_engine.h"
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
  if (_played)
  {
    return;
  }

  auto home_team_opt = game_data.getTeam(home_team_id);
  auto away_team_opt = game_data.getTeam(away_team_id);
  if (!home_team_opt || !away_team_opt)
  {
    return;  // Or handle error appropriately
  }
  const Team& home_team = home_team_opt->get();
  const Team& away_team = away_team_opt->get();
  const uint32_t seed = (static_cast<uint32_t>(home_team_id) << 16U) ^
                        static_cast<uint32_t>(away_team_id) ^
                        (static_cast<uint32_t>(match_date.year) << 8U) ^
                        (static_cast<uint32_t>(match_date.month) << 4U) ^
                        match_date.day;
  MatchEngine engine(home_team.getLineup(), away_team.getLineup(),
                     home_team.getStrategy(), away_team.getStrategy(),
                     game_data.getStatsConfig(), seed);
  while (engine.getState() != MatchState::FULL_TIME)
  {
    engine.update(0.25f);
  }

  home_score = static_cast<uint8_t>(std::clamp(engine.getHomeScore(), 0, 255));
  away_score = static_cast<uint8_t>(std::clamp(engine.getAwayScore(), 0, 255));
  _played = true;
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
