// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "global/runtime_paths.h"
#include "gui/render/match_render_snapshot.h"
#include "model/match_commentary.h"
#include "model/match_engine.h"
#include "model/player.h"
#include "model/team.h"

namespace
{
StatsConfig createStatsConfig()
{
  StatsConfig config;
  config.possible_stats = {"Pace",      "Shooting",  "Passing",
                           "Dribbling", "Defending", "Physicality",
                           "Stamina",   "Vision",    "Goalkeeping"};
  config.role_focus["Goalkeeper"] = {{"Goalkeeping", "Vision", "Physicality"},
                                     {0.7, 0.2, 0.1}};
  config.role_focus["Defender"] = {
      {"Defending", "Physicality", "Pace", "Vision"}, {0.4, 0.3, 0.15, 0.15}};
  config.role_focus["Midfielder"] = {
      {"Passing", "Vision", "Stamina", "Dribbling"}, {0.3, 0.3, 0.2, 0.2}};
  config.role_focus["Striker"] = {
      {"Shooting", "Pace", "Dribbling", "Physicality"}, {0.4, 0.2, 0.2, 0.2}};
  return config;
}

Team createDummyTeam(TeamID id, const std::string& name, int rating,
                     std::vector<std::unique_ptr<Player>>& players)
{
  Team team(id, 1, name, 50'000'000, {}, Strategy{}, Lineup{});
  static constexpr PlayerRole ROLES[11] = {
      PlayerRole::GK, PlayerRole::LB, PlayerRole::CB, PlayerRole::CB,
      PlayerRole::RB, PlayerRole::CM, PlayerRole::CM, PlayerRole::LW,
      PlayerRole::RW, PlayerRole::ST, PlayerRole::ST};
  static constexpr Vector2F POSITIONS[11] = {
      {0.04f, 0.50f}, {0.18f, 0.12f}, {0.18f, 0.38f}, {0.18f, 0.62f},
      {0.18f, 0.88f}, {0.43f, 0.35f}, {0.43f, 0.65f}, {0.68f, 0.16f},
      {0.68f, 0.84f}, {0.78f, 0.38f}, {0.78f, 0.62f}};

  for (uint32_t index = 0; index < 11; ++index)
  {
    const float value = static_cast<float>(rating);
    const std::map<std::string, float> stats = {
        {"Pace", value},      {"Shooting", value},  {"Passing", value},
        {"Dribbling", value}, {"Defending", value}, {"Physicality", value},
        {"Stamina", value},   {"Vision", value},    {"Goalkeeping", value}};
    auto player = std::make_unique<Player>(
        static_cast<PlayerID>(id) * 100U + index, id, "First",
        std::to_string(index), ROLES[index], Language::EN, 100'000, 0, 25, 3,
        180, Foot::Right, stats);
    if (ROLES[index] == PlayerRole::GK)
      team.getLineup().setGoalkeeper(player.get());
    else
      team.getLineup().addOutfieldPlayer(player.get(), POSITIONS[index]);
    players.push_back(std::move(player));
  }
  return team;
}

/** Plays a match through the live update() path at a fixed frame length. */
void simulateToFullTime(MatchEngine& engine, float frameDelta = 0.05f)
{
  // A match lasts at most ~7,200 simulated seconds.
  const auto maxFrames = static_cast<int>(8'000.0f / frameDelta);
  for (int frame = 0;
       frame < maxFrames && engine.getState() != MatchState::FULL_TIME; ++frame)
  {
    engine.update(frameDelta);
  }
}
}  // namespace

TEST(MatchEngineTest, CompletesARealisticMatch)
{
  std::vector<std::unique_ptr<Player>> players;
  Team home = createDummyTeam(1, "Home", 65, players);
  Team away = createDummyTeam(2, "Away", 65, players);
  const StatsConfig config = createStatsConfig();
  MatchEngine engine(home.getLineup(), away.getLineup(), home.getStrategy(),
                     away.getStrategy(), config, 42);

  simulateToFullTime(engine);

  EXPECT_EQ(engine.getState(), MatchState::FULL_TIME);
  // Real match time: two 45-minute halves plus stoppage and added time.
  EXPECT_GE(engine.getSimulatedSeconds(), 5'400.0);
  EXPECT_LE(engine.getSimulatedSeconds(), 7'300.0);
  // The match ends once the second-half added time has been played.
  const int addedMinutes = engine.getAddedMinutes(2);
  EXPECT_GE(addedMinutes, MatchTuning::Stoppage::MIN_ADDED_MINUTES);
  EXPECT_LE(addedMinutes, MatchTuning::Stoppage::MAX_ADDED_MINUTES);
  EXPECT_GE(engine.getMatchTimeMinutes(), 90.0f + addedMinutes);
  EXPECT_LE(engine.getMatchTimeMinutes(),
            90.0f + MatchTuning::Stoppage::MAX_ADDED_MINUTES +
                MatchTuning::Stoppage::MAX_OVERRUN_MINUTES);
  EXPECT_GE(engine.getAddedMinutes(1),
            MatchTuning::Stoppage::MIN_ADDED_MINUTES);
  ASSERT_FALSE(engine.getEvents().empty());
  EXPECT_EQ(engine.getEvents().back().type, MatchEventType::FULL_TIME);
  const MatchStats& stats = engine.getStats();
  EXPECT_GE(stats.homeShots + stats.awayShots, 8);
  EXPECT_LE(stats.homeShots + stats.awayShots, 45);
  EXPECT_GE(stats.homePassesAttempted + stats.awayPassesAttempted, 400);
  // Goals are shots on target, except own goals credited to the other side.
  int homeOwnGoalsFor = 0;
  int awayOwnGoalsFor = 0;
  for (const PlayerMatchStats& entry : engine.getPlayerStats())
    (entry.isHomeTeam ? awayOwnGoalsFor : homeOwnGoalsFor) += entry.ownGoals;
  EXPECT_LE(engine.getHomeScore(), stats.homeOnTarget + homeOwnGoalsFor);
  EXPECT_LE(engine.getAwayScore(), stats.awayOnTarget + awayOwnGoalsFor);
  EXPECT_NEAR(stats.homePossession + stats.awayPossession, 100.0f, 0.01f);
  EXPECT_NE(engine.getDebugSnapshotJson().find("\"full_time\""),
            std::string::npos);

  const auto snapshotPath = RuntimePaths::capturePath("engine.json");
  std::filesystem::remove(snapshotPath);
  ASSERT_TRUE(engine.writeDebugSnapshot(snapshotPath.string()));
  EXPECT_GT(std::filesystem::file_size(snapshotPath), 100u);
}

TEST(MatchEngineTest, InterpolationSnapshotTracksPreviousFixedStep)
{
  std::vector<std::unique_ptr<Player>> players;
  Team home = createDummyTeam(1, "Home", 65, players);
  Team away = createDummyTeam(2, "Away", 65, players);
  const StatsConfig config = createStatsConfig();
  MatchEngine engine(home.getLineup(), away.getLineup(), home.getStrategy(),
                     away.getStrategy(), config, 77);

  const auto& initial = engine.getPlayers();
  std::vector<Vector2F> before;
  for (const auto& matchPlayer : initial)
  {
    before.push_back(matchPlayer.position);
  }

  // A fresh engine mirrors current positions and reports a zero alpha.
  const MatchRenderSnapshot initialSnapshot = buildMatchRenderSnapshot(engine);
  ASSERT_EQ(initialSnapshot.players.size(), before.size());
  EXPECT_EQ(initialSnapshot.interpolationAlpha, 0.0f);
  EXPECT_FLOAT_EQ(initialSnapshot.ball.currentPosition.x,
                  engine.getBall().position.x);
  EXPECT_FLOAT_EQ(initialSnapshot.ball.currentPosition.y,
                  engine.getBall().position.y);
  for (std::size_t index = 0; index < before.size(); ++index)
  {
    EXPECT_FLOAT_EQ(initialSnapshot.players[index].currentPosition.x,
                    before[index].x);
    EXPECT_FLOAT_EQ(initialSnapshot.players[index].currentPosition.y,
                    before[index].y);
    EXPECT_FLOAT_EQ(initialSnapshot.players[index].previousPosition.x,
                    before[index].x);
    EXPECT_FLOAT_EQ(initialSnapshot.players[index].previousPosition.y,
                    before[index].y);
  }

  // One fixed step plus a partial frame: previous positions now equal the
  // pre-step positions and the alpha lies strictly inside (0, 1).
  const Vector2F ballBefore = engine.getBall().position;
  engine.update(MatchTuning::Timing::FIXED_STEP_SECONDS + 0.005f);
  const auto& previous = engine.getPreviousPlayerPositions();
  ASSERT_EQ(previous.size(), before.size());
  for (std::size_t index = 0; index < before.size(); ++index)
  {
    EXPECT_FLOAT_EQ(previous[index].x, before[index].x);
    EXPECT_FLOAT_EQ(previous[index].y, before[index].y);
  }
  EXPECT_FLOAT_EQ(engine.getPreviousBallPosition().x, ballBefore.x);
  EXPECT_FLOAT_EQ(engine.getPreviousBallPosition().y, ballBefore.y);
  EXPECT_GT(engine.getInterpolationAlpha(), 0.0f);
  EXPECT_LT(engine.getInterpolationAlpha(), 1.0f);

  // The rebuilt snapshot carries the same interpolation endpoints.
  const MatchRenderSnapshot advancedSnapshot = buildMatchRenderSnapshot(engine);
  EXPECT_EQ(advancedSnapshot.interpolationAlpha,
            engine.getInterpolationAlpha());
  for (std::size_t index = 0; index < before.size(); ++index)
  {
    EXPECT_FLOAT_EQ(advancedSnapshot.players[index].previousPosition.x,
                    before[index].x);
  }
}

TEST(MatchEngineTest, SameSeedIsIndependentOfRenderFrameRate)
{
  std::vector<std::unique_ptr<Player>> players;
  Team home = createDummyTeam(1, "Home", 65, players);
  Team away = createDummyTeam(2, "Away", 65, players);
  const StatsConfig config = createStatsConfig();
  MatchEngine fine(home.getLineup(), away.getLineup(), home.getStrategy(),
                   away.getStrategy(), config, 1234);
  MatchEngine coarse(home.getLineup(), away.getLineup(), home.getStrategy(),
                     away.getStrategy(), config, 1234);

  simulateToFullTime(fine, 0.02f);
  simulateToFullTime(coarse, 0.10f);

  EXPECT_EQ(fine.getHomeScore(), coarse.getHomeScore());
  EXPECT_EQ(fine.getAwayScore(), coarse.getAwayScore());
  EXPECT_EQ(fine.getStats().homeShots, coarse.getStats().homeShots);
  EXPECT_EQ(fine.getStats().awayShots, coarse.getStats().awayShots);
  EXPECT_EQ(fine.getStats().homePassesCompleted,
            coarse.getStats().homePassesCompleted);
  EXPECT_EQ(fine.getStats().awayPassesCompleted,
            coarse.getStats().awayPassesCompleted);
}

TEST(MatchEngineTest, DelayedRenderFrameHasBoundedCatchUpWork)
{
  std::vector<std::unique_ptr<Player>> players;
  Team home = createDummyTeam(1, "Home", 65, players);
  Team away = createDummyTeam(2, "Away", 65, players);
  const StatsConfig config = createStatsConfig();
  MatchEngine engine(home.getLineup(), away.getLineup(), home.getStrategy(),
                     away.getStrategy(), config, 9876);

  engine.update(10.0f);

  EXPECT_EQ(engine.getLastUpdateStepCount(),
            MatchTuning::Timing::MAX_FIXED_STEPS_PER_UPDATE);
  EXPECT_GT(engine.getDroppedSimulationSteps(), 0u);
  EXPECT_NE(engine.getState(), MatchState::FULL_TIME);
  for (const MatchPlayer& player : engine.getPlayers())
  {
    EXPECT_TRUE(std::isfinite(player.position.x));
    EXPECT_TRUE(std::isfinite(player.position.y));
  }
}

TEST(MatchEngineTest, LivePlayAlwaysAssignsPressureOrBallRecovery)
{
  std::vector<std::unique_ptr<Player>> players;
  Team home = createDummyTeam(1, "Home", 65, players);
  Team away = createDummyTeam(2, "Away", 65, players);
  const StatsConfig config = createStatsConfig();
  MatchEngine engine(home.getLineup(), away.getLineup(), home.getStrategy(),
                     away.getStrategy(), config, 2468);
  EXPECT_EQ(engine.getHomePhase(), TeamPhase::SET_PIECE);
  EXPECT_EQ(engine.getAwayPhase(), TeamPhase::SET_PIECE);

  constexpr int MAX_KICK_OFF_FRAMES = 100;
  constexpr float TEST_FRAME_SECONDS = 0.05f;
  for (int frame = 0;
       frame < MAX_KICK_OFF_FRAMES && engine.getState() != MatchState::PLAYING;
       ++frame)
  {
    engine.update(TEST_FRAME_SECONDS);
  }
  ASSERT_EQ(engine.getState(), MatchState::PLAYING);
  engine.update(MatchTuning::Timing::FIXED_STEP_SECONDS);
  EXPECT_NE(engine.getLastPassDecision().receiverId, 0u);
  EXPECT_GT(engine.getLastPassDecision().completionProbability, 0.0f);
  EXPECT_LE(engine.getLastPassDecision().completionProbability, 1.0f);
  EXPECT_GE(engine.getLastPassDecision().targetPoint.x, 0.0f);
  EXPECT_LE(engine.getLastPassDecision().targetPoint.x, 1.0f);
  EXPECT_GE(engine.getLastPassDecision().targetPoint.y, 0.0f);
  EXPECT_LE(engine.getLastPassDecision().targetPoint.y, 1.0f);
  EXPECT_NE(engine.getDebugSnapshotJson().find("\"last_pass\":{"),
            std::string::npos);
  EXPECT_NE(engine.getDebugSnapshotJson().find("\"completion_probability\":"),
            std::string::npos);
  EXPECT_NE(engine.getDebugSnapshotJson().find("\"team_phase\":{"),
            std::string::npos);

  bool homeEngagesBall = false;
  bool awayEngagesBall = false;
  for (const MatchPlayer& player : engine.getPlayers())
  {
    const bool engagesBall = player.intent == PlayerIntent::PRESS_BALL ||
                             player.intent == PlayerIntent::CLAIM_LOOSE_BALL ||
                             player.intent == PlayerIntent::RECEIVE_PASS;
    if (player.isHomeTeam)
      homeEngagesBall = homeEngagesBall || engagesBall;
    else
      awayEngagesBall = awayEngagesBall || engagesBall;
  }

  if (engine.getBall().possessedBy)
  {
    const bool homeHasBall = std::ranges::any_of(
        engine.getPlayers(),
        [&engine](const MatchPlayer& player)
        {
          return player.player == engine.getBall().possessedBy &&
                 player.isHomeTeam;
        });
    EXPECT_TRUE(homeHasBall ? awayEngagesBall : homeEngagesBall);
    EXPECT_EQ(homeHasBall ? engine.getAwayPhase() : engine.getHomePhase(),
              TeamPhase::DEFENSIVE_BLOCK);
  }
  else
  {
    EXPECT_TRUE(homeEngagesBall);
    EXPECT_TRUE(awayEngagesBall);
    const bool attackingHome = engine.getBall().passByHome;
    if (engine.getTransitionSecondsRemaining() > 0.0f)
    {
      EXPECT_EQ(attackingHome ? engine.getHomePhase() : engine.getAwayPhase(),
                TeamPhase::ATTACKING_TRANSITION);
      EXPECT_EQ(attackingHome ? engine.getAwayPhase() : engine.getHomePhase(),
                TeamPhase::DEFENSIVE_TRANSITION);
    }
    else
    {
      const TeamPhase attackingPhase =
          attackingHome ? engine.getHomePhase() : engine.getAwayPhase();
      EXPECT_TRUE(attackingPhase == TeamPhase::POSSESSION ||
                  attackingPhase == TeamPhase::FINAL_THIRD);
      EXPECT_EQ(attackingHome ? engine.getAwayPhase() : engine.getHomePhase(),
                TeamPhase::DEFENSIVE_BLOCK);
    }

    const bool attackingTeamKeepsSupporting = std::ranges::any_of(
        engine.getPlayers(),
        [attackingHome](const MatchPlayer& player)
        {
          return player.isHomeTeam == attackingHome &&
                 (player.intent == PlayerIntent::RUN_IN_BEHIND ||
                  player.intent == PlayerIntent::OFFER_SUPPORT);
        });
    EXPECT_TRUE(attackingTeamKeepsSupporting);
  }
}

TEST(MatchEngineTest, ControlledPassDoesNotInventAPossessionTransition)
{
  std::vector<std::unique_ptr<Player>> players;
  Team home = createDummyTeam(1, "Home", 70, players);
  Team away = createDummyTeam(2, "Away", 70, players);
  const StatsConfig config = createStatsConfig();
  MatchEngine engine(home.getLineup(), away.getLineup(), home.getStrategy(),
                     away.getStrategy(), config, 1122);

  constexpr float TEST_FRAME_SECONDS = MatchTuning::Timing::FIXED_STEP_SECONDS;
  constexpr int MAX_OBSERVATION_FRAMES = 1'200;
  bool inspectedControlledPass = false;
  for (int frame = 0; frame < MAX_OBSERVATION_FRAMES; ++frame)
  {
    engine.update(TEST_FRAME_SECONDS);
    const MatchBall& matchBall = engine.getBall();
    if (engine.getState() != MatchState::PLAYING || !matchBall.isPass ||
        engine.getTransitionSecondsRemaining() > 0.0f)
    {
      continue;
    }

    const TeamPhase attackingPhase =
        matchBall.passByHome ? engine.getHomePhase() : engine.getAwayPhase();
    const TeamPhase defendingPhase =
        matchBall.passByHome ? engine.getAwayPhase() : engine.getHomePhase();
    EXPECT_TRUE(attackingPhase == TeamPhase::POSSESSION ||
                attackingPhase == TeamPhase::FINAL_THIRD);
    EXPECT_EQ(defendingPhase, TeamPhase::DEFENSIVE_BLOCK);

    int attackingPlayersRecoveringShape = 0;
    for (const MatchPlayer& player : engine.getPlayers())
    {
      if (player.isHomeTeam == matchBall.passByHome &&
          player.player->getRole() != PlayerRole::GK &&
          player.intent == PlayerIntent::RECOVER_SHAPE)
      {
        ++attackingPlayersRecoveringShape;
      }
    }
    EXPECT_EQ(attackingPlayersRecoveringShape, 0)
        << "A controlled pass must preserve attacking support lanes";
    inspectedControlledPass = true;
    break;
  }
  EXPECT_TRUE(inspectedControlledPass);
}

TEST(MatchEngineTest, TurnoverCreatesTimedTeamTransitions)
{
  std::vector<std::unique_ptr<Player>> players;
  Team home = createDummyTeam(1, "Home", 70, players);
  Team away = createDummyTeam(2, "Away", 70, players);
  const StatsConfig config = createStatsConfig();
  MatchEngine engine(home.getLineup(), away.getLineup(), home.getStrategy(),
                     away.getStrategy(), config, 3344);

  constexpr float TEST_FRAME_SECONDS = 0.05f;
  constexpr int MAX_OBSERVATION_FRAMES = 12'000;
  bool observedTransition = false;
  for (int frame = 0; frame < MAX_OBSERVATION_FRAMES; ++frame)
  {
    engine.update(TEST_FRAME_SECONDS);
    if (engine.getState() != MatchState::PLAYING ||
        !engine.getBall().possessedBy ||
        engine.getTransitionSecondsRemaining() <= 0.0f)
    {
      continue;
    }

    const auto carrier = std::ranges::find_if(
        engine.getPlayers(), [&engine](const MatchPlayer& player)
        { return player.player == engine.getBall().possessedBy; });
    ASSERT_NE(carrier, engine.getPlayers().end());
    EXPECT_EQ(
        carrier->isHomeTeam ? engine.getHomePhase() : engine.getAwayPhase(),
        TeamPhase::ATTACKING_TRANSITION);
    EXPECT_EQ(
        carrier->isHomeTeam ? engine.getAwayPhase() : engine.getHomePhase(),
        TeamPhase::DEFENSIVE_TRANSITION);
    EXPECT_LE(engine.getTransitionSecondsRemaining(),
              MatchTuning::Timing::POSSESSION_TRANSITION_SECONDS);
    observedTransition = true;
    break;
  }
  EXPECT_TRUE(observedTransition);
}

TEST(MatchEngineTest, PossessionBuildsDistinctSupportAngles)
{
  std::vector<std::unique_ptr<Player>> players;
  Team home = createDummyTeam(1, "Home", 70, players);
  Team away = createDummyTeam(2, "Away", 70, players);
  const StatsConfig config = createStatsConfig();
  MatchEngine engine(home.getLineup(), away.getLineup(), home.getStrategy(),
                     away.getStrategy(), config, 5566);

  constexpr float TEST_FRAME_SECONDS = 0.05f;
  constexpr int MAX_OBSERVATION_FRAMES = 1'200;
  constexpr float MAX_SUPPORT_DISTANCE = 0.24f;
  constexpr float MIN_LATERAL_ANGLE = 0.015f;
  bool observedSupportAngles = false;
  for (int frame = 0; frame < MAX_OBSERVATION_FRAMES; ++frame)
  {
    engine.update(TEST_FRAME_SECONDS);
    if (engine.getState() != MatchState::PLAYING ||
        !engine.getBall().possessedBy)
    {
      continue;
    }

    const auto carrier = std::ranges::find_if(
        engine.getPlayers(), [&engine](const MatchPlayer& player)
        { return player.player == engine.getBall().possessedBy; });
    ASSERT_NE(carrier, engine.getPlayers().end());
    int nearbySupporters = 0;
    bool supportAbove = false;
    bool supportBelow = false;
    for (const MatchPlayer& player : engine.getPlayers())
    {
      if (player.isHomeTeam != carrier->isHomeTeam ||
          player.intent != PlayerIntent::OFFER_SUPPORT)
      {
        continue;
      }
      const float targetDistance =
          std::hypot(player.movementTarget.x - carrier->position.x,
                     player.movementTarget.y - carrier->position.y);
      if (targetDistance > MAX_SUPPORT_DISTANCE) continue;
      ++nearbySupporters;
      const float lateralOffset = player.movementTarget.y - carrier->position.y;
      supportAbove = supportAbove || lateralOffset < -MIN_LATERAL_ANGLE;
      supportBelow = supportBelow || lateralOffset > MIN_LATERAL_ANGLE;
    }
    if (nearbySupporters >= 2 && supportAbove && supportBelow)
    {
      observedSupportAngles = true;
      break;
    }
  }
  EXPECT_TRUE(observedSupportAngles)
      << "Possession should create nearby options on both sides of the carrier";
}

TEST(MatchEngineTest, FinalThirdPossessionKeepsRestDefenseBehindTheBall)
{
  std::vector<std::unique_ptr<Player>> players;
  Team home = createDummyTeam(1, "Home", 70, players);
  Team away = createDummyTeam(2, "Away", 70, players);
  const StatsConfig config = createStatsConfig();
  MatchEngine engine(home.getLineup(), away.getLineup(), home.getStrategy(),
                     away.getStrategy(), config, 7788);

  constexpr float TEST_FRAME_SECONDS = 0.05f;
  constexpr int MAX_OBSERVATION_FRAMES = 12'000;
  constexpr int MINIMUM_REST_DEFENDERS = 2;
  constexpr float MINIMUM_GOAL_SIDE_GAP = 0.12f;
  bool inspectedFinalThird = false;
  for (int frame = 0; frame < MAX_OBSERVATION_FRAMES; ++frame)
  {
    engine.update(TEST_FRAME_SECONDS);
    if (engine.getState() != MatchState::PLAYING ||
        !engine.getBall().possessedBy)
    {
      continue;
    }

    const auto carrier = std::ranges::find_if(
        engine.getPlayers(), [&engine](const MatchPlayer& player)
        { return player.player == engine.getBall().possessedBy; });
    ASSERT_NE(carrier, engine.getPlayers().end());
    const TeamPhase attackingPhase =
        carrier->isHomeTeam ? engine.getHomePhase() : engine.getAwayPhase();
    if (attackingPhase != TeamPhase::FINAL_THIRD) continue;

    int restDefenders = 0;
    for (const MatchPlayer& player : engine.getPlayers())
    {
      if (player.isHomeTeam != carrier->isHomeTeam || !player.player) continue;
      const PlayerRole role = player.player->getRole();
      const bool defensiveRole = role == PlayerRole::CB ||
                                 role == PlayerRole::LB ||
                                 role == PlayerRole::RB;
      if (!defensiveRole) continue;
      const float goalSideGap = carrier->isHomeTeam
                                    ? carrier->position.x - player.position.x
                                    : player.position.x - carrier->position.x;
      if (goalSideGap >= MINIMUM_GOAL_SIDE_GAP) ++restDefenders;
    }
    EXPECT_GE(restDefenders, MINIMUM_REST_DEFENDERS);
    inspectedFinalThird = true;
    break;
  }
  EXPECT_TRUE(inspectedFinalThird);
}

TEST(MatchEngineTest, PossessionCreatesCoordinatedRunsAndSupport)
{
  std::vector<std::unique_ptr<Player>> players;
  Team home = createDummyTeam(1, "Home", 70, players);
  Team away = createDummyTeam(2, "Away", 70, players);
  const StatsConfig config = createStatsConfig();
  MatchEngine engine(home.getLineup(), away.getLineup(), home.getStrategy(),
                     away.getStrategy(), config, 8642);

  constexpr float TEST_FRAME_SECONDS = 0.05f;
  constexpr int MAX_POSSESSION_FRAMES = 300;
  constexpr float SIGNIFICANT_TACTICAL_MOVEMENT = 0.08f;
  constexpr int MIN_SIGNIFICANT_TEAM_MOVEMENTS = 3;
  bool inspectedPossession = false;
  for (int frame = 0; frame < MAX_POSSESSION_FRAMES; ++frame)
  {
    engine.update(TEST_FRAME_SECONDS);
    if (engine.getState() != MatchState::PLAYING ||
        !engine.getBall().possessedBy)
    {
      continue;
    }

    // Possession can be established after movement resolution. Advance one
    // fixed step so the team has applied its possession-phase assignments.
    engine.update(MatchTuning::Timing::FIXED_STEP_SECONDS);
    if (!engine.getBall().possessedBy) continue;

    const auto carrier = std::ranges::find_if(
        engine.getPlayers(), [&engine](const MatchPlayer& player)
        { return player.player == engine.getBall().possessedBy; });
    ASSERT_NE(carrier, engine.getPlayers().end());

    int committedRuns = 0;
    int supportOptions = 0;
    int significantMovements = 0;
    for (const MatchPlayer& player : engine.getPlayers())
    {
      if (player.isHomeTeam != carrier->isHomeTeam ||
          player.player == nullptr ||
          player.player->getRole() == PlayerRole::GK)
      {
        continue;
      }
      if (player.isMakingRun) ++committedRuns;
      if (player.intent == PlayerIntent::OFFER_SUPPORT) ++supportOptions;
      const float movement =
          std::hypot(player.movementTarget.x - player.basePosition.x,
                     player.movementTarget.y - player.basePosition.y);
      if (movement > SIGNIFICANT_TACTICAL_MOVEMENT) ++significantMovements;
    }

    EXPECT_GE(committedRuns,
              static_cast<int>(MatchTuning::Shape::MIN_COMMITTED_RUNNERS));
    EXPECT_LE(committedRuns,
              static_cast<int>(MatchTuning::Shape::MAX_COMMITTED_RUNNERS));
    EXPECT_GE(supportOptions, 2);
    EXPECT_GE(significantMovements, MIN_SIGNIFICANT_TEAM_MOVEMENTS);
    inspectedPossession = true;
    break;
  }
  EXPECT_TRUE(inspectedPossession);
}

TEST(MatchEngineTest, MovementAssignmentsRemainCoherentAcrossLivePlay)
{
  std::vector<std::unique_ptr<Player>> players;
  Team home = createDummyTeam(1, "Home", 70, players);
  Team away = createDummyTeam(2, "Away", 70, players);
  const StatsConfig config = createStatsConfig();
  MatchEngine engine(home.getLineup(), away.getLineup(), home.getStrategy(),
                     away.getStrategy(), config, 97531);

  constexpr float TEST_FRAME_SECONDS = 0.05f;
  constexpr int OBSERVATION_FRAMES = 1'200;
  constexpr float MINIMUM_NON_OVERLAP_METRES = 0.25f;
  float observedMinimumSeparation = std::numeric_limits<float>::max();
  for (int frame = 0; frame < OBSERVATION_FRAMES; ++frame)
  {
    engine.update(TEST_FRAME_SECONDS);
    int homeRunners = 0;
    int awayRunners = 0;
    int homeReceivers = 0;
    int awayReceivers = 0;
    const auto& matchPlayers = engine.getPlayers();
    for (const MatchPlayer& player : matchPlayers)
    {
      EXPECT_TRUE(std::isfinite(player.position.x));
      EXPECT_TRUE(std::isfinite(player.position.y));
      EXPECT_TRUE(std::isfinite(player.movementTarget.x));
      EXPECT_TRUE(std::isfinite(player.movementTarget.y));
      EXPECT_GE(player.position.x, MatchTuning::Pitch::PLAYER_MIN_X);
      EXPECT_LE(player.position.x, MatchTuning::Pitch::PLAYER_MAX_X);
      EXPECT_GE(player.position.y, MatchTuning::Pitch::PLAYER_MIN_Y);
      EXPECT_LE(player.position.y, MatchTuning::Pitch::PLAYER_MAX_Y);
      if (player.isMakingRun)
      {
        if (player.isHomeTeam)
          ++homeRunners;
        else
          ++awayRunners;
      }
      if (player.intent == PlayerIntent::RECEIVE_PASS)
      {
        if (player.isHomeTeam)
          ++homeReceivers;
        else
          ++awayReceivers;
      }
    }

    EXPECT_LE(homeRunners,
              static_cast<int>(MatchTuning::Shape::MAX_COMMITTED_RUNNERS));
    EXPECT_LE(awayRunners,
              static_cast<int>(MatchTuning::Shape::MAX_COMMITTED_RUNNERS));
    EXPECT_LE(homeReceivers, 1);
    EXPECT_LE(awayReceivers, 1);
    if (engine.getBall().isPass)
    {
      EXPECT_EQ(engine.getBall().passByHome ? homeRunners : awayRunners, 0)
          << "A normal pass must not make every forward start a transition run";
    }

    for (std::size_t first = 0; first < matchPlayers.size(); ++first)
    {
      for (std::size_t second = first + 1; second < matchPlayers.size();
           ++second)
      {
        const float dxMetres =
            (matchPlayers[second].position.x - matchPlayers[first].position.x) *
            MatchTuning::Pitch::LENGTH_METRES;
        const float dyMetres =
            (matchPlayers[second].position.y - matchPlayers[first].position.y) *
            MatchTuning::Pitch::WIDTH_METRES;
        observedMinimumSeparation =
            std::min(observedMinimumSeparation, std::hypot(dxMetres, dyMetres));
      }
    }
  }

  RecordProperty("minimum_player_separation_metres",
                 std::to_string(observedMinimumSeparation));
  EXPECT_GT(observedMinimumSeparation, MINIMUM_NON_OVERLAP_METRES);
}

TEST(MatchEngineTest, StrongerTeamHasStatisticalAdvantage)
{
  std::vector<std::unique_ptr<Player>> players;
  // A wide but professional gap (attributes are stretched around a typical
  // level, so 88 against 42 would be a top side against amateurs).
  Team strong = createDummyTeam(1, "Strong", 82, players);
  Team weak = createDummyTeam(2, "Weak", 52, players);
  const StatsConfig config = createStatsConfig();

  int strongGoals = 0;
  int weakGoals = 0;
  int setPieceGoals = 0;
  int penaltyGoals = 0;
  int headedGoals = 0;
  int strongShots = 0;
  int totalGoals = 0;
  int totalShots = 0;
  int totalPassesAttempted = 0;
  int totalPassesCompleted = 0;
  int totalPurposefulPasses = 0;
  int totalCrosses = 0;
  int totalCutbacks = 0;
  float totalExpectedGoals = 0.0f;
  constexpr int SAMPLE_MATCHES = 32;
  for (uint32_t seed = 1; seed <= SAMPLE_MATCHES; ++seed)
  {
    MatchEngine engine(strong.getLineup(), weak.getLineup(),
                       strong.getStrategy(), weak.getStrategy(), config, seed);
    simulateToFullTime(engine, 0.1f);
    setPieceGoals += engine.getStats().homeSetPieceGoals;
    penaltyGoals += engine.getStats().homePenaltyGoals;
    headedGoals += engine.getStats().homeHeadedGoals;
    strongShots += engine.getStats().homeShots;
    strongGoals += engine.getHomeScore();
    weakGoals += engine.getAwayScore();
    totalGoals += engine.getHomeScore() + engine.getAwayScore();
    totalShots += engine.getStats().homeShots + engine.getStats().awayShots;
    totalExpectedGoals +=
        engine.getStats().homeShotXG + engine.getStats().awayShotXG;
    totalPassesAttempted += engine.getStats().homePassesAttempted +
                            engine.getStats().awayPassesAttempted;
    totalPassesCompleted += engine.getStats().homePassesCompleted +
                            engine.getStats().awayPassesCompleted;
    totalCrosses +=
        engine.getStats().homeCrosses + engine.getStats().awayCrosses;
    totalCutbacks +=
        engine.getStats().homeCutbacks + engine.getStats().awayCutbacks;
    totalPurposefulPasses +=
        engine.getStats().homeProgressivePasses +
        engine.getStats().awayProgressivePasses +
        engine.getStats().homeThroughBalls +
        engine.getStats().awayThroughBalls + engine.getStats().homeCrosses +
        engine.getStats().awayCrosses + engine.getStats().homeCutbacks +
        engine.getStats().awayCutbacks + engine.getStats().homeSwitchesOfPlay +
        engine.getStats().awaySwitchesOfPlay;
  }

  const float goalsPerMatch =
      static_cast<float>(totalGoals) / static_cast<float>(SAMPLE_MATCHES);
  const float shotsPerMatch =
      static_cast<float>(totalShots) / static_cast<float>(SAMPLE_MATCHES);
  const float expectedGoalsPerShot =
      totalShots > 0 ? totalExpectedGoals / static_cast<float>(totalShots)
                     : 0.0f;
  const float passCompletion =
      totalPassesAttempted > 0 ? static_cast<float>(totalPassesCompleted) /
                                     static_cast<float>(totalPassesAttempted)
                               : 0.0f;
  const float purposefulPassShare =
      totalPassesAttempted > 0 ? static_cast<float>(totalPurposefulPasses) /
                                     static_cast<float>(totalPassesAttempted)
                               : 0.0f;
  RecordProperty("sample_matches", SAMPLE_MATCHES);
  RecordProperty("shots_per_match", std::to_string(shotsPerMatch));
  RecordProperty("goals_per_match", std::to_string(goalsPerMatch));
  RecordProperty("expected_goals_per_shot",
                 std::to_string(expectedGoalsPerShot));
  RecordProperty("pass_completion", std::to_string(passCompletion));
  RecordProperty("purposeful_pass_share", std::to_string(purposefulPassShare));
  RecordProperty("crosses", totalCrosses);
  RecordProperty("cutbacks", totalCutbacks);
  RecordProperty("strong_goals", strongGoals);
  RecordProperty("weak_goals", weakGoals);
  EXPECT_GT(strongGoals, weakGoals);
  EXPECT_GT(shotsPerMatch, 12.0f) << "goals/match=" << goalsPerMatch;
  EXPECT_LT(shotsPerMatch, 36.0f) << "goals/match=" << goalsPerMatch;
  EXPECT_GT(goalsPerMatch, 1.0f) << "shots/match=" << shotsPerMatch;
  EXPECT_LT(goalsPerMatch, 5.5f)
      << "shots/match=" << shotsPerMatch << " strong=" << strongGoals
      << " weak=" << weakGoals << " xG/shot=" << expectedGoalsPerShot
      << " strongShots=" << strongShots << " setPieceGoals=" << setPieceGoals
      << " penaltyGoals=" << penaltyGoals << " headedGoals=" << headedGoals;
  EXPECT_GT(expectedGoalsPerShot, 0.04f);
  EXPECT_LT(expectedGoalsPerShot, 0.28f);
  EXPECT_GT(passCompletion, 0.60f);
  EXPECT_LT(passCompletion, 0.95f);
  EXPECT_GT(purposefulPassShare, 0.15f);
  EXPECT_LT(purposefulPassShare, 0.90f);
  EXPECT_GT(totalCrosses + totalCutbacks, 0);
}

TEST(MatchEngineTest, ScoredGoalCelebratesBeforeKickoff)
{
  std::vector<std::unique_ptr<Player>> players;
  Team home = createDummyTeam(1, "Home", 88, players);
  Team away = createDummyTeam(2, "Away", 42, players);
  const StatsConfig config = createStatsConfig();
  MatchEngine engine(home.getLineup(), away.getLineup(), home.getStrategy(),
                     away.getStrategy(), config, 12'345);

  int celebrations = 0;
  int goalsByEvent = 0;
  bool sawBallBeyondLine = false;
  bool wasInGoal = false;
  int lastTotalScore = engine.getHomeScore() + engine.getAwayScore();

  for (int frame = 0;
       frame < 200'000 && engine.getState() != MatchState::FULL_TIME; ++frame)
  {
    const bool inGoal = engine.getState() == MatchState::GOAL;
    if (inGoal && !wasInGoal)
    {
      ++celebrations;
      ++lastTotalScore;
    }
    if (inGoal)
    {
      const MatchBall& ball = engine.getBall();
      if (engine.getGoalScoredByHome() && ball.position.x > 1.0f)
        sawBallBeyondLine = true;
      if (!engine.getGoalScoredByHome() && ball.position.x < 0.0f)
        sawBallBeyondLine = true;
      EXPECT_GT(engine.getGoalCelebrationRemaining(), 0.0f);
    }
    wasInGoal = inGoal;
    engine.update(0.05f);
  }

  for (const MatchEvent& event : engine.getEvents())
  {
    if (event.type != MatchEventType::GOAL &&
        event.type != MatchEventType::OWN_GOAL)
      continue;
    ++goalsByEvent;
    EXPECT_FALSE(event.description.empty());
  }

  EXPECT_EQ(engine.getState(), MatchState::FULL_TIME)
      << "The goal celebration must complete and the match must finish";
  EXPECT_GT(lastTotalScore, 0)
      << "The test seed must produce at least one goal";
  EXPECT_EQ(celebrations, goalsByEvent);
  EXPECT_TRUE(sawBallBeyondLine)
      << "The scored ball must visibly enter the net past the goal line";
}

namespace
{
/** A full squad: the dummy starting XI plus a seven-player bench. */
Team createSquadWithBench(TeamID id, const std::string& name, int rating,
                          std::vector<std::unique_ptr<Player>>& players)
{
  Team team = createDummyTeam(id, name, rating, players);
  static constexpr PlayerRole BENCH[7] = {
      PlayerRole::GK, PlayerRole::CB, PlayerRole::RB, PlayerRole::CM,
      PlayerRole::LW, PlayerRole::ST, PlayerRole::ST};
  std::vector<const Player*> reserves;
  for (uint32_t index = 0; index < 7; ++index)
  {
    const float value = static_cast<float>(rating);
    const std::map<std::string, float> stats = {
        {"Pace", value},      {"Shooting", value},  {"Passing", value},
        {"Dribbling", value}, {"Defending", value}, {"Physicality", value},
        {"Stamina", value},   {"Vision", value},    {"Goalkeeping", value}};
    auto player = std::make_unique<Player>(
        static_cast<PlayerID>(id) * 100U + 50U + index, id, "Bench",
        std::to_string(index), BENCH[index], Language::EN, 100'000, 0, 25, 3,
        182, Foot::Right, stats);
    reserves.push_back(player.get());
    players.push_back(std::move(player));
  }
  team.getLineup().setReserves(reserves);
  return team;
}

int activePlayers(const MatchEngine& engine, bool homeTeam)
{
  return static_cast<int>(std::ranges::count_if(
      engine.getPlayers(), [homeTeam](const MatchPlayer& player)
      { return player.onPitch && player.isHomeTeam == homeTeam; }));
}

/** Advances fixed steps until play next stops (a new restart begins). */
bool advanceToNextStoppage(MatchEngine& engine)
{
  bool sawPlay = false;
  for (int step = 0; step < 20'000; ++step)
  {
    if (engine.getState() == MatchState::FULL_TIME) return false;
    engine.update(MatchTuning::Timing::FIXED_STEP_SECONDS);
    const MatchState state = engine.getState();
    if (state == MatchState::PLAYING)
      sawPlay = true;
    else if (sawPlay && state != MatchState::GOAL &&
             state != MatchState::FULL_TIME)
      return true;
  }
  return false;
}
}  // namespace

TEST(MatchEngineTest, StructuredEventsAndPlayerStatsAreConsistent)
{
  std::vector<std::unique_ptr<Player>> players;
  Team home = createSquadWithBench(1, "Home", 72, players);
  Team away = createSquadWithBench(2, "Away", 64, players);
  const StatsConfig config = createStatsConfig();
  MatchEngine engine(home.getLineup(), away.getLineup(), home.getStrategy(),
                     away.getStrategy(), config, 2024);
  simulateToFullTime(engine, 0.1f);
  ASSERT_EQ(engine.getState(), MatchState::FULL_TIME);

  int homeGoals = 0;
  int awayGoals = 0;
  int homePassesAttempted = 0;
  int awayPassesAttempted = 0;
  for (const PlayerMatchStats& entry : engine.getPlayerStats())
  {
    EXPECT_NE(entry.playerId, 0u);
    EXPECT_LE(entry.passesCompleted, entry.passesAttempted);
    EXPECT_LE(entry.shotsOnTarget, entry.shots);
    EXPECT_LE(entry.goals, entry.shotsOnTarget);
    EXPECT_GE(entry.rating, MatchTuning::Rating::MINIMUM);
    EXPECT_LE(entry.rating, MatchTuning::Rating::MAXIMUM);
    EXPECT_GE(entry.condition, MatchTuning::Player::MINIMUM_STAMINA);
    EXPECT_LE(entry.condition, 1.0f);
    (entry.isHomeTeam ? homeGoals : awayGoals) += entry.goals;
    (entry.isHomeTeam ? awayGoals : homeGoals) += entry.ownGoals;
    (entry.isHomeTeam ? homePassesAttempted : awayPassesAttempted) +=
        entry.passesAttempted;
    if (entry.started && !entry.substitutedOff && !entry.sentOff &&
        !entry.injured)
    {
      // A full-match player is on the pitch for every clock minute played.
      EXPECT_NEAR(entry.minutesPlayed, engine.getElapsedMatchMinutes(), 0.05f);
      if (entry.role != PlayerRole::GK)
        EXPECT_GT(entry.distanceMetres, 4'000.0f);
    }
  }
  EXPECT_EQ(homeGoals, engine.getHomeScore());
  EXPECT_EQ(awayGoals, engine.getAwayScore());
  EXPECT_EQ(homePassesAttempted, engine.getStats().homePassesAttempted);
  EXPECT_EQ(awayPassesAttempted, engine.getStats().awayPassesAttempted);

  int goalEvents = 0;
  int addedTimeEvents = 0;
  bool sawSecondHalf = false;
  for (const MatchEvent& event : engine.getEvents())
  {
    EXPECT_TRUE(event.period == 1 || event.period == 2);
    EXPECT_GE(event.addedMinute, 0.0f);
    if (event.type == MatchEventType::GOAL ||
        event.type == MatchEventType::OWN_GOAL)
    {
      ++goalEvents;
      EXPECT_TRUE(event.hasTeam);
      EXPECT_NE(event.primaryPlayerId, 0u);
      EXPECT_STREQ(MatchCommentary::key(event),
                   event.type == MatchEventType::GOAL ? "MATCH_COMMENT_GOAL"
                                                      : "MATCH_COMMENT_OWN_GOAL");
      EXPECT_EQ(event.homeScore + event.awayScore, goalEvents)
          << "goal events carry the score after the goal";
    }
    if (event.type == MatchEventType::SHOT)
    {
      EXPECT_GT(event.xg, 0.0f);
      EXPECT_NE(event.primaryPlayerId, 0u);
    }
    if (event.type == MatchEventType::ADDED_TIME) ++addedTimeEvents;
    if (event.type == MatchEventType::SECOND_HALF)
    {
      sawSecondHalf = true;
      EXPECT_FLOAT_EQ(event.timeMinute, MatchTuning::Timing::HALF_TIME_MINUTE);
      EXPECT_EQ(event.period, 2);
    }
    if (event.type == MatchEventType::HALF_TIME)
    {
      EXPECT_GE(event.timeMinute,
                MatchTuning::Timing::HALF_TIME_MINUTE +
                    static_cast<float>(engine.getAddedMinutes(1)));
    }
    if (event.type == MatchEventType::SUBSTITUTION)
    {
      EXPECT_NE(event.primaryPlayerId, 0u);
      EXPECT_NE(event.secondaryPlayerId, 0u);
    }
  }
  EXPECT_EQ(goalEvents, engine.getHomeScore() + engine.getAwayScore());
  EXPECT_EQ(addedTimeEvents, 2);
  EXPECT_TRUE(sawSecondHalf);
  EXPECT_NE(engine.getDebugSnapshotJson().find("\"clock\":{\"period\":2"),
            std::string::npos);
}

TEST(MatchEngineTest, SentOffPlayerLeavesThePitchForGood)
{
  std::vector<std::unique_ptr<Player>> players;
  Team home = createSquadWithBench(1, "Home", 65, players);
  Team away = createSquadWithBench(2, "Away", 65, players);
  const StatsConfig config = createStatsConfig();

  bool inspected = false;
  for (uint32_t seed = 1; seed <= 400 && !inspected; ++seed)
  {
    MatchEngine engine(home.getLineup(), away.getLineup(), home.getStrategy(),
                       away.getStrategy(), config, seed);
    std::size_t seenEvents = 0;
    PlayerID sentOff = 0;
    bool sentOffHome = false;
    while (engine.getState() != MatchState::FULL_TIME)
    {
      engine.update(0.1f);
      const auto& events = engine.getEvents();
      for (; seenEvents < events.size() && sentOff == 0; ++seenEvents)
      {
        const MatchEvent& event = events[seenEvents];
        if (event.type != MatchEventType::RED_CARD &&
            event.type != MatchEventType::SECOND_YELLOW)
          continue;
        sentOff = event.primaryPlayerId;
        sentOffHome = event.isHomeTeam;
      }
      if (sentOff == 0) continue;
      for (const MatchPlayer& player : engine.getPlayers())
      {
        if (player.player && player.player->getId() == sentOff)
          EXPECT_FALSE(player.onPitch);
      }
      EXPECT_LE(activePlayers(engine, sentOffHome), 10);
      EXPECT_NE(engine.getBall().possessedBy &&
                    engine.getBall().possessedBy->getId() == sentOff,
                true);
      inspected = true;
    }
    if (sentOff != 0)
    {
      const PlayerMatchStats* entry = engine.findPlayerStats(sentOff);
      ASSERT_NE(entry, nullptr);
      EXPECT_TRUE(entry->sentOff);
      EXPECT_EQ(entry->redCards, 1);
      // A dismissed player cannot be replaced.
      EXPECT_FALSE(engine.substitutePlayer(sentOff, players.back().get()));
    }
  }
  EXPECT_TRUE(inspected) << "no red card in the sampled seeds";
}

TEST(MatchEngineTest, InjuredPlayersAreReplacedOrLeaveThePitch)
{
  std::vector<std::unique_ptr<Player>> players;
  Team home = createSquadWithBench(1, "Home", 65, players);
  Team away = createSquadWithBench(2, "Away", 65, players);
  const StatsConfig config = createStatsConfig();

  int injuriesChecked = 0;
  for (uint32_t seed = 1; seed <= 120 && injuriesChecked < 3; ++seed)
  {
    MatchEngine engine(home.getLineup(), away.getLineup(), home.getStrategy(),
                       away.getStrategy(), config, seed);
    simulateToFullTime(engine, 0.1f);
    for (const MatchEvent& event : engine.getEvents())
    {
      if (event.type != MatchEventType::INJURY) continue;
      const PlayerMatchStats* entry =
          engine.findPlayerStats(event.primaryPlayerId);
      ASSERT_NE(entry, nullptr);
      EXPECT_TRUE(entry->injured);
      const bool replaced = std::ranges::any_of(
          engine.getSubstitutions(),
          [&event](const MatchSubstitution& change)
          {
            return change.outgoingPlayerId == event.primaryPlayerId &&
                   change.reason == SubstitutionReason::INJURY;
          });
      const bool stillOn = std::ranges::any_of(
          engine.getPlayers(),
          [&event](const MatchPlayer& player)
          {
            return player.onPitch && player.player &&
                   player.player->getId() == event.primaryPlayerId;
          });
      // Injured players leave at the next stoppage unless play never stopped
      // again before full time.
      EXPECT_TRUE(replaced || !stillOn ||
                  event.timeMinute > engine.getMatchTimeMinutes() - 2.0f)
          << "injured player " << event.primaryPlayerId << " kept playing";
      ++injuriesChecked;
    }
  }
  EXPECT_GT(injuriesChecked, 0) << "no injury in the sampled seeds";
}

TEST(MatchEngineTest, AiSubstitutionsRespectTheLaws)
{
  std::vector<std::unique_ptr<Player>> players;
  Team home = createSquadWithBench(1, "Home", 66, players);
  Team away = createSquadWithBench(2, "Away", 66, players);
  const StatsConfig config = createStatsConfig();

  int totalSubstitutions = 0;
  for (uint32_t seed = 1; seed <= 20; ++seed)
  {
    MatchEngine engine(home.getLineup(), away.getLineup(), home.getStrategy(),
                       away.getStrategy(), config, seed * 31U);
    simulateToFullTime(engine, 0.1f);
    for (const bool side : {true, false})
    {
      EXPECT_LE(engine.getSubstitutionsUsed(side),
                MatchTuning::Rules::MAX_SUBSTITUTIONS_PER_TEAM);
      EXPECT_LE(engine.getSubstitutionWindowsUsed(side),
                MatchTuning::Substitution::MAX_WINDOWS);
    }
    std::vector<PlayerID> leftThePitch;
    for (const MatchSubstitution& change : engine.getSubstitutions())
    {
      ++totalSubstitutions;
      // Incoming players come from the bench and nobody returns.
      EXPECT_GE(change.incomingPlayerId % 100U, 50u);
      EXPECT_EQ(std::ranges::count(leftThePitch, change.incomingPlayerId), 0);
      leftThePitch.push_back(change.outgoingPlayerId);
      EXPECT_NE(change.reason, SubstitutionReason::MANUAL);
    }
  }
  EXPECT_GT(totalSubstitutions, 20 * 2 * 2)
      << "AI managers should use most of their substitutions";
}

TEST(MatchEngineTest, ManualSubstitutionsFollowLimitsAndWindows)
{
  std::vector<std::unique_ptr<Player>> players;
  Team home = createSquadWithBench(1, "Home", 66, players);
  Team away = createSquadWithBench(2, "Away", 66, players);
  const StatsConfig config = createStatsConfig();
  MatchEngine engine(home.getLineup(), away.getLineup(), home.getStrategy(),
                     away.getStrategy(), config, 99);
  engine.setAutoSubstitutions(false, false);
  const auto& bench = home.getLineup().getReserves();

  // Two changes at the kick-off stoppage share one window.
  EXPECT_TRUE(engine.substitutePlayer(109, bench[5]));
  EXPECT_TRUE(engine.substitutePlayer(110, bench[6]));
  EXPECT_EQ(engine.getSubstitutionWindowsUsed(true), 1);
  // Nobody can come back on, and a bench player cannot come on twice.
  EXPECT_FALSE(engine.substitutePlayer(107, bench[5]));
  EXPECT_FALSE(engine.substitutePlayer(107, players[9].get()));

  ASSERT_TRUE(advanceToNextStoppage(engine));
  EXPECT_TRUE(engine.substitutePlayer(107, bench[4]));
  EXPECT_EQ(engine.getSubstitutionWindowsUsed(true), 2);
  ASSERT_TRUE(advanceToNextStoppage(engine));
  EXPECT_TRUE(engine.substitutePlayer(105, bench[3]));
  EXPECT_EQ(engine.getSubstitutionWindowsUsed(true), 3);
  ASSERT_TRUE(advanceToNextStoppage(engine));
  // The three windows are used: a fourth stoppage cannot open another.
  EXPECT_FALSE(engine.canSubstitute(true));
  EXPECT_FALSE(engine.substitutePlayer(102, bench[1]));
  EXPECT_EQ(engine.getSubstitutionsUsed(true), 4);
  EXPECT_TRUE(engine.canSubstitute(false));
}

TEST(MatchEngineTest, PenaltiesAreTakenByTheBestOutfieldShooter)
{
  std::vector<std::unique_ptr<Player>> players;
  Team home = createSquadWithBench(1, "Home", 65, players);
  Team away = createSquadWithBench(2, "Away", 65, players);
  const StatsConfig config = createStatsConfig();
  const Player* bestShooter = players[7].get();
  std::map<std::string, float> sharp = bestShooter->getStats();
  sharp["Shooting"] = 92.0f;
  auto specialist = std::make_unique<Player>(
      bestShooter->getId(), 1, "Spot", "Kick", PlayerRole::LW, Language::EN,
      100'000, 0, 25, 3, 180, Foot::Right, sharp);
  std::map<std::string, float> keeperStats = players[0]->getStats();
  keeperStats["Shooting"] = 99.0f;
  auto shootingKeeper = std::make_unique<Player>(
      players[0]->getId(), 1, "Keeper", "Shooter", PlayerRole::GK, Language::EN,
      100'000, 0, 25, 3, 190, Foot::Right, keeperStats);
  home.getLineup().setGoalkeeper(shootingKeeper.get());
  home.getLineup().removeOutfieldPlayer(bestShooter->getId());
  home.getLineup().addOutfieldPlayer(specialist.get(), {0.68f, 0.16f});

  int penaltiesChecked = 0;
  for (uint32_t seed = 1; seed <= 300 && penaltiesChecked < 3; ++seed)
  {
    MatchEngine engine(home.getLineup(), away.getLineup(), home.getStrategy(),
                       away.getStrategy(), config, seed);
    engine.setAutoSubstitutions(false, false);
    simulateToFullTime(engine, 0.1f);
    const PlayerMatchStats* specialistStats =
        engine.findPlayerStats(specialist->getId());
    ASSERT_NE(specialistStats, nullptr);
    if (specialistStats->sentOff || specialistStats->injured) continue;
    for (const MatchEvent& event : engine.getEvents())
    {
      if (event.type != MatchEventType::PENALTY || !event.isHomeTeam) continue;
      EXPECT_EQ(event.primaryPlayerId, specialist->getId())
          << "the penalty taker must be the best outfield shooter";
      ++penaltiesChecked;
    }
  }
  EXPECT_GT(penaltiesChecked, 0) << "no home penalty in the sampled seeds";
}

TEST(MatchEngineTest, DesignatedSetPieceTakersTakeTheirDuties)
{
  std::vector<std::unique_ptr<Player>> players;
  Team home = createSquadWithBench(1, "Home", 65, players);
  Team away = createSquadWithBench(2, "Away", 65, players);
  const StatsConfig config = createStatsConfig();
  // A sharp shooter on the left wing would be the automatic taker.
  const Player* winger = players[7].get();
  std::map<std::string, float> sharp = winger->getStats();
  sharp["Shooting"] = 92.0f;
  auto specialist = std::make_unique<Player>(
      winger->getId(), 1, "Spot", "Kick", PlayerRole::LW, Language::EN,
      100'000, 0, 25, 3, 180, Foot::Right, sharp);
  home.getLineup().removeOutfieldPlayer(winger->getId());
  home.getLineup().addOutfieldPlayer(specialist.get(), {0.68f, 0.16f});
  constexpr PlayerID PENALTY_TAKER = 102;  // a centre-back
  constexpr PlayerID CORNER_TAKER = 104;   // the right-back
  home.getLineup().setDesignated(SetPieceDuty::Penalties, PENALTY_TAKER);
  home.getLineup().setDesignated(SetPieceDuty::CornersLeft, CORNER_TAKER);
  home.getLineup().setDesignated(SetPieceDuty::CornersRight, CORNER_TAKER);

  int designatedPenalties = 0;
  int corners = 0;
  int fallbackPenalties = 0;
  for (uint32_t seed = 1; seed <= 300 && (designatedPenalties < 2 ||
                                          fallbackPenalties < 2);
       ++seed)
  {
    // Odd seeds: the designated taker plays. Even seeds: he is replaced at
    // kick-off, so the automatic choice (the best shooter) takes over.
    const bool replaced = seed % 2 == 0;
    MatchEngine engine(home.getLineup(), away.getLineup(), home.getStrategy(),
                       away.getStrategy(), config, seed);
    engine.setAutoSubstitutions(false, false);
    if (replaced)
    {
      ASSERT_TRUE(engine.substitutePlayer(PENALTY_TAKER,
                                          home.getLineup().getReserves()[1]));
    }
    simulateToFullTime(engine, 0.1f);
    const PlayerMatchStats* takerStats = engine.findPlayerStats(PENALTY_TAKER);
    const PlayerMatchStats* specialistStats =
        engine.findPlayerStats(specialist->getId());
    ASSERT_NE(takerStats, nullptr);
    ASSERT_NE(specialistStats, nullptr);
    const bool takerAvailable =
        !replaced && !takerStats->sentOff && !takerStats->injured;
    const bool specialistAvailable =
        !specialistStats->sentOff && !specialistStats->injured;
    const bool cornerTakerAvailable = [&]
    {
      const PlayerMatchStats* entry = engine.findPlayerStats(CORNER_TAKER);
      return entry && !entry->sentOff && !entry->injured;
    }();
    for (const MatchEvent& event : engine.getEvents())
    {
      if (!event.isHomeTeam) continue;
      if (event.type == MatchEventType::CORNER && cornerTakerAvailable)
      {
        EXPECT_EQ(event.primaryPlayerId, CORNER_TAKER);
        ++corners;
      }
      if (event.type != MatchEventType::PENALTY) continue;
      if (takerAvailable)
      {
        EXPECT_EQ(event.primaryPlayerId, PENALTY_TAKER);
        ++designatedPenalties;
      }
      else if (replaced && specialistAvailable)
      {
        EXPECT_EQ(event.primaryPlayerId, specialist->getId())
            << "without the designated taker the best shooter steps up";
        ++fallbackPenalties;
      }
    }
  }
  EXPECT_GT(designatedPenalties, 0);
  EXPECT_GT(fallbackPenalties, 0);
  EXPECT_GT(corners, 0);
}

TEST(MatchEngineTest, ConditionCanBeCarriedBetweenMatches)
{
  std::vector<std::unique_ptr<Player>> players;
  Team home = createSquadWithBench(1, "Home", 65, players);
  Team away = createSquadWithBench(2, "Away", 65, players);
  const StatsConfig config = createStatsConfig();
  MatchEngine engine(home.getLineup(), away.getLineup(), home.getStrategy(),
                     away.getStrategy(), config, 5150);
  engine.setAutoSubstitutions(false, false);
  ASSERT_TRUE(engine.setPlayerCondition(105, 0.7f));
  EXPECT_FLOAT_EQ(engine.getPlayerCondition(105).value_or(0.0f), 0.7f);
  EXPECT_FALSE(engine.getPlayerCondition(999'999).has_value());

  simulateToFullTime(engine, 0.1f);
  EXPECT_FALSE(engine.setPlayerCondition(106, 0.5f))
      << "condition can only be set before kick-off";
  const float tired = engine.getPlayerCondition(105).value_or(1.0f);
  const float fresh = engine.getPlayerCondition(106).value_or(0.0f);
  EXPECT_LT(tired, 0.7f);
  EXPECT_LT(fresh, 0.95f) << "a full match must cost condition";
  EXPECT_GT(fresh, tired);
  EXPECT_GE(tired, MatchTuning::Player::MINIMUM_STAMINA);
}

TEST(MatchEngineTest, GoalkeeperStateMachineCoversTheMatch)
{
  std::vector<std::unique_ptr<Player>> players;
  Team home = createSquadWithBench(1, "Home", 70, players);
  Team away = createSquadWithBench(2, "Away", 70, players);
  const StatsConfig config = createStatsConfig();
  MatchEngine engine(home.getLineup(), away.getLineup(), home.getStrategy(),
                     away.getStrategy(), config, 4321);
  std::map<GoalkeeperState, int> observed;
  while (engine.getState() != MatchState::FULL_TIME)
  {
    engine.update(MatchTuning::Timing::FIXED_STEP_SECONDS * 2.0f);
    ++observed[engine.getHomeGoalkeeperState()];
    ++observed[engine.getAwayGoalkeeperState()];
  }
  EXPECT_GT(observed[GoalkeeperState::SET_POSITION], 0);
  EXPECT_GT(observed[GoalkeeperState::DIVE], 0);
  EXPECT_GT(observed[GoalkeeperState::HOLD], 0);
  EXPECT_GT(observed[GoalkeeperState::DISTRIBUTE] +
                observed[GoalkeeperState::RECOVER],
            0);
  EXPECT_EQ(goalkeeperStateName(GoalkeeperState::DIVE), "dive");
}

TEST(MatchEngineTest, FullHeadlessMatchIsFast)
{
  std::vector<std::unique_ptr<Player>> players;
  Team home = createSquadWithBench(1, "Home", 70, players);
  Team away = createSquadWithBench(2, "Away", 70, players);
  const StatsConfig config = createStatsConfig();
  // Processor time of the fastest of several matches: robust against other
  // processes competing for the machine, still a hard budget per match.
  constexpr int MATCHES = 10;
  double fastest = std::numeric_limits<double>::max();
  double total = 0.0;
  for (uint32_t seed = 1; seed <= MATCHES; ++seed)
  {
    MatchEngine engine(home.getLineup(), away.getLineup(), home.getStrategy(),
                       away.getStrategy(), config, seed);
    const std::clock_t started = std::clock();
    engine.simulateToEnd();
    const double milliseconds = 1000.0 *
                                static_cast<double>(std::clock() - started) /
                                CLOCKS_PER_SEC;
    fastest = std::min(fastest, milliseconds);
    total += milliseconds;
    EXPECT_EQ(engine.getState(), MatchState::FULL_TIME);
  }
  RecordProperty("milliseconds_per_match", std::to_string(total / MATCHES));
  std::printf("[timing] headless match cpu %.2f ms (fastest %.2f ms)\n",
              total / MATCHES, fastest);
  EXPECT_LT(fastest, 150.0);
}

TEST(MatchEngineTest, BackgroundFidelityIsDeterministicAndCompletes)
{
  std::vector<std::unique_ptr<Player>> players;
  Team home = createSquadWithBench(1, "Home", 70, players);
  Team away = createSquadWithBench(2, "Away", 66, players);
  const StatsConfig config = createStatsConfig();
  const auto play = [&](MatchFidelity fidelity)
  {
    MatchEngine engine(home.getLineup(), away.getLineup(), home.getStrategy(),
                       away.getStrategy(), config, 2024);
    engine.simulateToEnd(fidelity);
    return engine;
  };
  const MatchEngine first = play(MatchFidelity::BACKGROUND);
  const MatchEngine second = play(MatchFidelity::BACKGROUND);
  ASSERT_EQ(first.getState(), MatchState::FULL_TIME);
  EXPECT_EQ(first.getHomeScore(), second.getHomeScore());
  EXPECT_EQ(first.getAwayScore(), second.getAwayScore());
  EXPECT_EQ(first.getSimulatedSteps(), second.getSimulatedSteps());
  ASSERT_EQ(first.getEvents().size(), second.getEvents().size());
  for (std::size_t index = 0; index < first.getEvents().size(); ++index)
  {
    EXPECT_EQ(first.getEvents()[index].type, second.getEvents()[index].type);
    EXPECT_EQ(first.getEvents()[index].primaryPlayerId,
              second.getEvents()[index].primaryPlayerId);
  }
  // A whole match of real time at either fidelity.
  EXPECT_GE(first.getSimulatedSeconds(), 5'600.0);
  EXPECT_LE(first.getSimulatedSeconds(), 6'600.0);
  EXPECT_GE(first.getStats().ballInPlayMinutes, 45.0f);

  // The default headless path stays at full fidelity: it replays the live
  // update() path step for step.
  MatchEngine headless(home.getLineup(), away.getLineup(), home.getStrategy(),
                       away.getStrategy(), config, 2024);
  headless.simulateToEnd();
  const MatchEngine full = play(MatchFidelity::FULL);
  EXPECT_EQ(headless.getSimulatedSteps(), full.getSimulatedSteps());
  EXPECT_EQ(headless.getEvents().size(), full.getEvents().size());
}

TEST(MatchEngineTest, DefaultMatchContextPlaysExactlyLikeNone)
{
  std::vector<std::unique_ptr<Player>> players;
  Team home = createSquadWithBench(1, "Home", 70, players);
  Team away = createSquadWithBench(2, "Away", 66, players);
  const StatsConfig config = createStatsConfig();
  MatchEngine plain(home.getLineup(), away.getLineup(), home.getStrategy(),
                    away.getStrategy(), config, 4242);
  MatchEngine withContext(home.getLineup(), away.getLineup(),
                          home.getStrategy(), away.getStrategy(), config, 4242);
  withContext.setMatchContext(MatchContext{});
  EXPECT_EQ(plain.getRefereeStrictness(), withContext.getRefereeStrictness());
  plain.simulateToEnd(MatchFidelity::BACKGROUND);
  withContext.simulateToEnd(MatchFidelity::BACKGROUND);
  EXPECT_EQ(plain.getHomeScore(), withContext.getHomeScore());
  EXPECT_EQ(plain.getAwayScore(), withContext.getAwayScore());
  EXPECT_EQ(plain.getSimulatedSteps(), withContext.getSimulatedSteps());
  EXPECT_EQ(plain.getStats().homePossession,
            withContext.getStats().homePossession);
  ASSERT_EQ(plain.getEvents().size(), withContext.getEvents().size());
  for (std::size_t index = 0; index < plain.getEvents().size(); ++index)
  {
    EXPECT_EQ(plain.getEvents()[index].type, withContext.getEvents()[index].type);
    EXPECT_EQ(plain.getEvents()[index].timeMinute,
              withContext.getEvents()[index].timeMinute);
    EXPECT_EQ(plain.getEvents()[index].primaryPlayerId,
              withContext.getEvents()[index].primaryPlayerId);
  }
}

TEST(MatchEngineTest, MatchContextIsSetBeforeKickOffWithinLimits)
{
  std::vector<std::unique_ptr<Player>> players;
  Team home = createSquadWithBench(1, "Home", 70, players);
  Team away = createSquadWithBench(2, "Away", 70, players);
  const StatsConfig config = createStatsConfig();
  MatchEngine engine(home.getLineup(), away.getLineup(), home.getStrategy(),
                     away.getStrategy(), config, 7);
  MatchContext extreme;
  extreme.goalRateScale = 50.0f;
  extreme.refereeStrictnessMean = -3.0f;
  extreme.homeAdvantageScale = std::numeric_limits<float>::quiet_NaN();
  engine.setMatchContext(extreme);
  EXPECT_LE(engine.getMatchContext().goalRateScale,
            MatchTuning::Context::MAX_GOAL_RATE_SCALE);
  EXPECT_GE(engine.getMatchContext().refereeStrictnessMean,
            MatchTuning::Context::MIN_REFEREE_STRICTNESS);
  EXPECT_EQ(engine.getMatchContext().homeAdvantageScale, 1.0f);
  EXPECT_GT(engine.getRefereeStrictness(), 0.0f);

  // Once the match is under way the context no longer changes.
  engine.advance(30.0f);
  const MatchContext before = engine.getMatchContext();
  MatchContext later;
  later.goalRateScale = 0.7f;
  engine.setMatchContext(later);
  EXPECT_EQ(engine.getMatchContext().goalRateScale, before.goalRateScale);
}

TEST(MatchEngineTest, MatchContextMovesGoalsCardsAndHomeAdvantage)
{
  std::vector<std::unique_ptr<Player>> players;
  Team home = createSquadWithBench(1, "Home", 68, players);
  Team away = createSquadWithBench(2, "Away", 68, players);
  const StatsConfig config = createStatsConfig();
  struct Totals
  {
    int goals = 0;
    int yellows = 0;
    int homePoints = 0;
  };
  // The same seeds under each context, so only the context differs.
  const auto run = [&](const MatchContext& context, int matches)
  {
    Totals totals;
    for (int index = 0; index < matches; ++index)
    {
      MatchEngine engine(home.getLineup(), away.getLineup(), home.getStrategy(),
                         away.getStrategy(), config,
                         900U + static_cast<uint32_t>(index) * 31U);
      engine.setMatchContext(context);
      engine.simulateToEnd(MatchFidelity::BACKGROUND);
      totals.goals += engine.getHomeScore() + engine.getAwayScore();
      totals.yellows +=
          engine.getStats().homeYellowCards + engine.getStats().awayYellowCards;
      totals.homePoints += engine.getHomeScore() > engine.getAwayScore()    ? 3
                           : engine.getHomeScore() == engine.getAwayScore() ? 1
                                                                            : 0;
    }
    return totals;
  };
  MatchContext lowScoring;
  lowScoring.goalRateScale = 0.8f;
  MatchContext highScoring;
  highScoring.goalRateScale = 1.25f;
  const Totals fewGoals = run(lowScoring, 60);
  const Totals manyGoals = run(highScoring, 60);
  std::printf("[context] goals %d vs %d\n", fewGoals.goals, manyGoals.goals);
  EXPECT_LT(fewGoals.goals * 115, manyGoals.goals * 100);

  MatchContext lenient;
  lenient.refereeStrictnessMean = 0.75f;
  MatchContext strict;
  strict.refereeStrictnessMean = 1.35f;
  const Totals fewCards = run(lenient, 40);
  const Totals manyCards = run(strict, 40);
  std::printf("[context] yellows %d vs %d\n", fewCards.yellows,
              manyCards.yellows);
  EXPECT_LT(fewCards.yellows * 130, manyCards.yellows * 100);

  MatchContext neutral;
  neutral.homeAdvantageScale = 0.0f;
  MatchContext fortress;
  fortress.homeAdvantageScale = 2.0f;
  const Totals neutralVenue = run(neutral, 80);
  const Totals strongHome = run(fortress, 80);
  std::printf("[context] home points %d vs %d\n", neutralVenue.homePoints,
              strongHome.homePoints);
  EXPECT_LT(neutralVenue.homePoints, strongHome.homePoints);
}

namespace
{
/** Plays a knockout match between two squads to the end. */
MatchEngine playKnockout(const Team& home, const Team& away,
                         const StatsConfig& config, uint32_t seed,
                         const MatchRules::Knockout& rules)
{
  MatchEngine engine(home.getLineup(), away.getLineup(), home.getStrategy(),
                     away.getStrategy(), config, seed);
  engine.setKnockout(rules);
  engine.simulateToEnd();
  return engine;
}
}  // namespace

TEST(MatchEngineTest, LevelKnockoutGoesToExtraTimeThenPenalties)
{
  std::vector<std::unique_ptr<Player>> players;
  Team home = createSquadWithBench(1, "Home", 68, players);
  Team away = createSquadWithBench(2, "Away", 68, players);
  const StatsConfig config = createStatsConfig();
  MatchRules::Knockout rules;
  rules.required = true;

  int shootouts = 0;
  for (uint32_t seed = 1; seed <= 200 && shootouts < 3; ++seed)
  {
    const MatchEngine engine = playKnockout(home, away, config, seed, rules);
    ASSERT_EQ(engine.getState(), MatchState::FULL_TIME);
    ASSERT_TRUE(engine.getTieWinnerHome().has_value())
        << "a knockout match always has a winner";
    if (!engine.hasShootout()) continue;
    ++shootouts;
    EXPECT_TRUE(engine.wentToExtraTime());
    EXPECT_EQ(engine.getHomeScore(), engine.getAwayScore());
    EXPECT_EQ(engine.getPeriod(), 4);
    EXPECT_GE(engine.getElapsedMatchMinutes(), 120.0f);

    bool sawExtraTime = false;
    bool sawSecondExtraHalf = false;
    int goalEvents = 0;
    int kicks = 0;
    std::optional<bool> lastKicker;
    for (const MatchEvent& event : engine.getEvents())
    {
      if (event.type == MatchEventType::SECOND_HALF && event.period == 3)
        sawExtraTime = true;
      if (event.type == MatchEventType::SECOND_HALF && event.period == 4)
        sawSecondExtraHalf = true;
      if (event.type == MatchEventType::GOAL ||
          event.type == MatchEventType::OWN_GOAL)
        ++goalEvents;
      if (event.type != MatchEventType::PENALTY_SHOOTOUT ||
          event.detail == MatchEventDetail::NONE)
        continue;
      ++kicks;
      // ABAB: the sides alternate from the first kick to the last.
      if (lastKicker) EXPECT_NE(*lastKicker, event.isHomeTeam);
      lastKicker = event.isHomeTeam;
      EXPECT_NE(event.primaryPlayerId, 0u);
    }
    EXPECT_TRUE(sawExtraTime);
    EXPECT_TRUE(sawSecondExtraHalf);
    EXPECT_EQ(goalEvents, engine.getHomeScore() + engine.getAwayScore())
        << "shootout kicks never count as goals";
    EXPECT_EQ(kicks, engine.getShootoutKicks(true) +
                         engine.getShootoutKicks(false));
    EXPECT_NE(engine.getShootoutScore(true), engine.getShootoutScore(false));
    EXPECT_TRUE(MatchRules::shootoutDecided(
        engine.getShootoutScore(true), engine.getShootoutKicks(true),
        engine.getShootoutScore(false), engine.getShootoutKicks(false)));
    EXPECT_EQ(*engine.getTieWinnerHome(),
              engine.getShootoutScore(true) > engine.getShootoutScore(false));
    EXPECT_EQ(engine.getEvents().back().type, MatchEventType::FULL_TIME);
    EXPECT_LE(std::max(engine.getSubstitutionsUsed(true),
                       engine.getSubstitutionsUsed(false)),
              MatchTuning::Rules::MAX_SUBSTITUTIONS_PER_TEAM +
                  MatchTuning::Rules::EXTRA_TIME_SUBSTITUTIONS);

    // The same seed plays the same extra time and shootout.
    const MatchEngine replay = playKnockout(home, away, config, seed, rules);
    EXPECT_EQ(replay.getShootoutScore(true), engine.getShootoutScore(true));
    EXPECT_EQ(replay.getShootoutScore(false), engine.getShootoutScore(false));
    ASSERT_EQ(replay.getEvents().size(), engine.getEvents().size());
    for (std::size_t index = 0; index < engine.getEvents().size(); ++index)
    {
      EXPECT_EQ(replay.getEvents()[index].type, engine.getEvents()[index].type);
      EXPECT_EQ(replay.getEvents()[index].primaryPlayerId,
                engine.getEvents()[index].primaryPlayerId);
    }
  }
  EXPECT_GT(shootouts, 0) << "no shootout in the sampled seeds";
}

TEST(MatchEngineTest, AggregateScoreDecidesWhetherExtraTimeIsPlayed)
{
  std::vector<std::unique_ptr<Player>> players;
  Team home = createSquadWithBench(1, "Home", 68, players);
  Team away = createSquadWithBench(2, "Away", 68, players);
  const StatsConfig config = createStatsConfig();
  // Second leg: the home side won the first leg 1-0 away (so it leads 1-0).
  MatchRules::Knockout rules;
  rules.required = true;
  rules.homeAggregate = 1;
  rules.awayAggregate = 0;
  int extraTimes = 0;
  int decidedInNormalTime = 0;
  for (uint32_t seed = 1; seed <= 60; ++seed)
  {
    const MatchEngine engine = playKnockout(home, away, config, seed, rules);
    ASSERT_EQ(engine.getState(), MatchState::FULL_TIME);
    ASSERT_TRUE(engine.getTieWinnerHome().has_value());
    int normalTimeHome = 0;
    int normalTimeAway = 0;
    for (const MatchEvent& event : engine.getEvents())
    {
      if (event.period > 2) break;
      normalTimeHome = event.homeScore;
      normalTimeAway = event.awayScore;
    }
    const bool levelAfterNinety = normalTimeHome + 1 == normalTimeAway;
    EXPECT_EQ(engine.wentToExtraTime(), levelAfterNinety) << "seed " << seed;
    if (levelAfterNinety)
    {
      ++extraTimes;
      continue;
    }
    ++decidedInNormalTime;
    EXPECT_EQ(*engine.getTieWinnerHome(), normalTimeHome + 1 > normalTimeAway);
    EXPECT_FALSE(engine.hasShootout());
    EXPECT_EQ(engine.getPeriod(), 2);
  }
  EXPECT_GT(decidedInNormalTime, 0);
  // Without extra time a level tie goes straight to penalties.
  rules.extraTime = false;
  rules.homeAggregate = 0;
  for (uint32_t seed = 1; seed <= 60; ++seed)
  {
    const MatchEngine engine = playKnockout(home, away, config, seed, rules);
    EXPECT_FALSE(engine.wentToExtraTime());
    if (engine.getHomeScore() == engine.getAwayScore())
    {
      EXPECT_TRUE(engine.hasShootout());
      EXPECT_EQ(engine.getPeriod(), 2);
    }
  }
  RecordProperty("extra_times", extraTimes);
}

TEST(MatchEngineTest, LeagueMatchesCanEndLevel)
{
  std::vector<std::unique_ptr<Player>> players;
  Team home = createSquadWithBench(1, "Home", 68, players);
  Team away = createSquadWithBench(2, "Away", 68, players);
  const StatsConfig config = createStatsConfig();
  int draws = 0;
  for (uint32_t seed = 1; seed <= 40; ++seed)
  {
    MatchEngine engine(home.getLineup(), away.getLineup(), home.getStrategy(),
                       away.getStrategy(), config, seed);
    engine.simulateToEnd();
    ASSERT_EQ(engine.getState(), MatchState::FULL_TIME);
    EXPECT_EQ(engine.getPeriod(), 2);
    EXPECT_FALSE(engine.wentToExtraTime());
    EXPECT_FALSE(engine.hasShootout());
    EXPECT_FALSE(engine.getTieWinnerHome().has_value());
    for (const MatchEvent& event : engine.getEvents())
      EXPECT_NE(event.type, MatchEventType::PENALTY_SHOOTOUT);
    if (engine.getHomeScore() == engine.getAwayScore()) ++draws;
  }
  EXPECT_GT(draws, 0);
}

TEST(MatchEngineTest, HeadlessSimulationMatchesLivePlayback)
{
  std::vector<std::unique_ptr<Player>> players;
  Team home = createSquadWithBench(1, "Home", 68, players);
  Team away = createSquadWithBench(2, "Away", 66, players);
  const StatsConfig config = createStatsConfig();
  MatchEngine headless(home.getLineup(), away.getLineup(), home.getStrategy(),
                       away.getStrategy(), config, 31'337);
  MatchEngine live(home.getLineup(), away.getLineup(), home.getStrategy(),
                   away.getStrategy(), config, 31'337);

  headless.simulateToEnd();
  simulateToFullTime(live, 0.35f);

  ASSERT_EQ(headless.getState(), MatchState::FULL_TIME);
  ASSERT_EQ(live.getState(), MatchState::FULL_TIME);
  EXPECT_EQ(live.getDroppedSimulationSteps(), 0u);
  EXPECT_EQ(headless.getHomeScore(), live.getHomeScore());
  EXPECT_EQ(headless.getAwayScore(), live.getAwayScore());
  EXPECT_EQ(headless.getStats().homeShots, live.getStats().homeShots);
  EXPECT_EQ(headless.getStats().awayPassesCompleted,
            live.getStats().awayPassesCompleted);
  EXPECT_EQ(headless.getEvents().size(), live.getEvents().size());
  EXPECT_DOUBLE_EQ(headless.getSimulatedSeconds(), live.getSimulatedSeconds());
  ASSERT_EQ(headless.getPlayerStats().size(), live.getPlayerStats().size());
  EXPECT_FLOAT_EQ(headless.getPlayerStats()[3].distanceMetres,
                  live.getPlayerStats()[3].distanceMetres);
}

TEST(MatchEngineTest, AdvanceRunsEveryStepWhileUpdateBoundsCatchUp)
{
  std::vector<std::unique_ptr<Player>> players;
  Team home = createDummyTeam(1, "Home", 65, players);
  Team away = createDummyTeam(2, "Away", 65, players);
  const StatsConfig config = createStatsConfig();
  MatchEngine engine(home.getLineup(), away.getLineup(), home.getStrategy(),
                     away.getStrategy(), config, 4'242);

  const float advanced = engine.advance(600.0f);
  EXPECT_NEAR(advanced, 600.0f, 0.001f);
  EXPECT_NEAR(engine.getSimulatedSeconds(), 600.0, 0.001);
  EXPECT_EQ(engine.getDroppedSimulationSteps(), 0u);
  // Ten minutes of real match time on the clock, less the few seconds of
  // kick-off setup that do not run it.
  EXPECT_LE(engine.getElapsedMatchMinutes(), 10.0f);
  EXPECT_GE(engine.getElapsedMatchMinutes(), 9.5f);

  engine.update(60.0f);
  EXPECT_EQ(engine.getLastUpdateStepCount(),
            MatchTuning::Timing::MAX_FIXED_STEPS_PER_UPDATE);
  EXPECT_GT(engine.getDroppedSimulationSteps(), 0u);
}

TEST(MatchEngineTest, PlayersAndBallMoveAtRealisticSpeeds)
{
  std::vector<std::unique_ptr<Player>> players;
  Team home = createSquadWithBench(1, "Home", 70, players);
  Team away = createSquadWithBench(2, "Away", 70, players);
  const StatsConfig config = createStatsConfig();
  MatchEngine engine(home.getLineup(), away.getLineup(), home.getStrategy(),
                     away.getStrategy(), config, 2'718);

  float fastestPass = 0.0f;
  float fastestShot = 0.0f;
  float fastestPlayer = 0.0f;
  while (engine.getState() != MatchState::FULL_TIME)
  {
    engine.advance(MatchTuning::Timing::FIXED_STEP_SECONDS);
    const MatchBall& ball = engine.getBall();
    const float ballSpeed = std::hypot(ball.velocity.x, ball.velocity.y);
    if (ball.isPass) fastestPass = std::max(fastestPass, ballSpeed);
    if (ball.isShot) fastestShot = std::max(fastestShot, ballSpeed);
    for (const MatchPlayer& player : engine.getPlayers())
    {
      fastestPlayer = std::max(
          fastestPlayer, std::hypot(player.velocity.x, player.velocity.y));
    }
  }
  // Passes 7-25 m/s, struck shots up to ~33 m/s, sprints below 10.5 m/s.
  EXPECT_GT(fastestPass, 12.0f);
  EXPECT_LE(fastestPass, 25.5f);
  EXPECT_GT(fastestShot, 18.0f);
  EXPECT_LE(fastestShot, 34.0f);
  EXPECT_GT(fastestPlayer, 7.0f);
  EXPECT_LE(fastestPlayer, 10.5f);

  for (const PlayerMatchStats& entry : engine.getPlayerStats())
  {
    EXPECT_LE(entry.topSpeed, 10.5f);
    EXPECT_LE(entry.highIntensityMetres, entry.distanceMetres);
    EXPECT_LE(entry.sprintMetres, entry.highIntensityMetres);
    const bool fullMatch = entry.started && !entry.substitutedOff &&
                           !entry.sentOff && !entry.injured;
    if (!fullMatch || entry.role == PlayerRole::GK) continue;
    EXPECT_GE(entry.distanceMetres, 6'000.0f);
    EXPECT_LE(entry.distanceMetres, 14'000.0f);
    EXPECT_GT(entry.topSpeed, 6.5f);
  }
}

TEST(MatchEngineTest, PredictedHighlightHappensInTheLiveMatch)
{
  std::vector<std::unique_ptr<Player>> players;
  Team home = createSquadWithBench(1, "Home", 72, players);
  Team away = createSquadWithBench(2, "Away", 64, players);
  const StatsConfig config = createStatsConfig();
  MatchEngine engine(home.getLineup(), away.getLineup(), home.getStrategy(),
                     away.getStrategy(), config, 8'080);
  engine.advance(120.0f);
  const double now = engine.getSimulatedSeconds();
  const std::size_t knownHighlights = engine.getHighlights().size();

  const std::optional<MatchHighlight> predicted = engine.predictNextHighlight();
  ASSERT_TRUE(predicted.has_value());
  EXPECT_GE(predicted->startSeconds, now);
  EXPECT_GT(predicted->triggerSeconds, now);
  EXPECT_LE(predicted->startSeconds, predicted->triggerSeconds);
  EXPECT_GT(predicted->endSeconds, predicted->triggerSeconds);
  // Prediction runs on a copy: the live match has not moved.
  EXPECT_DOUBLE_EQ(engine.getSimulatedSeconds(), now);

  engine.advance(static_cast<float>(predicted->triggerSeconds - now) + 0.05f);
  ASSERT_GT(engine.getHighlights().size(), knownHighlights);
  const MatchHighlight& recorded = engine.getHighlights().back();
  EXPECT_NEAR(recorded.triggerSeconds, predicted->triggerSeconds, 0.001);
  EXPECT_EQ(recorded.type, predicted->type);
}

TEST(MatchEngineTest, HighlightPlaybackSkipsBetweenWindowsAndFinishes)
{
  std::vector<std::unique_ptr<Player>> players;
  Team home = createSquadWithBench(1, "Home", 70, players);
  Team away = createSquadWithBench(2, "Away", 70, players);
  const StatsConfig config = createStatsConfig();
  MatchEngine engine(home.getLineup(), away.getLineup(), home.getStrategy(),
                     away.getStrategy(), config, 606);
  engine.setPlaybackMode(MatchPlaybackMode::HIGHLIGHTS);
  engine.setHighlightPlaybackSpeed(4.0f);
  EXPECT_FLOAT_EQ(engine.getHighlightPlaybackSpeed(), 4.0f);

  constexpr float FRAME_SECONDS = 1.0f / 30.0f;
  int skips = 0;
  double watchedSeconds = 0.0;
  for (int frame = 0;
       frame < 400'000 && engine.getState() != MatchState::FULL_TIME; ++frame)
  {
    const double before = engine.getSimulatedSeconds();
    if (engine.advancePlayback(FRAME_SECONDS))
    {
      ++skips;
      continue;
    }
    ASSERT_TRUE(engine.getScheduledHighlight().has_value() ||
                engine.getState() == MatchState::FULL_TIME);
    watchedSeconds += engine.getSimulatedSeconds() - before;
  }
  EXPECT_EQ(engine.getState(), MatchState::FULL_TIME);
  EXPECT_GE(skips, 5);
  // Only the highlight windows are shown live.
  EXPECT_GT(watchedSeconds, 60.0);
  EXPECT_LT(watchedSeconds, engine.getSimulatedSeconds() * 0.5);
  EXPECT_FALSE(engine.getHighlights().empty());

  engine.setPlaybackSpeed(1'000.0f);
  EXPECT_FLOAT_EQ(engine.getPlaybackSpeed(), MatchTuning::Playback::MAX_SPEED);
}

TEST(MatchEngineTest, BenchConditionAndFamiliarityInputs)
{
  std::vector<std::unique_ptr<Player>> players;
  Team home = createSquadWithBench(1, "Home", 66, players);
  Team away = createSquadWithBench(2, "Away", 66, players);
  const StatsConfig config = createStatsConfig();
  MatchEngine engine(home.getLineup(), away.getLineup(), home.getStrategy(),
                     away.getStrategy(), config, 77);
  engine.setAutoSubstitutions(false, false);
  const Player* substitute = home.getLineup().getReserves()[3];
  ASSERT_TRUE(engine.setPlayerCondition(substitute->getId(), 0.6f));
  EXPECT_FALSE(engine.setPlayerCondition(999'999, 0.6f));

  engine.setTacticalFamiliarity(true, 3.0f);
  EXPECT_FLOAT_EQ(engine.getTacticalFamiliarity(true), 1.0f);
  engine.setTacticalFamiliarity(false, 0.25f);
  EXPECT_FLOAT_EQ(engine.getTacticalFamiliarity(false), 0.25f);

  ASSERT_TRUE(engine.substitutePlayer(105, substitute));
  EXPECT_FLOAT_EQ(engine.getPlayerCondition(substitute->getId()).value_or(1.0f),
                  0.6f);
}

namespace
{
float metresBetweenPoints(Vector2F first, Vector2F second)
{
  return std::hypot((first.x - second.x) * MatchTuning::Pitch::LENGTH_METRES,
                    (first.y - second.y) * MatchTuning::Pitch::WIDTH_METRES);
}

/** Metric distance from a point to the segment [start, end]. */
float metresToSegment(Vector2F point, Vector2F start, Vector2F end)
{
  const float sx = (end.x - start.x) * MatchTuning::Pitch::LENGTH_METRES;
  const float sy = (end.y - start.y) * MatchTuning::Pitch::WIDTH_METRES;
  const float px = (point.x - start.x) * MatchTuning::Pitch::LENGTH_METRES;
  const float py = (point.y - start.y) * MatchTuning::Pitch::WIDTH_METRES;
  const float squared = sx * sx + sy * sy;
  const float along =
      squared > 0.0f ? std::clamp((px * sx + py * sy) / squared, 0.0f, 1.0f)
                     : 0.0f;
  return std::hypot(px - sx * along, py - sy * along);
}
}  // namespace

TEST(MatchEngineTest, PlayersNeverExceedTheirTopSpeed)
{
  std::vector<std::unique_ptr<Player>> players;
  Team home = createSquadWithBench(1, "Home", 88, players);
  Team away = createSquadWithBench(2, "Away", 45, players);
  const StatsConfig config = createStatsConfig();
  MatchEngine engine(home.getLineup(), away.getLineup(), home.getStrategy(),
                     away.getStrategy(), config, 1'999);
  while (engine.getState() != MatchState::FULL_TIME)
  {
    engine.advance(MatchTuning::Timing::FIXED_STEP_SECONDS);
    for (const MatchPlayer& player : engine.getPlayers())
    {
      if (!player.onPitch) continue;
      const float pace = player.player->getStats().at("Pace") / 100.0f;
      const float topSpeed = MatchTuning::Player::TOP_SPEED_BASE +
                             pace * MatchTuning::Player::TOP_SPEED_PACE;
      ASSERT_LE(std::hypot(player.velocity.x, player.velocity.y),
                topSpeed + 0.01f)
          << "player " << player.player->getId();
    }
  }
}

TEST(MatchEngineTest, PossessionIsOnlyGainedWithinReach)
{
  std::vector<std::unique_ptr<Player>> players;
  Team home = createSquadWithBench(1, "Home", 70, players);
  Team away = createSquadWithBench(2, "Away", 70, players);
  const StatsConfig config = createStatsConfig();
  MatchEngine engine(home.getLineup(), away.getLineup(), home.getStrategy(),
                     away.getStrategy(), config, 7'007);
  // The ball is only won by getting a foot, the body or (a keeper) the hands
  // to it: its path during the step must pass within reach of the player's
  // body, allowing for his own movement in that step.
  constexpr float STEP = MatchTuning::Timing::FIXED_STEP_SECONDS;
  constexpr float SLACK = MatchTuning::Aerial::GOALKEEPER_CLAIM_RADIUS_METRES +
                          10.0f * STEP + 0.05f;
  const Player* previousOwner = engine.getBall().possessedBy;
  int gains = 0;
  while (engine.getState() != MatchState::FULL_TIME)
  {
    const MatchBall before = engine.getBall();
    const MatchState stateBefore = engine.getState();
    engine.advance(STEP);
    const MatchBall& after = engine.getBall();
    if (after.possessedBy && after.possessedBy != previousOwner &&
        !before.possessedBy && stateBefore == MatchState::PLAYING &&
        engine.getState() == MatchState::PLAYING)
    {
      const auto owner = std::ranges::find_if(
          engine.getPlayers(), [&after](const MatchPlayer& player)
          { return player.player == after.possessedBy; });
      ASSERT_NE(owner, engine.getPlayers().end());
      const Vector2F travel{before.velocity.x * STEP /
                                MatchTuning::Pitch::LENGTH_METRES,
                            before.velocity.y * STEP /
                                MatchTuning::Pitch::WIDTH_METRES};
      const Vector2F end{before.position.x + travel.x,
                         before.position.y + travel.y};
      EXPECT_LE(metresToSegment(owner->position, before.position, end), SLACK);
      ++gains;
    }
    previousOwner = after.possessedBy;
  }
  EXPECT_GT(gains, 100);
}

namespace
{
/**
 * Share of scenarios in which a lone dribbler gets past a defender jockeying
 * 3 m in front of him: after three seconds his side still has the ball, or
 * he has already used it (shot or pass).
 */
double dribblerBeatsTheDefender(int dribblerRating, int defenderRating)
{
  std::vector<std::unique_ptr<Player>> players;
  Team home = createDummyTeam(1, "Home", dribblerRating, players);
  Team away = createDummyTeam(2, "Away", defenderRating, players);
  const StatsConfig config = createStatsConfig();
  int beaten = 0;
  int duels = 0;
  for (std::uint32_t seed = 1; seed <= 160; ++seed)
  {
    MatchEngine engine(home.getLineup(), away.getLineup(), home.getStrategy(),
                       away.getStrategy(), config, seed);
    engine.setAutoSubstitutions(false, false);
    MatchScenario scenario;
    for (std::uint32_t index = 0; index < 11; ++index)
    {
      // Team-mates are far behind so the carrier has to take the man on.
      scenario.players.push_back(
          {100U + index, {0.02f + 0.005f * index, 0.1f + 0.08f * index}, false});
      scenario.players.push_back(
          {200U + index, {0.95f, 0.1f + 0.08f * index}, false});
    }
    scenario.players[18] = {109U, {0.70f, 0.50f}, false};
    scenario.players[19] = {209U, {0.73f, 0.50f}, false};
    scenario.players[1] = {200U, {0.97f, 0.50f}, false};
    scenario.carrierId = 109U;
    scenario.ballPosition = {0.70f, 0.50f};
    if (!engine.applyScenario(scenario)) continue;
    if (engine.getLastScenarioDecision().action != ScenarioAction::CARRY)
      continue;
    ++duels;
    const int usedBefore =
        engine.getStats().homePassesAttempted + engine.getStats().homeShots;
    engine.advance(3.0f);
    const MatchBall& ball = engine.getBall();
    const bool used = engine.getStats().homePassesAttempted +
                          engine.getStats().homeShots !=
                      usedBefore;
    const bool homeHasIt =
        ball.possessedBy ? ball.possessedBy->getTeamId() == 1
                         : ball.lastPossessor &&
                               ball.lastPossessor->getTeamId() == 1;
    if (used || homeHasIt) ++beaten;
  }
  return duels > 20 ? static_cast<double>(beaten) / duels : -1.0;
}
}  // namespace

TEST(MatchEngineTest, DribblerVersusJockeyingDefenderDependsOnSkill)
{
  const double skilled = dribblerBeatsTheDefender(85, 45);
  const double outclassed = dribblerBeatsTheDefender(45, 85);
  RecordProperty("skilled_keep_rate", std::to_string(skilled));
  RecordProperty("outclassed_keep_rate", std::to_string(outclassed));
  ASSERT_GE(skilled, 0.0) << "too few duels sampled";
  ASSERT_GE(outclassed, 0.0) << "too few duels sampled";
  // Neither side always wins, but skill clearly shifts the duel.
  EXPECT_GT(skilled, outclassed + 0.15);
  EXPECT_LT(skilled, 0.98);
  EXPECT_GT(outclassed, 0.02);
}

namespace
{
/** Mean own-goal depth of a side's outfield players over `seconds`. */
double meanOutfieldDepth(MatchEngine& engine, bool homeTeam, float seconds)
{
  double sum = 0.0;
  int samples = 0;
  const auto steps = static_cast<int>(
      seconds / MatchTuning::Timing::FIXED_STEP_SECONDS);
  for (int step = 0; step < steps; ++step)
  {
    engine.advance(MatchTuning::Timing::FIXED_STEP_SECONDS);
    for (const MatchPlayer& player : engine.getPlayers())
    {
      if (!player.onPitch || player.isGoalkeeper ||
          player.isHomeTeam != homeTeam)
        continue;
      sum += homeTeam ? player.position.x : 1.0f - player.position.x;
      ++samples;
    }
  }
  return samples > 0 ? sum / samples : 0.0;
}
}  // namespace

TEST(MatchEngineTest, MidMatchStrategyChangeMovesTheTeam)
{
  std::vector<std::unique_ptr<Player>> players;
  Team home = createSquadWithBench(1, "Home", 66, players);
  Team away = createSquadWithBench(2, "Away", 66, players);
  const StatsConfig config = createStatsConfig();
  double cautiousDepth = 0.0;
  double boldDepth = 0.0;
  for (std::uint32_t seed = 1; seed <= 6; ++seed)
  {
    MatchEngine cautious(home.getLineup(), away.getLineup(),
                         home.getStrategy(), away.getStrategy(), config, seed);
    MatchEngine bold(home.getLineup(), away.getLineup(), home.getStrategy(),
                     away.getStrategy(), config, seed);
    cautious.advance(120.0f);
    bold.advance(120.0f);
    StrategySliders low{0.1f, 0.2f, 0.1f, 0.5f, 0.9f};
    StrategySliders high{1.0f, 0.9f, 1.0f, 0.6f, 0.2f};
    Strategy cautiousPlan;
    cautiousPlan.setAllSliders(low);
    Strategy boldPlan;
    boldPlan.setAllSliders(high);
    cautious.setStrategy(true, cautiousPlan);
    bold.setStrategy(true, boldPlan);
    // Score effects temper the instruction once a goal has gone in.
    if (bold.getHomeScore() == bold.getAwayScore())
      EXPECT_FLOAT_EQ(bold.getEffectiveSliders(true).pressing, 1.0f);
    else
      EXPECT_GT(bold.getEffectiveSliders(true).pressing, 0.8f);
    cautiousDepth += meanOutfieldDepth(cautious, true, 600.0f);
    boldDepth += meanOutfieldDepth(bold, true, 600.0f);
  }
  EXPECT_GT(boldDepth, cautiousDepth * 1.03)
      << "bold tactics must push the team higher up the pitch";
}

TEST(MatchEngineTest, ShoutsNudgeTheSlidersAndFade)
{
  std::vector<std::unique_ptr<Player>> players;
  Team home = createSquadWithBench(1, "Home", 66, players);
  Team away = createSquadWithBench(2, "Away", 66, players);
  const StatsConfig config = createStatsConfig();
  MatchEngine engine(home.getLineup(), away.getLineup(), home.getStrategy(),
                     away.getStrategy(), config, 3);
  engine.setAutoSubstitutions(false, false);
  const float basePressing = engine.getEffectiveSliders(true).pressing;
  EXPECT_FLOAT_EQ(engine.getShoutStrength(true), 0.0f);
  engine.applyShout(true, MatchShout::PRESS_MORE);
  EXPECT_FLOAT_EQ(engine.getShoutStrength(true), 1.0f);
  EXPECT_GT(engine.getEffectiveSliders(true).pressing, basePressing + 0.2f);
  EXPECT_FLOAT_EQ(engine.getEffectiveSliders(false).pressing,
                  away.getStrategy().getSliders().pressing);
  engine.advance(MatchTuning::Touchline::SHOUT_DURATION_SECONDS * 0.5f);
  EXPECT_NEAR(engine.getShoutStrength(true), 0.5f, 0.01f);
  engine.advance(MatchTuning::Touchline::SHOUT_DURATION_SECONDS * 0.6f);
  EXPECT_FLOAT_EQ(engine.getShoutStrength(true), 0.0f);
  // Only the score effect remains once the shout has faded.
  const float scoreEffect =
      MatchTuning::Touchline::SCORE_EFFECT_MAX_GOALS *
      MatchTuning::Touchline::SCORE_EFFECT_PRESSING;
  EXPECT_NEAR(engine.getEffectiveSliders(true).pressing, basePressing,
              engine.getHomeScore() == engine.getAwayScore()
                  ? 1e-6f
                  : scoreEffect + 1e-6f);
}

TEST(MatchEngineTest, TeamTalkModifierIsBoundedAndDeterministic)
{
  std::vector<std::unique_ptr<Player>> players;
  Team home = createSquadWithBench(1, "Home", 66, players);
  Team away = createSquadWithBench(2, "Away", 66, players);
  const StatsConfig config = createStatsConfig();
  const auto play = [&](float modifier)
  {
    MatchEngine engine(home.getLineup(), away.getLineup(), home.getStrategy(),
                       away.getStrategy(), config, 99);
    engine.setTeamTalkModifier(true, 1, modifier);
    engine.setTeamTalkModifier(true, 2, modifier);
    engine.simulateToEnd();
    return engine.getStats().homePassesCompleted * 1000 +
           engine.getHomeScore() * 10 + engine.getAwayScore();
  };
  MatchEngine engine(home.getLineup(), away.getLineup(), home.getStrategy(),
                     away.getStrategy(), config, 99);
  engine.setTeamTalkModifier(true, 1, 0.5f);
  EXPECT_FLOAT_EQ(engine.getTeamTalkModifier(true, 1),
                  MatchTuning::Touchline::MAX_TEAM_TALK_MODIFIER);
  engine.setTeamTalkModifier(false, 2, -0.5f);
  EXPECT_FLOAT_EQ(engine.getTeamTalkModifier(false, 2),
                  -MatchTuning::Touchline::MAX_TEAM_TALK_MODIFIER);
  EXPECT_FLOAT_EQ(engine.getTeamTalkModifier(true, 3), 0.0f);
  EXPECT_EQ(play(0.012f), play(0.012f));
}

namespace
{
/** Advances until an outfield player of `homeTeam` holds the ball. */
const MatchPlayer* advanceUntilOutfieldCarrier(MatchEngine& engine,
                                               bool homeTeam)
{
  for (int step = 0; step < 20'000; ++step)
  {
    engine.advance(MatchTuning::Timing::FIXED_STEP_SECONDS);
    if (engine.getState() != MatchState::PLAYING) continue;
    const Player* holder = engine.getBall().possessedBy;
    if (!holder) continue;
    for (const auto& candidate : engine.getPlayers())
    {
      if (candidate.player == holder && candidate.isHomeTeam == homeTeam &&
          !candidate.isGoalkeeper)
        return &candidate;
    }
  }
  return nullptr;
}

const MatchPlayer* findOnPitch(const MatchEngine& engine, PlayerID id)
{
  for (const auto& candidate : engine.getPlayers())
    if (candidate.player && candidate.player->getId() == id) return &candidate;
  return nullptr;
}
}  // namespace

TEST(MatchEngineTest, ControlledPlayerFollowsTheStickWithinHisPhysics)
{
  std::vector<std::unique_ptr<Player>> players;
  Team home = createSquadWithBench(1, "Home", 70, players);
  Team away = createSquadWithBench(2, "Away", 70, players);
  const StatsConfig config = createStatsConfig();
  MatchEngine engine(home.getLineup(), away.getLineup(), home.getStrategy(),
                     away.getStrategy(), config, 17);
  engine.setAutoSubstitutions(false, false);
  engine.advance(30.0f);
  for (int step = 0; step < 2'000 && engine.getState() != MatchState::PLAYING;
       ++step)
    engine.advance(MatchTuning::Timing::FIXED_STEP_SECONDS);
  ASSERT_EQ(engine.getState(), MatchState::PLAYING);

  // Goalkeepers and unknown players cannot be taken over.
  const PlayerID keeper = home.getLineup().getGoalkeeper()->getId();
  EXPECT_FALSE(engine.setControlledPlayer(keeper));
  EXPECT_FALSE(engine.setControlledPlayer(999'999));
  EXPECT_EQ(engine.getControlledPlayer(), 0u);

  // A centre-back runs across the pitch toward the touchline on the stick.
  const PlayerID controlled = 102;
  ASSERT_TRUE(engine.setControlledPlayer(controlled));
  EXPECT_EQ(engine.getControlledPlayer(), controlled);
  MatchPlayerInput input;
  input.moveY = engine.getBall().position.y < 0.5f ? 1.0f : -1.0f;
  input.sprint = true;
  engine.submitInput(input);
  const MatchPlayer* player = findOnPitch(engine, controlled);
  ASSERT_NE(player, nullptr);
  const Vector2F start = player->position;
  float fastest = 0.0f;
  for (int step = 0; step < 25; ++step)
  {
    engine.advance(MatchTuning::Timing::FIXED_STEP_SECONDS);
    player = findOnPitch(engine, controlled);
    ASSERT_NE(player, nullptr);
    const float speed = std::hypot(player->velocity.x, player->velocity.y);
    EXPECT_LE(speed, player->maxSpeed + 1e-3f);
    // The run follows the stick: sideways, barely any drift along x.
    if (speed > 1.0f)
      EXPECT_GT(std::abs(player->velocity.y), 4.0f * std::abs(player->velocity.x));
    fastest = std::max(fastest, speed);
  }
  // Accelerates realistically: a hard 2.5 s run nears top speed.
  EXPECT_GT(fastest, 0.8f * player->maxSpeed);
  const float runMetres =
      std::abs(player->position.y - start.y) * MatchTuning::Pitch::WIDTH_METRES;
  EXPECT_GT(runMetres, 10.0f);
  EXPECT_LT(runMetres, 25.0f);

  // Releasing the stick stops him under his braking limit.
  engine.submitInput(MatchPlayerInput{});
  engine.advance(1.5f);
  player = findOnPitch(engine, controlled);
  EXPECT_LT(std::hypot(player->velocity.x, player->velocity.y), 0.5f);

  // Handing him back to the AI.
  ASSERT_TRUE(engine.setControlledPlayer(0));
  engine.advance(MatchTuning::Timing::FIXED_STEP_SECONDS);
  EXPECT_EQ(engine.getControlledPlayer(), 0u);
}

TEST(MatchEngineTest, ControlledCarrierPassesTowardTheAim)
{
  std::vector<std::unique_ptr<Player>> players;
  Team home = createSquadWithBench(1, "Home", 70, players);
  Team away = createSquadWithBench(2, "Away", 60, players);
  const StatsConfig config = createStatsConfig();
  MatchEngine engine(home.getLineup(), away.getLineup(), home.getStrategy(),
                     away.getStrategy(), config, 23);
  engine.setAutoSubstitutions(false, false);
  engine.advance(20.0f);
  const MatchPlayer* carrier = advanceUntilOutfieldCarrier(engine, true);
  ASSERT_NE(carrier, nullptr);
  const PlayerID carrierId = carrier->player->getId();
  const Vector2F from = carrier->position;
  ASSERT_TRUE(engine.setControlledPlayer(carrierId));

  // Aim at the farthest-away team-mate: the pass goes to the team-mate best
  // aligned with that direction (usually him), never behind the aim.
  const MatchPlayer* aimed = nullptr;
  float farthest = 0.0f;
  for (const auto& mate : engine.getPlayers())
  {
    if (!mate.player || !mate.onPitch || mate.isHomeTeam != true ||
        mate.isGoalkeeper || mate.player->getId() == carrierId)
      continue;
    const float metres = std::hypot((mate.position.x - from.x) * 105.0f,
                                    (mate.position.y - from.y) * 68.0f);
    if (metres > farthest && metres < 40.0f)
    {
      farthest = metres;
      aimed = &mate;
    }
  }
  ASSERT_NE(aimed, nullptr);
  MatchPlayerInput input;
  input.aimX = (aimed->position.x - from.x) * 105.0f;
  input.aimY = (aimed->position.y - from.y) * 68.0f;
  input.action = MatchInputAction::PASS;
  engine.submitInput(input);
  bool passed = false;
  const int buffered = static_cast<int>(
      MatchTuning::Control::ACTION_BUFFER_SECONDS /
      MatchTuning::Timing::FIXED_STEP_SECONDS);
  for (int step = 0; step < buffered && !passed; ++step)
  {
    engine.advance(MatchTuning::Timing::FIXED_STEP_SECONDS);
    passed = engine.getBall().possessedBy != carrier->player &&
             engine.getBall().isPass;
  }
  ASSERT_TRUE(passed);
  const Player* receiver = engine.getBall().intendedReceiver;
  ASSERT_NE(receiver, nullptr);
  const MatchPlayer* target = findOnPitch(engine, receiver->getId());
  ASSERT_NE(target, nullptr);
  const float toTargetX = (target->position.x - from.x) * 105.0f;
  const float toTargetY = (target->position.y - from.y) * 68.0f;
  const float alignment =
      (toTargetX * input.aimX + toTargetY * input.aimY) /
      (std::hypot(toTargetX, toTargetY) * std::hypot(input.aimX, input.aimY));
  EXPECT_GT(alignment, MatchTuning::Control::PASS_MIN_ALIGNMENT - 0.1f);
}

TEST(MatchEngineTest, RecordedControllerInputReplaysIdentically)
{
  std::vector<std::unique_ptr<Player>> players;
  Team home = createSquadWithBench(1, "Home", 68, players);
  Team away = createSquadWithBench(2, "Away", 67, players);
  const StatsConfig config = createStatsConfig();
  MatchEngine live(home.getLineup(), away.getLineup(), home.getStrategy(),
                   away.getStrategy(), config, 4'242);
  // A scripted "human": takes a striker, runs, sprints, presses buttons.
  ASSERT_TRUE(live.setControlledPlayer(109));
  for (int second = 0; second < 900; ++second)
  {
    MatchPlayerInput input;
    const float phase = static_cast<float>(second) * 0.37f;
    input.moveX = std::cos(phase);
    input.moveY = std::sin(phase * 1.3f);
    input.sprint = second % 7 < 3;
    if (second % 11 == 0) input.action = MatchInputAction::PASS;
    if (second % 29 == 0) input.action = MatchInputAction::SHOOT;
    if (second % 13 == 0) input.action = MatchInputAction::TACKLE;
    live.submitInput(input);
    live.update(0.5f);
    live.update(0.5f);
  }
  ASSERT_TRUE(live.setControlledPlayer(0));
  live.simulateToEnd();
  ASSERT_EQ(live.getState(), MatchState::FULL_TIME);
  ASSERT_GT(live.getInputLog().size(), 900u);

  MatchEngine replay(home.getLineup(), away.getLineup(), home.getStrategy(),
                     away.getStrategy(), config, 4'242);
  replay.loadInputReplay(live.getInputLog());
  replay.simulateToEnd();
  EXPECT_EQ(replay.getHomeScore(), live.getHomeScore());
  EXPECT_EQ(replay.getAwayScore(), live.getAwayScore());
  EXPECT_EQ(replay.getEvents().size(), live.getEvents().size());
  EXPECT_EQ(replay.getStats().homePassesCompleted,
            live.getStats().homePassesCompleted);
  EXPECT_DOUBLE_EQ(replay.getSimulatedSeconds(), live.getSimulatedSeconds());
  ASSERT_EQ(replay.getPlayerStats().size(), live.getPlayerStats().size());
  for (std::size_t index = 0; index < live.getPlayerStats().size(); ++index)
    EXPECT_FLOAT_EQ(replay.getPlayerStats()[index].distanceMetres,
                    live.getPlayerStats()[index].distanceMetres);

  // The controlled run really changed the match versus pure AI.
  MatchEngine ai(home.getLineup(), away.getLineup(), home.getStrategy(),
                 away.getStrategy(), config, 4'242);
  ai.simulateToEnd();
  EXPECT_NE(ai.getEvents().size() * 1000 + ai.getStats().homePassesCompleted,
            live.getEvents().size() * 1000 + live.getStats().homePassesCompleted);
}
