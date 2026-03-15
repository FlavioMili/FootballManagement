// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>

// ID Types
using LeagueID = uint8_t;
using TeamID = uint16_t;
using PlayerID = uint32_t;

enum class SeasonPhase : uint8_t
{
  PRE_SEASON,
  REGULAR_SEASON,
  POST_SEASON,
  OFF_SEASON
};

enum class MatchType : uint8_t
{
  LEAGUE = 0,
  FRIENDLY = 1,
  CUP = 2
};

struct Matchup
{
  TeamID home_team_id;
  TeamID away_team_id;
  MatchType type;
};