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
  Match(uint16_t home_id, uint16_t away_id, GameDateValue date, MatchType type);

  void simulate(const GameData &game_data);

  uint16_t getHomeTeamId() const;
  uint16_t getAwayTeamId() const;
  uint8_t getHomeScore() const;
  uint8_t getAwayScore() const;
  MatchType getMatchType() const;
  const GameDateValue &getDate() const;
  bool isPlayed() const;

 private:
  uint16_t home_team_id;
  uint16_t away_team_id;
  GameDateValue match_date;
  MatchType match_type;
  uint8_t home_score;
  uint8_t away_score;
  bool _played = false;
};
