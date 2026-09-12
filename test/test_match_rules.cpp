// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include <gtest/gtest.h>

#include "model/match_rules.h"

using MatchRules::FoulContext;
using MatchRules::FoulSanction;

TEST(MatchRulesTest, AddedTimeGrowsWithStoppagesAndStaysBounded)
{
  const MatchRules::StoppageLog quiet;
  const int quietFirst = MatchRules::computeAddedMinutes(quiet, 1);
  const int quietSecond = MatchRules::computeAddedMinutes(quiet, 2);
  EXPECT_GE(quietFirst, MatchTuning::Stoppage::MIN_ADDED_MINUTES);
  EXPECT_GE(quietSecond, quietFirst);

  MatchRules::StoppageLog busy;
  busy.goals = 2;
  busy.substitutions = 6;
  busy.cards = 3;
  busy.injuries = 1;
  const int busySecond = MatchRules::computeAddedMinutes(busy, 2);
  EXPECT_GT(busySecond, quietSecond);
  EXPECT_GE(busySecond, 5);
  EXPECT_LE(busySecond, 9);

  MatchRules::StoppageLog chaos;
  chaos.goals = 20;
  chaos.injuries = 20;
  EXPECT_EQ(MatchRules::computeAddedMinutes(chaos, 2),
            MatchTuning::Stoppage::MAX_ADDED_MINUTES);
}

TEST(MatchRulesTest, RatingIsEventDrivenAndBounded)
{
  PlayerMatchStats quiet;
  quiet.minutesPlayed = 90.0f;
  EXPECT_FLOAT_EQ(MatchRules::computeMatchRating(quiet, false, false, 0),
                  MatchTuning::Rating::BASELINE);

  PlayerMatchStats scorer = quiet;
  scorer.goals = 2;
  scorer.assists = 1;
  scorer.shots = 4;
  scorer.shotsOnTarget = 3;
  const float scorerRating =
      MatchRules::computeMatchRating(scorer, false, false, 2);
  EXPECT_GT(scorerRating, 8.0f);

  PlayerMatchStats sentOff = quiet;
  sentOff.redCards = 1;
  sentOff.foulsCommitted = 3;
  EXPECT_LT(MatchRules::computeMatchRating(sentOff, false, false, -1), 5.0f);

  PlayerMatchStats cleanSheetKeeper = quiet;
  cleanSheetKeeper.saves = 4;
  PlayerMatchStats beatenKeeper = quiet;
  beatenKeeper.goalsConceded = 4;
  EXPECT_GT(MatchRules::computeMatchRating(cleanSheetKeeper, true, false, 0),
            MatchRules::computeMatchRating(beatenKeeper, true, false, -4));

  PlayerMatchStats extreme = quiet;
  extreme.goals = 12;
  EXPECT_FLOAT_EQ(MatchRules::computeMatchRating(extreme, false, false, 12),
                  MatchTuning::Rating::MAXIMUM);
  PlayerMatchStats disaster = quiet;
  disaster.ownGoals = 5;
  disaster.redCards = 1;
  EXPECT_FLOAT_EQ(MatchRules::computeMatchRating(disaster, false, true, -5),
                  MatchTuning::Rating::MINIMUM);
}

TEST(MatchRulesTest, AerialReachDependsOnHeightAndPhysicality)
{
  EXPECT_FLOAT_EQ(MatchRules::playerHeightMetres(0),
                  MatchTuning::Units::DEFAULT_PLAYER_HEIGHT_METRES);
  EXPECT_FLOAT_EQ(MatchRules::playerHeightMetres(191), 1.91f);

  const float shortWeak = MatchRules::headerReachMetres(1.70f, 0.3f);
  const float tallWeak = MatchRules::headerReachMetres(1.92f, 0.3f);
  const float tallStrong = MatchRules::headerReachMetres(1.92f, 0.9f);
  EXPECT_LT(shortWeak, tallWeak);
  EXPECT_LT(tallWeak, tallStrong);
  EXPECT_GT(tallStrong, 2.3f);
  EXPECT_LT(tallStrong, 3.0f);
  EXPECT_GT(MatchRules::goalkeeperReachMetres(1.90f, 0.7f), tallStrong);

  EXPECT_FLOAT_EQ(MatchRules::aerialDuelStrength(1.70f, 0.5f, 3.2f), 0.0f);
  EXPECT_GT(MatchRules::aerialDuelStrength(1.92f, 0.9f, 2.2f),
            MatchRules::aerialDuelStrength(1.75f, 0.4f, 2.2f));
}

