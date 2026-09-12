// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "model/board.h"

#include <algorithm>
#include <cmath>

#include "model/world_tuning.h"

namespace
{
// Home advantage expressed in overall-rating points. [P]
constexpr float HOME_ADVANTAGE = 2.5f;
// Rating difference that moves the win probability by one logistic unit. [P]
constexpr float STRENGTH_SCALE = 4.5f;
// Draw probability for evenly matched sides. [P] ~27% of league matches.
constexpr float EVEN_DRAW_PROBABILITY = 0.27f;
// Standard deviation of (points - expected points) per match. [P]
constexpr float POINTS_STDDEV = 1.2f;

float blend(float confidence, float signal)
{
  return std::clamp(0.9f * confidence + 0.1f * signal, 0.0f, 100.0f);
}
}  // namespace

namespace BoardModel
{
BoardObjective objectiveFor(int expected_position, int league_size)
{
  if (expected_position <= 1) return BoardObjective::WinLeague;
  if (expected_position <= 4) return BoardObjective::TopFour;
  if (expected_position <= league_size / 2) return BoardObjective::TopHalf;
  if (expected_position <= league_size - 5) return BoardObjective::MidTable;
  return BoardObjective::AvoidRelegation;
}

std::uint8_t targetPosition(BoardObjective objective, int league_size)
{
  int target = league_size;
  switch (objective)
  {
    case BoardObjective::WinLeague:
      target = 2;  // Finishing second still keeps the board broadly happy.
      break;
    case BoardObjective::TopFour:
      target = 4;
      break;
    case BoardObjective::TopHalf:
      target = league_size / 2;
      break;
    case BoardObjective::MidTable:
      target = league_size - 5;
      break;
    case BoardObjective::AvoidRelegation:
      target = league_size - 3;
      break;
  }
  return static_cast<std::uint8_t>(std::clamp(target, 1, 255));
}

const char* objectiveKey(BoardObjective objective)
{
  switch (objective)
  {
    case BoardObjective::WinLeague:
      return "BOARD_OBJ_WIN_LEAGUE";
    case BoardObjective::TopFour:
      return "BOARD_OBJ_TOP_FOUR";
    case BoardObjective::TopHalf:
      return "BOARD_OBJ_TOP_HALF";
    case BoardObjective::MidTable:
      return "BOARD_OBJ_MID_TABLE";
    case BoardObjective::AvoidRelegation:
      return "BOARD_OBJ_AVOID_RELEGATION";
  }
  return "BOARD_OBJ_MID_TABLE";
}

float expectedPoints(float own_strength, float opponent_strength, bool home)
{
  const float diff = own_strength - opponent_strength +
                     (home ? HOME_ADVANTAGE : -HOME_ADVANTAGE);
  const float draw =
      EVEN_DRAW_PROBABILITY * std::exp(-(diff / 10.0f) * (diff / 10.0f));
  const float win = (1.0f - draw) / (1.0f + std::exp(-diff / STRENGTH_SCALE));
  return 3.0f * win + draw;
}

void recordMatch(BoardState& state, float points, float expected_points)
{
  auto& deltas = state.recent_deltas;
  std::shift_right(deltas.begin(), deltas.end(), 1);
  deltas[0] = points - expected_points;
  if (state.result_count < deltas.size()) ++state.result_count;
  ++state.league_matches;

  float total = 0.0f;
  for (std::size_t i = 0; i < state.result_count; ++i) total += deltas[i];
  const float z = std::clamp(
      total /
          (POINTS_STDDEV * std::sqrt(static_cast<float>(state.result_count))),
      -2.0f, 2.0f);
  state.confidence = blend(state.confidence, 50.0f + 25.0f * z);
}

BoardReviewOutcome monthlyReview(BoardState& state, int position,
                                 int league_size, bool negative_balance,
                                 bool over_wage_budget)
{
  using Tuning = WorldTuning::Board;
  if (state.dismissed) return BoardReviewOutcome::Dismissed;

  const float quarter = std::max(1.0f, static_cast<float>(league_size) / 4.0f);
  float signal = 50.0f;
  if (state.league_matches > 0)
  {
    signal += 25.0f *
              std::clamp(static_cast<float>(state.target_position - position) /
                             quarter,
                         -2.0f, 2.0f);
  }
  if (negative_balance) signal -= 25.0f;
  if (over_wage_budget) signal -= 12.0f;
  state.confidence = blend(state.confidence, signal);

  if (state.confidence < Tuning::DISMISSAL_THRESHOLD)
    ++state.low_reviews;
  else
    state.low_reviews = 0;

  if (state.low_reviews >= Tuning::DISMISSAL_REVIEWS &&
      state.league_matches >= Tuning::MIN_MATCHES_FOR_DISMISSAL)
  {
    state.dismissed = true;
    return BoardReviewOutcome::Dismissed;
  }
  return state.confidence < Tuning::WARNING_THRESHOLD
             ? BoardReviewOutcome::Warning
             : BoardReviewOutcome::Satisfied;
}

void seasonReview(BoardState& state, int final_position)
{
  const float margin =
      static_cast<float>(state.target_position - final_position);
  state.confidence =
      std::clamp(state.confidence + std::clamp(margin * 3.0f, -20.0f, 15.0f),
                 0.0f, 100.0f);
}
}  // namespace BoardModel
