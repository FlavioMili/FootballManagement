// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "global/types.h"

class Calendar;
class GameData;
class League;

/**
 * @struct StandingRow
 * @brief One row of a league table, derived from played league fixtures.
 */
struct StandingRow
{
  TeamID team_id = 0;
  uint16_t position = 0; /*!< 1-based table position. */
  uint16_t played = 0;
  uint16_t won = 0;
  uint16_t drawn = 0;
  uint16_t lost = 0;
  uint16_t goals_for = 0;
  uint16_t goals_against = 0;
  int32_t goal_difference = 0;
  uint16_t points = 0;
  /** Last (up to) five results, oldest first: 'W', 'D' or 'L'. */
  std::string form;
};

namespace Standings
{
constexpr uint16_t POINTS_FOR_WIN = 3;
constexpr uint16_t POINTS_FOR_DRAW = 1;
constexpr size_t FORM_LENGTH = 5;

/**
 * @brief Computes a league table from the played league fixtures.
 *
 * The calendar is the single source of truth. With the league's
 * TieBreakRule::GOAL_DIFFERENCE teams are ordered by points, goal
 * difference, goals scored, then a head-to-head mini-league (points, goal
 * difference) among the tied teams; with TieBreakRule::HEAD_TO_HEAD the
 * mini-league comes right after points, followed by overall goal difference
 * and goals scored. Remaining ties fall back to team name, then team ID.
 */
std::vector<StandingRow> compute(const League& league,
                                 const Calendar& calendar,
                                 const GameData& gamedata);
}  // namespace Standings
