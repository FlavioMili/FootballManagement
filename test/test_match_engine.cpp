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
#include <filesystem>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "global/runtime_paths.h"
#include "gui/render/match_render_snapshot.h"
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

void simulateToFullTime(MatchEngine& engine, float frameDelta = 0.05f)
{
  for (int frame = 0;
       frame < 10'000 && engine.getState() != MatchState::FULL_TIME; ++frame)
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
  EXPECT_GE(stats.homeShots + stats.awayShots, 4);
  EXPECT_LE(stats.homeShots + stats.awayShots, 40);
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
  engine.update(1.0f / 60.0f + 0.005f);
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
  engine.update(TEST_FRAME_SECONDS);
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
  constexpr int MAX_OBSERVATION_FRAMES = 1'800;
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
  constexpr int MAX_OBSERVATION_FRAMES = 1'800;
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

    // Possession can be established after movement resolution. Advance once
    // so the team has applied its possession-phase assignments.
    engine.update(TEST_FRAME_SECONDS);
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
  Team strong = createDummyTeam(1, "Strong", 88, players);
  Team weak = createDummyTeam(2, "Weak", 42, players);
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
       frame < 10'000 && engine.getState() != MatchState::FULL_TIME; ++frame)
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
    if (event.description.rfind("GOAL!", 0) == 0) ++goalsByEvent;

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
      EXPECT_EQ(event.description.rfind("GOAL!", 0), 0u);
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
  const auto started = std::chrono::steady_clock::now();
  constexpr int MATCHES = 10;
  for (uint32_t seed = 1; seed <= MATCHES; ++seed)
  {
    MatchEngine engine(home.getLineup(), away.getLineup(), home.getStrategy(),
                       away.getStrategy(), config, seed);
    simulateToFullTime(engine, 0.25f);
    EXPECT_EQ(engine.getState(), MatchState::FULL_TIME);
  }
  const double milliseconds = std::chrono::duration<double, std::milli>(
                                  std::chrono::steady_clock::now() - started)
                                  .count() /
                              MATCHES;
  RecordProperty("milliseconds_per_match", std::to_string(milliseconds));
  std::printf("[timing] headless match %.2f ms\n", milliseconds);
  EXPECT_LT(milliseconds, 100.0);
}
