// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>

enum class SeasonPhase : uint8_t {
    PRE_SEASON,
    REGULAR_SEASON,
    POST_SEASON,
    OFF_SEASON
};

enum class MatchType : uint8_t {
    LEAGUE = 0,
    FRIENDLY = 1,
    CUP = 2
};

struct Matchup {
  uint16_t home_team_id;
  uint16_t away_team_id;
  MatchType type;
};