TEST(MatchRulesTest, FoulSanctionsFollowTheLaws)
{
  FoulContext careless;
  careless.severityRoll = 0.2f;
  careless.cardRoll = 0.9f;
  EXPECT_EQ(MatchRules::decideFoulSanction(careless), FoulSanction::NONE);

  FoulContext reckless = careless;
  reckless.severityRoll = 0.95f;
  reckless.cardRoll = 0.1f;
  EXPECT_EQ(MatchRules::decideFoulSanction(reckless), FoulSanction::YELLOW);
  reckless.offenderAlreadyBooked = true;
  EXPECT_EQ(MatchRules::decideFoulSanction(reckless),
            FoulSanction::SECOND_YELLOW);

  FoulContext violent = careless;
  violent.severityRoll = 0.9999f;
  EXPECT_EQ(MatchRules::decideFoulSanction(violent), FoulSanction::RED);

  // Denying an obvious goal-scoring opportunity is a sending-off outside the
  // area, but usually "only" a caution inside it (double punishment rule).
  using D = MatchTuning::Discipline;
  static_assert(D::PENALTY_AREA_DOGSO_RED_CHANCE <
                D::TACTICAL_YELLOW_CHANCE * D::HOME_CARD_BIAS);
  FoulContext lastMan = careless;
  lastMan.denyingGoalChance = true;
  lastMan.cardRoll = (D::PENALTY_AREA_DOGSO_RED_CHANCE +
                      D::TACTICAL_YELLOW_CHANCE * D::HOME_CARD_BIAS) *
                     0.5f;
  EXPECT_EQ(MatchRules::decideFoulSanction(lastMan), FoulSanction::RED);
  lastMan.inPenaltyArea = true;
  EXPECT_EQ(MatchRules::decideFoulSanction(lastMan), FoulSanction::YELLOW);

  // A stricter referee cautions for rolls a lenient one lets go.
  FoulContext tactical = careless;
  tactical.tactical = true;
  tactical.cardRoll = D::TACTICAL_YELLOW_CHANCE * D::HOME_CARD_BIAS;
  tactical.strictness = 0.8f;
  EXPECT_EQ(MatchRules::decideFoulSanction(tactical), FoulSanction::NONE);
  tactical.strictness = 1.3f;
  EXPECT_EQ(MatchRules::decideFoulSanction(tactical), FoulSanction::YELLOW);
}

TEST(MatchRulesTest, StaminaDrainFollowsIntensityAndEndurance)
{
  const float walking = MatchRules::staminaDrainPerSecond(0.2f, 0.5f, 0.0f);
  const float running = MatchRules::staminaDrainPerSecond(0.6f, 0.5f, 0.0f);
  const float sprinting = MatchRules::staminaDrainPerSecond(1.0f, 0.5f, 0.0f);
  EXPECT_LT(walking, running);
  EXPECT_LT(running, sprinting);
  EXPECT_GT(MatchRules::staminaDrainPerSecond(0.6f, 0.2f, 0.0f),
            MatchRules::staminaDrainPerSecond(0.6f, 0.9f, 0.0f));
  EXPECT_GT(MatchRules::staminaDrainPerSecond(0.6f, 0.5f, 1.0f), running);
}
