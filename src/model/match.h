// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include "gamedate.h"
#include "global/types.h"
#include <cstdint>

class GameData;

class Match {
 public:
  Match(TeamID home_id, TeamID away_id, GameDateValue date, MatchType type);

  void simulate(const GameData &game_data);

  TeamID getHomeTeamId() const;
  TeamID getAwayTeamId() const;
  uint8_t getHomeScore() const;
  uint8_t getAwayScore() const;
  MatchType getMatchType() const;
  const GameDateValue &getDate() const;
  bool isPlayed() const;

 private:
  TeamID home_team_id;
  TeamID away_team_id;
  GameDateValue match_date;
  MatchType match_type;
  uint8_t home_score;
  uint8_t away_score;
  bool _played = false;
};
