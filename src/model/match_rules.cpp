// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "model/match_rules.h"

#include <algorithm>
#include <cmath>

std::string_view matchEventTypeName(MatchEventType type)
{
  switch (type)
  {
    case MatchEventType::INFO:
      return "info";
    case MatchEventType::KICK_OFF:
      return "kick_off";
    case MatchEventType::GOAL:
      return "goal";
    case MatchEventType::OWN_GOAL:
      return "own_goal";
    case MatchEventType::SHOT:
      return "shot";
    case MatchEventType::SAVE:
      return "save";
    case MatchEventType::SHOT_BLOCKED:
      return "shot_blocked";
    case MatchEventType::SHOT_OFF_TARGET:
      return "shot_off_target";
    case MatchEventType::WOODWORK:
      return "woodwork";
    case MatchEventType::FOUL:
      return "foul";
    case MatchEventType::ADVANTAGE:
      return "advantage";
    case MatchEventType::YELLOW_CARD:
      return "yellow_card";
    case MatchEventType::SECOND_YELLOW:
      return "second_yellow";
    case MatchEventType::RED_CARD:
      return "red_card";
    case MatchEventType::INJURY:
      return "injury";
    case MatchEventType::SUBSTITUTION:
      return "substitution";
    case MatchEventType::OFFSIDE:
      return "offside";
    case MatchEventType::CORNER:
      return "corner";
    case MatchEventType::FREE_KICK:
      return "free_kick";
    case MatchEventType::PENALTY:
      return "penalty";
    case MatchEventType::PENALTY_MISSED:
      return "penalty_missed";
    case MatchEventType::THROW_IN:
      return "throw_in";
    case MatchEventType::GOAL_KICK:
      return "goal_kick";
    case MatchEventType::ADDED_TIME:
      return "added_time";
    case MatchEventType::HALF_TIME:
      return "half_time";
    case MatchEventType::SECOND_HALF:
      return "second_half";
    case MatchEventType::FULL_TIME:
      return "full_time";
  }
  return "unknown";
}

