// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>

#include "model/match_events.h"

/**
 * Pure, deterministic rules used by the match engine. Everything here is a
 * function of its arguments (random rolls are passed in), so each rule can be
 * unit tested without running a match.
 */
namespace MatchRules
{
/** Stoppages recorded during one half; drives the added time. */
struct StoppageLog
{
  int goals = 0;
  int substitutions = 0;
  int cards = 0;
  int injuries = 0;
  int penalties = 0;
};

/** Whole added minutes for a half (period 1 or 2) from its stoppages. */
int computeAddedMinutes(const StoppageLog& log, int period);

/**
 * Event-driven match rating: 6.0 baseline, bounded to [3, 10].
 * `teamGoalDifference` is the player's team score minus the opponent's.
 */
float computeMatchRating(const PlayerMatchStats& stats, bool goalkeeper,
                         bool defender, int teamGoalDifference);

/** Player height in metres; unknown (0) heights fall back to the default. */
float playerHeightMetres(std::uint8_t heightCentimetres);

/** Highest ball height (metres) an outfield player can head. */
float headerReachMetres(float heightMetres, float physicality);

/** Highest ball height (metres) a goalkeeper can claim with the hands. */
float goalkeeperReachMetres(float heightMetres, float goalkeeping);

/** Relative strength of a player in an aerial duel for a ball at a height. */
float aerialDuelStrength(float heightMetres, float physicality,
                         float ballHeightMetres);

enum class FoulSanction : std::uint8_t
{
  NONE,
  YELLOW,
  SECOND_YELLOW,
  RED
};

struct FoulContext
{
  /** Uniform rolls in [0, 1). */
  float severityRoll = 0.0f;
  float cardRoll = 0.0f;
  /** Per-match referee strictness, ~N(1, 0.13). */
  float strictness = 1.0f;
  /** The foul stopped a promising attack (tactical foul). */
  bool tactical = false;
  /** Denied an obvious goal-scoring opportunity. */
  bool denyingGoalChance = false;
  /** The foul happened inside the offender's penalty area. */
  bool inPenaltyArea = false;
  bool offenderAlreadyBooked = false;
  bool offenderIsAway = false;
};

FoulSanction decideFoulSanction(const FoulContext& context);

/** Everything that decides a ground challenge on a dribbler. */
struct TackleContext
{
  float defending = 0.5f;
  float dribbling = 0.5f;
  float defenderPhysicality = 0.5f;
  float carrierPhysicality = 0.5f;
  /** Team pressing instruction in [0, 1]. */
  float pressing = 0.5f;
  /** Team risk-taking instruction in [0, 1]. */
  float riskTaking = 0.5f;
  /** Share of the touch the ball is still away from the carrier's foot. */
  float exposure = 0.0f;
  /** The carrier is shielding the ball at walking pace. */
  bool shielding = false;
  bool sliding = false;
  bool fromBehind = false;
  bool inPenaltyArea = false;
  bool defenderBooked = false;
};

/** Chance that a challenge reaches the ball first. */
float tackleWinChance(const TackleContext& context);

/**
 * Propensity for the challenge to be a foul: applied in full to a missed
 * challenge and scaled down for one that also reached the ball.
 */
float tackleFoulPropensity(const TackleContext& context);

/**
 * Match condition drained per second for a player moving at `speedRatio`
 * of their fresh top speed. `endurance` is the Stamina attribute in [0, 1].
 */
float staminaDrainPerSecond(float speedRatio, float endurance,
                            float pressingIntensity);
}  // namespace MatchRules
