// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <array>
#include <cstdint>

#include "global/types.h"

/**
 * @enum BoardObjective
 * @brief Season objective set from the club's expected finish (persisted).
 */
enum class BoardObjective : std::uint8_t
{
  WinLeague = 0,
  TopFour,
  TopHalf,
  MidTable,
  AvoidRelegation
};

/** @brief Outcome of a monthly board review. */
enum class BoardReviewOutcome : std::uint8_t
{
  Satisfied,
  Warning,
  Dismissed
};

/**
 * @struct BoardState
 * @brief The board's view of the manager of the managed club.
 */
struct BoardState
{
  static constexpr std::size_t RESULT_WINDOW = 8;

  TeamID team_id = 0;
  std::uint16_t season_year = 0; /*!< Calendar year the season started. */
  BoardObjective objective = BoardObjective::MidTable;
  std::uint8_t expected_position = 0; /*!< From the wage-bill rank. */
  std::uint8_t target_position = 0;   /*!< Worst acceptable finish. */
  float confidence = 60.0f;           /*!< 0-100. */
  std::uint8_t low_reviews = 0;       /*!< Consecutive critical reviews. */
  std::uint8_t league_matches = 0;    /*!< League matches this season. */
  bool dismissed = false;
  std::uint8_t result_count = 0;
  /** Points minus expected points of recent league matches, newest first. */
  std::array<float, RESULT_WINDOW> recent_deltas{};
};

namespace BoardModel
{
/**
 * Objective for a club expected to finish @p expected_position. The
 * expectation comes from the wage-bill rank because wages explain most of
 * the variation in league position (Szymanski).
 */
BoardObjective objectiveFor(int expected_position, int league_size);

/** Worst acceptable league position for @p objective. */
std::uint8_t targetPosition(BoardObjective objective, int league_size);

/** Language key naming @p objective. */
const char* objectiveKey(BoardObjective objective);

/**
 * Expected league points of a match from both line-up strengths (mean
 * overall), including home advantage. [P] Elo-style logistic with a draw
 * band.
 */
float expectedPoints(float own_strength, float opponent_strength, bool home);

/**
 * Records a league result: confidence moves with results relative to
 * expectation over the last eight matches, B = 0.9 B + 0.1 (50 + 25 z).
 */
void recordMatch(BoardState& state, float points, float expected_points);

/**
 * Monthly review of league position (vs target) and finances. Dismissal
 * needs several consecutive critical reviews after enough matches, so only
 * sustained, extreme failure ends the job.
 */
BoardReviewOutcome monthlyReview(BoardState& state, int position,
                                 int league_size, bool negative_balance,
                                 bool over_wage_budget);

/** Season-end adjustment from the final position. */
void seasonReview(BoardState& state, int final_position);
}  // namespace BoardModel