namespace MatchRules
{
int computeAddedMinutes(const StoppageLog& log, int period)
{
  using T = MatchTuning::Stoppage;
  const float minutes =
      (period == 1 ? T::FIRST_HALF_BASE_MINUTES : T::SECOND_HALF_BASE_MINUTES) +
      static_cast<float>(log.goals) * T::MINUTES_PER_GOAL +
      static_cast<float>(log.substitutions) * T::MINUTES_PER_SUBSTITUTION +
      static_cast<float>(log.cards) * T::MINUTES_PER_CARD +
      static_cast<float>(log.injuries) * T::MINUTES_PER_INJURY +
      static_cast<float>(log.penalties) * T::MINUTES_PER_PENALTY;
  return std::clamp(static_cast<int>(std::lround(minutes)),
                    T::MIN_ADDED_MINUTES, T::MAX_ADDED_MINUTES);
}

float computeMatchRating(const PlayerMatchStats& stats, bool goalkeeper,
                         bool defender, int teamGoalDifference)
{
  using R = MatchTuning::Rating;
  const int failedPasses =
      std::max(0, stats.passesAttempted - stats.passesCompleted);
  const int shotsOffTarget = std::max(0, stats.shots - stats.shotsOnTarget);
  float rating = R::BASELINE + static_cast<float>(stats.goals) * R::GOAL +
                 static_cast<float>(stats.ownGoals) * R::OWN_GOAL +
                 static_cast<float>(stats.assists) * R::ASSIST +
                 static_cast<float>(stats.shotsOnTarget) * R::SHOT_ON_TARGET +
                 static_cast<float>(shotsOffTarget) * R::SHOT_OFF_TARGET +
                 static_cast<float>(stats.keyPasses) * R::KEY_PASS +
                 static_cast<float>(stats.passesCompleted) * R::COMPLETED_PASS +
                 static_cast<float>(failedPasses) * R::FAILED_PASS +
                 static_cast<float>(stats.tacklesWon) * R::TACKLE_WON +
                 static_cast<float>(stats.interceptions) * R::INTERCEPTION +
                 static_cast<float>(stats.clearances) * R::CLEARANCE +
                 static_cast<float>(stats.aerialDuelsWon) * R::AERIAL_WON +
                 static_cast<float>(stats.saves) * R::SAVE +
                 static_cast<float>(stats.foulsCommitted) * R::FOUL +
                 static_cast<float>(stats.yellowCards) * R::YELLOW_CARD +
                 static_cast<float>(stats.redCards) * R::RED_CARD;

  if (goalkeeper || defender)
  {
    rating +=
        static_cast<float>(stats.goalsConceded) *
        (goalkeeper ? R::GOALKEEPER_GOAL_CONCEDED : R::DEFENDER_GOAL_CONCEDED);
    if (stats.goalsConceded == 0 &&
        stats.minutesPlayed >= R::CLEAN_SHEET_MINIMUM_MINUTES)
    {
      rating +=
          (goalkeeper ? R::CLEAN_SHEET_GOALKEEPER : R::CLEAN_SHEET_DEFENDER) *
          std::min(1.0f, stats.minutesPlayed / R::FULL_MATCH_MINUTES);
    }
  }

  if (teamGoalDifference != 0)
  {
    const float participation =
        std::min(1.0f, stats.minutesPlayed / R::FULL_MATCH_MINUTES);
    rating += (teamGoalDifference > 0 ? R::RESULT_BONUS : -R::RESULT_BONUS) *
              participation;
  }
  return std::clamp(rating, R::MINIMUM, R::MAXIMUM);
}

float playerHeightMetres(std::uint8_t heightCentimetres)
{
  // Unknown or implausible heights fall back to the default height.
  if (heightCentimetres < 150)
    return MatchTuning::Units::DEFAULT_PLAYER_HEIGHT_METRES;
  return static_cast<float>(heightCentimetres) / 100.0f;
}

float headerReachMetres(float heightMetres, float physicality)
{
  using A = MatchTuning::Aerial;
  return heightMetres - A::HEAD_OFFSET_METRES + A::BASE_JUMP_METRES +
         std::clamp(physicality, 0.0f, 1.0f) * A::PHYSICALITY_JUMP_METRES;
}

float goalkeeperReachMetres(float heightMetres, float goalkeeping)
{
  using A = MatchTuning::Aerial;
  return heightMetres * A::GOALKEEPER_ARM_REACH_RATIO +
         A::GOALKEEPER_BASE_JUMP_METRES +
         std::clamp(goalkeeping, 0.0f, 1.0f) * A::GOALKEEPER_SKILL_JUMP_METRES;
}

float aerialDuelStrength(float heightMetres, float physicality,
                         float ballHeightMetres)
{
  const float reach = headerReachMetres(heightMetres, physicality);
  const float margin = reach - ballHeightMetres;
  if (margin < 0.0f) return 0.0f;
  // Being able to meet the ball comfortably matters, but beyond a few
  // centimetres of spare reach it is timing and strength that decide.
  return (0.35f + std::clamp(physicality, 0.0f, 1.0f)) *
         (0.6f + std::min(margin, 0.4f));
}

FoulSanction decideFoulSanction(const FoulContext& context)
{
  using D = MatchTuning::Discipline;
  const float strictness =
      std::clamp(context.strictness, D::MIN_STRICTNESS, D::MAX_STRICTNESS) *
      (context.offenderIsAway ? D::AWAY_CARD_BIAS : D::HOME_CARD_BIAS);

  if (context.severityRoll >= 1.0f - D::SERIOUS_FOUL_PLAY_SHARE * strictness)
    return FoulSanction::RED;
  if (context.denyingGoalChance)
  {
    const float redChance = context.inPenaltyArea
                                ? D::PENALTY_AREA_DOGSO_RED_CHANCE
                                : D::DOGSO_RED_CHANCE;
    if (context.cardRoll < redChance) return FoulSanction::RED;
  }

  const bool reckless = context.severityRoll >= 1.0f - D::RECKLESS_SHARE;
  float yellowChance =
      reckless ? D::RECKLESS_YELLOW_CHANCE : D::CARELESS_YELLOW_CHANCE;
  if (context.tactical || context.denyingGoalChance)
    yellowChance = std::max(yellowChance, D::TACTICAL_YELLOW_CHANCE);
  yellowChance = std::clamp(yellowChance * strictness, 0.0f, 0.95f);
  if (context.cardRoll >= yellowChance) return FoulSanction::NONE;
  return context.offenderAlreadyBooked ? FoulSanction::SECOND_YELLOW
                                       : FoulSanction::YELLOW;
}

float tackleWinChance(const TackleContext& context)
{
  using D = MatchTuning::Defending;
  return std::clamp(
      D::BASE_WIN_CHANCE + context.defending * D::DEFENDING_WIN_BONUS -
          context.dribbling * D::DRIBBLING_WIN_PENALTY +
          (context.defenderPhysicality - context.carrierPhysicality) *
              D::PHYSICALITY_DUEL_WEIGHT +
          context.pressing * D::PRESSING_WIN_EFFECT +
          std::clamp(context.exposure, 0.0f, 1.0f) * D::EXPOSURE_WIN_BONUS -
          (context.shielding
               ? context.carrierPhysicality * D::SHIELD_PHYSICALITY_PENALTY
               : 0.0f) -
          (context.sliding ? D::SLIDE_WIN_PENALTY : 0.0f) -
          (context.fromBehind ? D::FROM_BEHIND_WIN_PENALTY : 0.0f),
      D::MIN_WIN_CHANCE, D::MAX_WIN_CHANCE);
}

float tackleFoulPropensity(const TackleContext& context)
{
  using D = MatchTuning::Defending;
  // Defenders are far more careful inside their own penalty area, a booked
  // player picks his challenges, and a challenge while the ball is away from
  // the attacker's foot rarely catches the man.
  return (D::BASE_FOUL_CHANCE + context.riskTaking * D::RISK_FOUL_BONUS +
          context.pressing * D::PRESSING_FOUL_BONUS +
          (1.0f - context.defending) * D::TECHNIQUE_FOUL_BONUS) *
         (context.defenderBooked ? MatchTuning::Discipline::BOOKED_PLAYER_CAUTION
                                 : 1.0f) *
         (context.inPenaltyArea ? D::PENALTY_AREA_FOUL_SCALE : 1.0f) *
         (context.fromBehind ? D::FROM_BEHIND_FOUL_FACTOR : 1.0f) *
         (context.sliding ? D::SLIDE_FOUL_FACTOR : 1.0f) *
         (1.0f - std::clamp(context.exposure, 0.0f, 1.0f) * D::EXPOSED_FOUL_RELIEF);
}

float staminaDrainPerSecond(float speedRatio, float endurance,
                            float pressingIntensity)
{
  using P = MatchTuning::Fatigue;
  const float ratio = std::clamp(speedRatio, 0.0f, 1.5f);
  const float sprint = std::max(0.0f, ratio - P::SPRINT_THRESHOLD) /
                       (1.0f - P::SPRINT_THRESHOLD);
  const float work =
      P::IDLE_DRAIN + ratio * ratio * P::RUNNING_DRAIN +
      sprint * P::SPRINT_DRAIN +
      std::clamp(pressingIntensity, 0.0f, 1.0f) * P::PRESSING_DRAIN;
  return work * (P::ENDURANCE_BASE -
                 std::clamp(endurance, 0.0f, 1.0f) * P::ENDURANCE_RELIEF);
}
}  // namespace MatchRules
