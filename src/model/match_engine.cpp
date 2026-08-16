// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "model/match_engine.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <numbers>
#include <sstream>

#include "model/player.h"
#include "model/role_utils.h"

namespace
{
constexpr float EPSILON = 0.00001f;

std::uint32_t makeNonBlockingMatchSeed()
{
  static std::atomic<std::uint32_t> sequence{0};
  const auto clockValue = static_cast<std::uint64_t>(
      std::chrono::steady_clock::now().time_since_epoch().count());
  const std::uint64_t mixed =
      clockValue ^ (static_cast<std::uint64_t>(++sequence) << 32U);
  return static_cast<std::uint32_t>(mixed ^ (mixed >> 32U));
}

float length(Vector2F vector)
{
  return std::sqrt(vector.x * vector.x + vector.y * vector.y);
}

float distance(Vector2F first, Vector2F second)
{
  return length({first.x - second.x, first.y - second.y});
}

Vector2F normalized(Vector2F vector)
{
  const float magnitude = length(vector);
  if (magnitude <= EPSILON) return {0.0f, 0.0f};
  return {vector.x / magnitude, vector.y / magnitude};
}

bool stateClockRuns(MatchState state)
{
  return state != MatchState::KICK_OFF && state != MatchState::HALF_TIME &&
         state != MatchState::FULL_TIME;
}

std::string_view stateName(MatchState state)
{
  switch (state)
  {
    case MatchState::KICK_OFF:
      return "kick_off";
    case MatchState::PLAYING:
      return "playing";
    case MatchState::THROW_IN:
      return "throw_in";
    case MatchState::GOAL_KICK:
      return "goal_kick";
    case MatchState::CORNER_KICK:
      return "corner_kick";
    case MatchState::FREE_KICK:
      return "free_kick";
    case MatchState::PENALTY:
      return "penalty";
    case MatchState::GOAL:
      return "goal";
    case MatchState::HALF_TIME:
      return "half_time";
    case MatchState::FULL_TIME:
      return "full_time";
  }
  return "unknown";
}

std::string_view teamPhaseName(TeamPhase phase)
{
  switch (phase)
  {
    case TeamPhase::STOPPAGE:
      return "stoppage";
    case TeamPhase::SET_PIECE:
      return "set_piece";
    case TeamPhase::DEFENSIVE_BLOCK:
      return "defensive_block";
    case TeamPhase::DEFENSIVE_TRANSITION:
      return "defensive_transition";
    case TeamPhase::ATTACKING_TRANSITION:
      return "attacking_transition";
    case TeamPhase::POSSESSION:
      return "possession";
    case TeamPhase::FINAL_THIRD:
      return "final_third";
  }
  return "unknown";
}

std::string_view intentName(PlayerIntent intent)
{
  switch (intent)
  {
    case PlayerIntent::HOLD_SHAPE:
      return "hold_shape";
    case PlayerIntent::CARRY_BALL:
      return "carry_ball";
    case PlayerIntent::OFFER_SUPPORT:
      return "offer_support";
    case PlayerIntent::RECEIVE_PASS:
      return "receive_pass";
    case PlayerIntent::RUN_IN_BEHIND:
      return "run_in_behind";
    case PlayerIntent::ATTACK_BOX:
      return "attack_box";
    case PlayerIntent::OVERLAP:
      return "overlap";
    case PlayerIntent::PRESS_BALL:
      return "press_ball";
    case PlayerIntent::COVER_PRESS:
      return "cover_press";
    case PlayerIntent::BLOCK_PASSING_LANE:
      return "block_passing_lane";
    case PlayerIntent::MARK_OPPONENT:
      return "mark_opponent";
    case PlayerIntent::CLAIM_LOOSE_BALL:
      return "claim_loose_ball";
    case PlayerIntent::RECOVER_SHAPE:
      return "recover_shape";
    case PlayerIntent::GOALKEEP:
      return "goalkeep";
  }
  return "unknown";
}

float movementSpeedScale(PlayerIntent intent)
{
  switch (intent)
  {
    case PlayerIntent::CARRY_BALL:
      return MatchTuning::Player::CARRY_BALL_SPEED_SCALE;
    case PlayerIntent::OFFER_SUPPORT:
      return MatchTuning::Player::SUPPORT_SPEED_SCALE;
    case PlayerIntent::RECEIVE_PASS:
      return MatchTuning::Player::ATTACKING_RUN_SPEED_SCALE;
    case PlayerIntent::RUN_IN_BEHIND:
    case PlayerIntent::ATTACK_BOX:
    case PlayerIntent::OVERLAP:
      return MatchTuning::Player::ATTACKING_RUN_SPEED_SCALE;
    case PlayerIntent::PRESS_BALL:
    case PlayerIntent::CLAIM_LOOSE_BALL:
      return MatchTuning::Player::PRESS_SPEED_SCALE;
    case PlayerIntent::COVER_PRESS:
    case PlayerIntent::BLOCK_PASSING_LANE:
      return MatchTuning::Player::COVER_SPEED_SCALE;
    case PlayerIntent::MARK_OPPONENT:
      return MatchTuning::Player::MARKING_SPEED_SCALE;
    case PlayerIntent::RECOVER_SHAPE:
      return MatchTuning::Player::RECOVERY_SPEED_SCALE;
    case PlayerIntent::GOALKEEP:
      return MatchTuning::Player::GOALKEEPER_MOVEMENT_SPEED_SCALE;
    case PlayerIntent::HOLD_SHAPE:
      return MatchTuning::Player::HOLD_SHAPE_SPEED_SCALE;
  }
  return MatchTuning::Player::HOLD_SHAPE_SPEED_SCALE;
}

std::string_view passIntentName(PassIntent intent)
{
  switch (intent)
  {
    case PassIntent::RECYCLE:
      return "recycle";
    case PassIntent::PROGRESSIVE:
      return "progressive";
    case PassIntent::THROUGH_BALL:
      return "through_ball";
    case PassIntent::CROSS:
      return "cross";
    case PassIntent::CUTBACK:
      return "cutback";
    case PassIntent::SWITCH_PLAY:
      return "switch_play";
    case PassIntent::PRESSURE_RELEASE:
      return "pressure_release";
    case PassIntent::SET_PIECE:
      return "set_piece";
  }
  return "unknown";
}

std::string_view scenarioActionName(ScenarioAction action)
{
  switch (action)
  {
    case ScenarioAction::SHOT:
      return "shot";
    case ScenarioAction::PASS:
      return "pass";
    case ScenarioAction::CARRY:
      return "carry";
    case ScenarioAction::SHIELD:
      return "shield";
    case ScenarioAction::NONE:
      return "none";
  }
  return "unknown";
}
}  // namespace

MatchEngine::MatchEngine(const Lineup& home_lineup, const Lineup& away_lineup,
                         const Strategy& home_strat, const Strategy& away_strat,
                         const StatsConfig& config)
    : MatchEngine(home_lineup, away_lineup, home_strat, away_strat, config,
                  makeNonBlockingMatchSeed())
{
}

MatchEngine::MatchEngine(const Lineup& home_lineup, const Lineup& away_lineup,
                         const Strategy& home_strat, const Strategy& away_strat,
                         const StatsConfig& config, uint32_t seed)
    : statsConfig(config),
      homeStrategy(home_strat),
      awayStrategy(away_strat),
      rng(seed)
{
  players.reserve(22);
  initializePlayers(home_lineup, true);
  initializePlayers(away_lineup, false);
  setupKickOff(true);
  updateTeamPhases();
  logEvent("Kick-off");
}

void MatchEngine::initializePlayers(const Lineup& lineup, bool isHomeTeam)
{
  const auto addPlayer = [&](const Player* player, Vector2F position,
                             std::vector<MatchPlayer>& destination)
  {
    if (!player) return;
    if (!isHomeTeam) position.x = 1.0f - position.x;

    MatchPlayer matchPlayer;
    matchPlayer.player = player;
    matchPlayer.isHomeTeam = isHomeTeam;
    matchPlayer.position = position;
    matchPlayer.basePosition = position;
    matchPlayer.movementTarget = position;
    matchPlayer.facingAngle = isHomeTeam ? 0.0f : std::numbers::pi_v<float>;
    matchPlayer.targetAngle = matchPlayer.facingAngle;
    matchPlayer.pace = attribute(player, "Pace");
    matchPlayer.shooting = attribute(player, "Shooting");
    matchPlayer.passing = attribute(player, "Passing");
    matchPlayer.dribbling = attribute(player, "Dribbling");
    matchPlayer.defending = attribute(player, "Defending");
    matchPlayer.goalkeeping = attribute(player, "Goalkeeping");
    matchPlayer.maxSpeed =
        MatchTuning::Player::BASE_MAX_SPEED +
        matchPlayer.pace * MatchTuning::Player::PACE_SPEED_BONUS;
    matchPlayer.acceleration =
        MatchTuning::Player::BASE_ACCELERATION +
        matchPlayer.pace * MatchTuning::Player::PACE_ACCELERATION_BONUS;
    destination.push_back(matchPlayer);
  };

  addPlayer(
      lineup.getGoalkeeper(),
      {MatchTuning::Pitch::LINEUP_GOALKEEPER_X, MatchTuning::Pitch::CENTRE},
      players);
  for (const auto& positioned : lineup.getOutfieldPlayers())
  {
    addPlayer(positioned.player, positioned.position, players);
  }
}

void MatchEngine::update(float deltaTime)
{
  lastUpdateStepCount = 0;
  if (state == MatchState::FULL_TIME || !std::isfinite(deltaTime) ||
      deltaTime <= 0.0f)
  {
    return;
  }

  accumulator +=
      std::min(deltaTime, MatchTuning::Timing::MAX_FRAME_DELTA_SECONDS);
  while (accumulator + EPSILON >= MatchTuning::Timing::FIXED_STEP_SECONDS &&
         state != MatchState::FULL_TIME &&
         lastUpdateStepCount < MatchTuning::Timing::MAX_FIXED_STEPS_PER_UPDATE)
  {
    simulateStep(MatchTuning::Timing::FIXED_STEP_SECONDS);
    accumulator -= MatchTuning::Timing::FIXED_STEP_SECONDS;
    ++lastUpdateStepCount;
  }
  if (lastUpdateStepCount == MatchTuning::Timing::MAX_FIXED_STEPS_PER_UPDATE &&
      accumulator >= MatchTuning::Timing::FIXED_STEP_SECONDS)
  {
    droppedSimulationSteps += static_cast<std::uint64_t>(
        std::floor(accumulator / MatchTuning::Timing::FIXED_STEP_SECONDS));
    accumulator =
        std::fmod(accumulator, MatchTuning::Timing::FIXED_STEP_SECONDS);
  }
}

void MatchEngine::captureInterpolationFrame()
{
  previousPlayerPositions.resize(players.size());
  previousPlayerFacingAngles.resize(players.size());
  for (std::size_t index = 0; index < players.size(); ++index)
  {
    previousPlayerPositions[index] = players[index].position;
    previousPlayerFacingAngles[index] = players[index].facingAngle;
  }
  previousBallPosition = ball.position;
  previousBallZ = ball.z;
}

float MatchEngine::getInterpolationAlpha() const
{
  return std::clamp(accumulator / MatchTuning::Timing::FIXED_STEP_SECONDS, 0.0f,
                    1.0f);
}

void MatchEngine::simulateStep(float dt)
{
  captureInterpolationFrame();
  for (auto& player : players)
  {
    player.tackleCooldown = std::max(0.0f, player.tackleCooldown - dt);
    player.actionCooldown = std::max(0.0f, player.actionCooldown - dt);
    player.trapTimer = std::max(0.0f, player.trapTimer - dt);
    player.diveTimer = std::max(0.0f, player.diveTimer - dt);
    player.isTrapping = player.trapTimer > 0.0f;
    player.isDiving = player.diveTimer > 0.0f;
  }
  if (state == MatchState::PLAYING)
  {
    transitionSecondsRemaining =
        std::max(0.0f, transitionSecondsRemaining - dt);
  }
  updateTeamPhases();

  if (state == MatchState::GOAL)
  {
    // The scored ball keeps travelling into the net while the teams
    // celebrate; only then does the kick-off restart occur.
    goalCelebrationRemaining = std::max(0.0f, goalCelebrationRemaining - dt);
    if (!ball.possessedBy) updateBallInNet(dt);
    if (goalCelebrationRemaining <= 0.0f) setupKickOff(!goalScoredByHome);
    return;
  }

  if (state == MatchState::HALF_TIME)
  {
    setPieceTimer -= dt;
    if (setPieceTimer <= 0.0f)
    {
      setupKickOff(false);
      logEvent("Second half");
    }
    return;
  }

  if (state != MatchState::PLAYING)
  {
    if (stateClockRuns(state)) matchTimeMinutes += dt;
    setPieceTimer -= dt;
    if (setPieceTimer <= 0.0f) completeRestart();
  }
  else
  {
    matchTimeMinutes += dt * MatchTuning::Timing::MATCH_MINUTES_PER_REAL_SECOND;
    updateMovement(dt);
    resolvePossessionAndActions(dt);

    if (!ball.possessedBy && state == MatchState::PLAYING)
    {
      updateBall(dt);
      checkOutOfBounds();
      if (state == MatchState::PLAYING) resolveLooseBall();
    }
  }

  if (!firstHalfComplete &&
      matchTimeMinutes >= MatchTuning::Timing::HALF_TIME_MINUTE)
  {
    firstHalfComplete = true;
    matchTimeMinutes = MatchTuning::Timing::HALF_TIME_MINUTE;
    state = MatchState::HALF_TIME;
    setPieceTimer = MatchTuning::Timing::HALF_TIME_PAUSE_SECONDS;
    restartTaker = nullptr;
    ball.possessedBy = nullptr;
    ball.velocity = {0.0f, 0.0f};
    updateTeamPhases();
    logEvent("Half-time");
  }
  else if (firstHalfComplete &&
           matchTimeMinutes >= MatchTuning::Timing::FULL_TIME_MINUTE)
  {
    matchTimeMinutes = MatchTuning::Timing::FULL_TIME_MINUTE;
    state = MatchState::FULL_TIME;
    restartTaker = nullptr;
    ball.possessedBy = nullptr;
    ball.velocity = {0.0f, 0.0f};
    updateTeamPhases();
    logEvent("Full-time");
  }
}

void MatchEngine::updateTeamPhases()
{
  if (state == MatchState::HALF_TIME || state == MatchState::FULL_TIME)
  {
    homePhase = TeamPhase::STOPPAGE;
    awayPhase = TeamPhase::STOPPAGE;
    return;
  }
  if (state != MatchState::PLAYING)
  {
    homePhase = TeamPhase::SET_PIECE;
    awayPhase = TeamPhase::SET_PIECE;
    transitionSecondsRemaining = 0.0f;
    if (const MatchPlayer* restartOwner = findMatchPlayer(ball.possessedBy))
      lastControlledTeamHome = restartOwner->isHomeTeam;
    return;
  }

  const MatchPlayer* carrier = findMatchPlayer(ball.possessedBy);
  if (carrier && !lastControlledTeamHome)
    lastControlledTeamHome = carrier->isHomeTeam;

  const auto setTransitionPhases = [&](bool attackingHome)
  {
    homePhase = attackingHome ? TeamPhase::ATTACKING_TRANSITION
                              : TeamPhase::DEFENSIVE_TRANSITION;
    awayPhase = attackingHome ? TeamPhase::DEFENSIVE_TRANSITION
                              : TeamPhase::ATTACKING_TRANSITION;
  };
  if (transitionSecondsRemaining > 0.0f && lastControlledTeamHome)
  {
    setTransitionPhases(*lastControlledTeamHome);
    return;
  }

  const auto setControlledPhases = [&](bool attackingHome, float ballX)
  {
    const bool finalThird =
        attackingHome ? ballX >= MatchTuning::Rules::HOME_FINAL_THIRD_START
                      : ballX <= MatchTuning::Rules::AWAY_FINAL_THIRD_START;
    const TeamPhase attackingPhase =
        finalThird ? TeamPhase::FINAL_THIRD : TeamPhase::POSSESSION;
    homePhase = attackingHome ? attackingPhase : TeamPhase::DEFENSIVE_BLOCK;
    awayPhase = attackingHome ? TeamPhase::DEFENSIVE_BLOCK : attackingPhase;
  };
  if (carrier)
  {
    setControlledPhases(carrier->isHomeTeam, carrier->position.x);
    return;
  }

  // An intentional pass or shot is still part of the attacking team's
  // controlled phase. Treating every ball flight as a transition made both
  // teams repeatedly abandon their coordinated support and defensive shape.
  if ((ball.isPass || ball.isShot) && lastControlledTeamHome)
  {
    setControlledPhases(*lastControlledTeamHome, ball.position.x);
    return;
  }

  if (!lastControlledTeamHome)
  {
    homePhase = TeamPhase::DEFENSIVE_TRANSITION;
    awayPhase = TeamPhase::DEFENSIVE_TRANSITION;
    return;
  }

  setTransitionPhases(*lastControlledTeamHome);
}

void MatchEngine::updateMovement(float dt)
{
  const MatchPlayer* carrier = findMatchPlayer(ball.possessedBy);
  const MatchPlayer* transitionSource = findMatchPlayer(ball.lastPossessor);
  const Vector2F pressurePosition = carrier ? carrier->position : ball.position;
  const StrategySliders homeSliders = homeStrategy.getSliders();
  const StrategySliders awaySliders = awayStrategy.getSliders();

  const auto estimatedArrivalTime = [&](const MatchPlayer& candidate)
  {
    const Vector2F toTarget{pressurePosition.x - candidate.position.x,
                            pressurePosition.y - candidate.position.y};
    const float targetDistance = length(toTarget);
    if (targetDistance <= EPSILON) return 0.0f;
    const Vector2F pursuitDirection = normalized(toTarget);
    const float velocityTowardTarget =
        std::max(0.0f, candidate.velocity.x * pursuitDirection.x +
                           candidate.velocity.y * pursuitDirection.y);
    const float staminaSpeed =
        MatchTuning::Player::STAMINA_SPEED_BASE +
        candidate.stamina * MatchTuning::Player::STAMINA_SPEED_BONUS;
    const float pursuitSpeed =
        std::max(candidate.maxSpeed * staminaSpeed +
                     velocityTowardTarget *
                         MatchTuning::Player::CURRENT_VELOCITY_PURSUIT_WEIGHT,
                 MatchTuning::Player::MINIMUM_PURSUIT_SPEED);
    float arrivalTime = targetDistance / pursuitSpeed;
    if (candidate.intent == PlayerIntent::PRESS_BALL ||
        candidate.intent == PlayerIntent::CLAIM_LOOSE_BALL)
    {
      arrivalTime = std::max(
          0.0f, arrivalTime - MatchTuning::Shape::PRESSER_CONTINUITY_SECONDS);
    }
    return arrivalTime;
  };

  const auto closestOutfieldPair = [&](bool homeTeam)
  {
    std::array<MatchPlayer*, 2> closest{nullptr, nullptr};
    std::array<float, 2> arrivalTimes{std::numeric_limits<float>::max(),
                                      std::numeric_limits<float>::max()};
    for (auto& candidate : players)
    {
      if (!candidate.player || candidate.isHomeTeam != homeTeam ||
          candidate.player->getRole() == PlayerRole::GK)
      {
        continue;
      }
      const float arrivalTime = estimatedArrivalTime(candidate);
      if (arrivalTime < arrivalTimes[0])
      {
        closest[1] = closest[0];
        arrivalTimes[1] = arrivalTimes[0];
        closest[0] = &candidate;
        arrivalTimes[0] = arrivalTime;
      }
      else if (arrivalTime < arrivalTimes[1])
      {
        closest[1] = &candidate;
        arrivalTimes[1] = arrivalTime;
      }
    }

    // A purposeful pass creates a receiving run. Prioritizing that receiver
    // prevents a merely nearby teammate from making the play look arbitrary.
    if (!carrier && ball.isPass && ball.passByHome == homeTeam)
    {
      MatchPlayer* intended = findMatchPlayer(ball.intendedReceiver);
      if (intended && intended->player &&
          intended->player->getRole() != PlayerRole::GK &&
          closest[0] != intended)
      {
        closest[1] = closest[0];
        closest[0] = intended;
      }
    }
    return closest;
  };

  const auto homePressers = closestOutfieldPair(true);
  const auto awayPressers = closestOutfieldPair(false);

  const auto runPriority = [&](const MatchPlayer& candidate)
  {
    float rolePriority = MatchTuning::Shape::MIDFIELDER_RUN_PRIORITY;
    switch (candidate.player->getRole())
    {
      case PlayerRole::ST:
        rolePriority = MatchTuning::Shape::STRIKER_RUN_PRIORITY;
        break;
      case PlayerRole::LW:
      case PlayerRole::RW:
        rolePriority = MatchTuning::Shape::WINGER_RUN_PRIORITY;
        break;
      case PlayerRole::CAM:
        rolePriority = MatchTuning::Shape::ATTACKING_MIDFIELDER_RUN_PRIORITY;
        break;
      case PlayerRole::CDM:
      case PlayerRole::CM:
      case PlayerRole::LM:
      case PlayerRole::RM:
        break;
      default:
        return -std::numeric_limits<float>::infinity();
    }
    const float depth = candidate.isHomeTeam ? candidate.position.x
                                             : 1.0f - candidate.position.x;
    const float separation =
        carrier ? std::abs(candidate.position.y - carrier->position.y) : 0.0f;
    return rolePriority +
           candidate.pace * MatchTuning::Shape::RUN_PACE_PRIORITY +
           depth * MatchTuning::Shape::RUN_DEPTH_PRIORITY +
           separation * MatchTuning::Shape::RUN_SEPARATION_PRIORITY +
           (candidate.isMakingRun ? MatchTuning::Shape::RUN_CONTINUITY_PRIORITY
                                  : 0.0f);
  };

  const auto selectAttackingRunners = [&](bool homeTeam)
  {
    std::array<MatchPlayer*, MatchTuning::Shape::MAX_COMMITTED_RUNNERS>
        runners{};
    std::array<float, MatchTuning::Shape::MAX_COMMITTED_RUNNERS> scores;
    scores.fill(-std::numeric_limits<float>::infinity());
    const bool possessionContext = carrier && carrier->isHomeTeam == homeTeam;
    const bool looseTransitionContext =
        !carrier && !ball.isPass && transitionSource &&
        transitionSource->isHomeTeam == homeTeam;
    if (!possessionContext && !looseTransitionContext) return runners;

    const StrategySliders& sliders = homeTeam ? homeSliders : awaySliders;
    const float attackingPosition =
        carrier ? carrier->position.x : ball.position.x;
    const float attackingProgress =
        homeTeam ? attackingPosition : 1.0f - attackingPosition;
    const float attackingCommitment =
        sliders.offensiveBias + sliders.riskTaking;
    const TeamPhase phase = homeTeam ? homePhase : awayPhase;
    const std::size_t runnerCount =
        phase == TeamPhase::ATTACKING_TRANSITION ||
                attackingProgress >=
                    MatchTuning::Shape::SECOND_RUNNER_PROGRESS_THRESHOLD ||
                attackingCommitment >=
                    MatchTuning::Shape::SECOND_RUNNER_ATTACK_THRESHOLD
            ? MatchTuning::Shape::MAX_COMMITTED_RUNNERS
            : MatchTuning::Shape::MIN_COMMITTED_RUNNERS;

    for (auto& candidate : players)
    {
      if (&candidate == carrier || !candidate.player ||
          candidate.isHomeTeam != homeTeam)
      {
        continue;
      }
      const float score = runPriority(candidate);
      for (std::size_t slot = 0; slot < runnerCount; ++slot)
      {
        if (score <= scores[slot]) continue;
        for (std::size_t shifted = runners.size() - 1; shifted > slot;
             --shifted)
        {
          runners[shifted] = runners[shifted - 1];
          scores[shifted] = scores[shifted - 1];
        }
        runners[slot] = &candidate;
        scores[slot] = score;
        break;
      }
    }
    return runners;
  };

  const auto homeRunners = selectAttackingRunners(true);
  const auto awayRunners = selectAttackingRunners(false);

  const auto runChannel = [](const MatchPlayer& runner)
  {
    const PlayerRole role = runner.player->getRole();
    if (role == PlayerRole::LW)
      return MatchTuning::Shape::LEFT_INSIDE_FORWARD_CHANNEL;
    if (role == PlayerRole::RW)
      return MatchTuning::Shape::RIGHT_INSIDE_FORWARD_CHANNEL;
    if (role == PlayerRole::LM)
      return MatchTuning::Shape::LEFT_WIDE_ATTACK_CHANNEL;
    if (role == PlayerRole::RM)
      return MatchTuning::Shape::RIGHT_WIDE_ATTACK_CHANNEL;
    if (runner.basePosition.y < MatchTuning::Pitch::CENTRE)
      return MatchTuning::Shape::LEFT_STRIKER_ATTACK_CHANNEL;
    if (runner.basePosition.y > MatchTuning::Pitch::CENTRE)
      return MatchTuning::Shape::RIGHT_STRIKER_ATTACK_CHANNEL;
    return MatchTuning::Shape::CENTRAL_ATTACK_CHANNEL;
  };

  const auto selectFinalThirdPlayer = [&](bool homeTeam,
                                          bool selectFullback) -> MatchPlayer*
  {
    if (!carrier || carrier->isHomeTeam != homeTeam) return nullptr;
    const TeamPhase phase = homeTeam ? homePhase : awayPhase;
    if (phase != TeamPhase::FINAL_THIRD) return nullptr;

    MatchPlayer* selected = nullptr;
    float bestScore = std::numeric_limits<float>::max();
    for (auto& candidate : players)
    {
      if (!candidate.player || candidate.isHomeTeam != homeTeam ||
          &candidate == carrier)
      {
        continue;
      }
      const auto& runners = homeTeam ? homeRunners : awayRunners;
      if (std::ranges::find(runners, &candidate) != runners.end()) continue;
      const PlayerRole role = candidate.player->getRole();
      const bool eligible =
          selectFullback ? role == PlayerRole::LB || role == PlayerRole::RB
                         : role == PlayerRole::CM || role == PlayerRole::LM ||
                               role == PlayerRole::RM;
      if (!eligible) continue;

      float score = std::abs(candidate.basePosition.y - carrier->position.y);
      const PlayerIntent continuityIntent =
          selectFullback ? PlayerIntent::OVERLAP : PlayerIntent::ATTACK_BOX;
      if (candidate.intent == continuityIntent)
        score -= MatchTuning::Shape::FINAL_THIRD_SELECTION_CONTINUITY;
      if (score < bestScore)
      {
        bestScore = score;
        selected = &candidate;
      }
    }
    return selected;
  };

  MatchPlayer* homeMidfieldArrival = selectFinalThirdPlayer(true, false);
  MatchPlayer* awayMidfieldArrival = selectFinalThirdPlayer(false, false);
  MatchPlayer* homeOverlappingFullback = selectFinalThirdPlayer(true, true);
  MatchPlayer* awayOverlappingFullback = selectFinalThirdPlayer(false, true);

  const auto selectActiveSupporters = [&](bool homeTeam)
  {
    std::array<MatchPlayer*, MatchTuning::Shape::MAX_ACTIVE_SUPPORTERS>
        supporters{};
    std::array<float, MatchTuning::Shape::MAX_ACTIVE_SUPPORTERS> scores;
    scores.fill(std::numeric_limits<float>::max());
    if (!carrier || carrier->isHomeTeam != homeTeam) return supporters;

    const auto& runners = homeTeam ? homeRunners : awayRunners;
    const MatchPlayer* midfieldArrival =
        homeTeam ? homeMidfieldArrival : awayMidfieldArrival;
    const MatchPlayer* overlappingFullback =
        homeTeam ? homeOverlappingFullback : awayOverlappingFullback;
    for (auto& candidate : players)
    {
      if (!candidate.player || candidate.isHomeTeam != homeTeam ||
          &candidate == carrier ||
          candidate.player->getRole() == PlayerRole::GK ||
          &candidate == midfieldArrival || &candidate == overlappingFullback ||
          std::ranges::find(runners, &candidate) != runners.end())
      {
        continue;
      }

      const PlayerRole role = candidate.player->getRole();
      const bool eligibleSupportRole =
          role == PlayerRole::CDM || role == PlayerRole::CM ||
          role == PlayerRole::CAM || role == PlayerRole::LM ||
          role == PlayerRole::RM || role == PlayerRole::LW ||
          role == PlayerRole::RW || role == PlayerRole::ST;
      if (!eligibleSupportRole) continue;

      const float score =
          distance(candidate.position, carrier->position) -
          (candidate.intent == PlayerIntent::OFFER_SUPPORT
               ? MatchTuning::Shape::SUPPORT_SELECTION_CONTINUITY_BONUS
               : 0.0f);
      for (std::size_t slot = 0; slot < supporters.size(); ++slot)
      {
        if (score >= scores[slot]) continue;
        for (std::size_t shifted = supporters.size() - 1; shifted > slot;
             --shifted)
        {
          supporters[shifted] = supporters[shifted - 1];
          scores[shifted] = scores[shifted - 1];
        }
        supporters[slot] = &candidate;
        scores[slot] = score;
        break;
      }
    }
    return supporters;
  };

  const auto homeSupporters = selectActiveSupporters(true);
  const auto awaySupporters = selectActiveSupporters(false);

  const auto findCoverOutlet = [&](bool defendingHome) -> const MatchPlayer*
  {
    if (!carrier || carrier->isHomeTeam == defendingHome) return nullptr;
    const float possessionDirection = carrier->isHomeTeam ? 1.0f : -1.0f;
    const MatchPlayer* outlet = nullptr;
    float bestScore = std::numeric_limits<float>::max();
    for (const auto& candidate : players)
    {
      if (!candidate.player || &candidate == carrier ||
          candidate.isHomeTeam != carrier->isHomeTeam ||
          candidate.player->getRole() == PlayerRole::GK)
      {
        continue;
      }
      const float outletDistance =
          distance(candidate.position, carrier->position);
      if (outletDistance > MatchTuning::Shape::COVER_OUTLET_MAX_DISTANCE)
        continue;
      const bool forwardOption =
          (candidate.position.x - carrier->position.x) * possessionDirection >
          0.0f;
      const float score =
          outletDistance - (forwardOption
                                ? MatchTuning::Shape::COVER_FORWARD_OPTION_BONUS
                                : 0.0f);
      if (score < bestScore)
      {
        bestScore = score;
        outlet = &candidate;
      }
    }
    return outlet;
  };

  const MatchPlayer* homeCoverOutlet = findCoverOutlet(true);
  const MatchPlayer* awayCoverOutlet = findCoverOutlet(false);

  for (auto& player : players)
  {
    if (!player.player) continue;
    const bool goalkeeper = player.player->getRole() == PlayerRole::GK;
    const StrategySliders& sliders =
        player.isHomeTeam ? homeSliders : awaySliders;
    const TeamPhase teamPhase = player.isHomeTeam ? homePhase : awayPhase;
    const float attackDirection = player.isHomeTeam ? 1.0f : -1.0f;
    const bool ownPossession =
        carrier && carrier->isHomeTeam == player.isHomeTeam;
    Vector2F target = player.basePosition;
    player.intent = PlayerIntent::HOLD_SHAPE;
    player.isPressing = false;
    player.isMakingRun = false;

    // The whole block follows the ball while preserving its formation. This
    // produces recognizable defensive, middle and attacking lines rather than
    // twenty outfield players independently chasing one point.
    const float widthScale =
        MatchTuning::Shape::MIN_WIDTH_SCALE +
        sliders.widthUsage * MatchTuning::Shape::WIDTH_SLIDER_SCALE;
    target.y = MatchTuning::Pitch::CENTRE +
               (target.y - MatchTuning::Pitch::CENTRE) * widthScale;
    target.x += (pressurePosition.x - MatchTuning::Pitch::CENTRE) *
                (MatchTuning::Shape::BASE_LONGITUDINAL_SHIFT +
                 sliders.compactness *
                     MatchTuning::Shape::COMPACTNESS_LONGITUDINAL_SHIFT);
    target.y +=
        (pressurePosition.y - MatchTuning::Pitch::CENTRE) *
        (MatchTuning::Shape::BASE_LATERAL_SHIFT +
         sliders.compactness * MatchTuning::Shape::COMPACTNESS_LATERAL_SHIFT);

    if (&player == carrier)
    {
      player.intent = PlayerIntent::CARRY_BALL;
      target = player.position;
      target.x +=
          attackDirection *
          (MatchTuning::Shape::CARRIER_BASE_ADVANCE +
           player.dribbling * MatchTuning::Shape::CARRIER_DRIBBLING_ADVANCE +
           sliders.riskTaking * MatchTuning::Shape::CARRIER_RISK_ADVANCE);
      if (teamPhase == TeamPhase::ATTACKING_TRANSITION)
      {
        target.x += attackDirection *
                    MatchTuning::Shape::ATTACKING_TRANSITION_CARRIER_ADVANCE;
      }

      // Carry away from the nearest defender instead of running directly
      // through them, while gradually looking for a central shooting lane.
      const MatchPlayer* closestOpponent = nullptr;
      float closestDistance = std::numeric_limits<float>::max();
      for (const auto& opponent : players)
      {
        if (opponent.isHomeTeam == player.isHomeTeam) continue;
        const float opponentDistance =
            distance(opponent.position, player.position);
        if (opponentDistance < closestDistance)
        {
          closestDistance = opponentDistance;
          closestOpponent = &opponent;
        }
      }
      if (closestOpponent &&
          closestDistance < MatchTuning::Shape::CARRIER_EVASION_RANGE)
      {
        const float evadeDirection =
            player.position.y <= closestOpponent->position.y ? -1.0f : 1.0f;
        target.y +=
            evadeDirection *
            (MatchTuning::Shape::CARRIER_BASE_EVASION +
             player.dribbling * MatchTuning::Shape::CARRIER_DRIBBLING_EVASION);
      }
      else
      {
        target.y += (MatchTuning::Pitch::CENTRE - player.position.y) *
                    MatchTuning::Shape::CARRIER_CENTRALITY;
      }
    }
    else if (ownPossession)
    {
      player.intent = PlayerIntent::HOLD_SHAPE;
      const float attackingProgress =
          player.isHomeTeam ? carrier->position.x : 1.0f - carrier->position.x;
      target.x +=
          attackDirection *
          std::max(0.0f, attackingProgress -
                             MatchTuning::Shape::POSSESSION_PROGRESS_START) *
          MatchTuning::Shape::POSSESSION_BLOCK_PROGRESS;
      target.x +=
          attackDirection * (MatchTuning::Shape::SUPPORT_BASE_ADVANCE +
                             sliders.offensiveBias *
                                 MatchTuning::Shape::SUPPORT_OFFENSIVE_ADVANCE);
      const PlayerRole role = player.player->getRole();
      const bool forward = role == PlayerRole::ST || role == PlayerRole::LW ||
                           role == PlayerRole::RW || role == PlayerRole::CAM;
      const auto& runners = player.isHomeTeam ? homeRunners : awayRunners;
      const auto runnerPosition = std::ranges::find(runners, &player);
      const bool committedRunner = runnerPosition != runners.end();
      if (committedRunner)
      {
        const std::size_t runnerSlot = static_cast<std::size_t>(
            std::distance(runners.begin(), runnerPosition));
        player.intent = runnerSlot == 0 ? PlayerIntent::RUN_IN_BEHIND
                                        : PlayerIntent::ATTACK_BOX;
        player.isMakingRun = true;
        const float defenderLine = offsideLine(player.isHomeTeam);
        const float legalRunLine =
            player.isHomeTeam ? std::max(defenderLine, ball.position.x)
                              : std::min(defenderLine, ball.position.x);
        target.x += attackDirection *
                    (MatchTuning::Shape::RUN_BASE_ADVANCE +
                     sliders.riskTaking * MatchTuning::Shape::RUN_RISK_ADVANCE);
        const float onsideTarget =
            legalRunLine -
            attackDirection * MatchTuning::Shape::RUN_ONSIDE_BUFFER;
        const float runDepthTarget =
            onsideTarget - attackDirection * static_cast<float>(runnerSlot) *
                               MatchTuning::Shape::SECONDARY_RUN_DEPTH_STAGGER;
        target.x += (runDepthTarget - target.x) *
                    MatchTuning::Shape::RUN_DEPTH_TARGET_PULL;
        target.x = player.isHomeTeam ? std::min(target.x, onsideTarget)
                                     : std::max(target.x, onsideTarget);

        float channel = runChannel(player);
        if (runnerSlot > 0 && runners[0])
        {
          const float primaryChannel = runChannel(*runners[0]);
          if (std::abs(channel - primaryChannel) <
              MatchTuning::Shape::MINIMUM_RUN_CHANNEL_SEPARATION)
          {
            channel = primaryChannel <= MatchTuning::Pitch::CENTRE
                          ? MatchTuning::Shape::RIGHT_INSIDE_FORWARD_CHANNEL
                          : MatchTuning::Shape::LEFT_INSIDE_FORWARD_CHANNEL;
          }
        }
        target.y +=
            (channel - target.y) * MatchTuning::Shape::RUN_CHANNEL_BLEND;

        if (isOffside(player, player.isHomeTeam))
          target.x -= attackDirection * MatchTuning::Shape::ONSIDE_RECOVERY;
      }
      else
      {
        MatchPlayer* midfieldArrival =
            player.isHomeTeam ? homeMidfieldArrival : awayMidfieldArrival;
        MatchPlayer* overlappingFullback = player.isHomeTeam
                                               ? homeOverlappingFullback
                                               : awayOverlappingFullback;
        const auto& supporters =
            player.isHomeTeam ? homeSupporters : awaySupporters;
        const auto supportPosition = std::ranges::find(supporters, &player);
        if (&player == midfieldArrival)
        {
          player.intent = PlayerIntent::ATTACK_BOX;
          target.x += attackDirection *
                      MatchTuning::Shape::FINAL_THIRD_MIDFIELD_ARRIVAL;
        }
        else if (&player == overlappingFullback)
        {
          player.intent = PlayerIntent::OVERLAP;
          target.x += attackDirection *
                      MatchTuning::Shape::FINAL_THIRD_FULLBACK_OVERLAP;
        }
        else if (supportPosition != supporters.end())
        {
          player.intent = PlayerIntent::OFFER_SUPPORT;
          const std::size_t supportSlot = static_cast<std::size_t>(
              std::distance(supporters.begin(), supportPosition));
          const float primarySide =
              supporters[MatchTuning::Shape::NEAR_SUPPORT_SLOT] &&
                      supporters[MatchTuning::Shape::NEAR_SUPPORT_SLOT]
                              ->basePosition.y <= carrier->position.y
                  ? -1.0f
                  : 1.0f;
          float supportDepth = MatchTuning::Shape::NEAR_SUPPORT_DEPTH;
          float supportWidth = MatchTuning::Shape::NEAR_SUPPORT_WIDTH;
          float supportSide = primarySide;
          if (supportSlot == MatchTuning::Shape::SQUARE_SUPPORT_SLOT)
          {
            supportDepth = MatchTuning::Shape::SQUARE_SUPPORT_DEPTH;
            supportWidth = MatchTuning::Shape::SQUARE_SUPPORT_WIDTH;
            supportSide = -primarySide;
          }
          else if (supportSlot == MatchTuning::Shape::TRAILING_SUPPORT_SLOT)
          {
            supportDepth = MatchTuning::Shape::TRAILING_SUPPORT_DEPTH;
            supportWidth = MatchTuning::Shape::TRAILING_SUPPORT_WIDTH;
          }

          target = carrier->position;
          target.x -= attackDirection * supportDepth;
          target.y += supportSide * supportWidth;
          if (teamPhase == TeamPhase::ATTACKING_TRANSITION)
          {
            target.x += attackDirection *
                        MatchTuning::Shape::TRANSITION_SUPPORT_FORWARD_BONUS;
          }
        }
        else if (forward)
        {
          player.intent = PlayerIntent::OFFER_SUPPORT;
          // Not every attacker runs beyond the defence. A complementary
          // forward checks toward the ball to form a passing triangle and drag
          // a marker.
          target.x =
              carrier->position.x -
              attackDirection * MatchTuning::Shape::FORWARD_SHORT_OPTION_DEPTH;
          const float lateralDirection =
              player.basePosition.y <= carrier->position.y ? -1.0f : 1.0f;
          target.y =
              carrier->position.y +
              lateralDirection *
                  MatchTuning::Shape::FORWARD_SHORT_OPTION_LATERAL_SEPARATION;
        }
        if (distance(target, carrier->position) >
            MatchTuning::Shape::MAX_SUPPORT_DISTANCE)
        {
          target.x += (carrier->position.x - target.x) *
                      MatchTuning::Shape::SUPPORT_LONGITUDINAL_PULL;
          target.y += (carrier->position.y - target.y) *
                      MatchTuning::Shape::SUPPORT_LATERAL_PULL;
        }
      }
    }
    else if (carrier)
    {
      const auto& pressers = player.isHomeTeam ? homePressers : awayPressers;
      if (pressers[0] == &player)
      {
        player.intent = PlayerIntent::PRESS_BALL;
        player.isPressing = true;
        const float standOff =
            MatchTuning::Shape::PRESSING_STANDOFF_BASE +
            (1.0f - sliders.pressing) *
                MatchTuning::Shape::PRESSING_STANDOFF_CAUTIOUS_BONUS;
        target = pressurePosition;
        target.x -= attackDirection * standOff;
      }
      else if (pressers[1] == &player &&
               sliders.pressing > MatchTuning::Shape::COVER_PRESS_MINIMUM)
      {
        player.isPressing = true;
        const MatchPlayer* coverOutlet =
            player.isHomeTeam ? homeCoverOutlet : awayCoverOutlet;
        if (coverOutlet)
        {
          player.intent = PlayerIntent::BLOCK_PASSING_LANE;
          target.x = pressurePosition.x +
                     (coverOutlet->position.x - pressurePosition.x) *
                         MatchTuning::Shape::COVER_LANE_INTERCEPTION_POINT;
          target.y = pressurePosition.y +
                     (coverOutlet->position.y - pressurePosition.y) *
                         MatchTuning::Shape::COVER_LANE_INTERCEPTION_POINT;
          target.x -=
              attackDirection * MatchTuning::Shape::COVER_LANE_GOAL_SIDE_OFFSET;
        }
        else
        {
          player.intent = PlayerIntent::COVER_PRESS;
          const float lateralSide =
              player.position.y <= pressurePosition.y ? -1.0f : 1.0f;
          target.x = pressurePosition.x -
                     attackDirection * MatchTuning::Shape::COVER_FALLBACK_DEPTH;
          target.y =
              pressurePosition.y +
              lateralSide * MatchTuning::Shape::COVER_FALLBACK_LATERAL_OFFSET;
        }
      }
      else if (teamPhase == TeamPhase::DEFENSIVE_TRANSITION)
      {
        // The nearest players counter-press above. Everyone else first gets
        // goal-side and narrows toward the danger before settling into the
        // normal defensive block.
        player.intent = PlayerIntent::RECOVER_SHAPE;
        target.x -=
            attackDirection * MatchTuning::Shape::DEFENSIVE_TRANSITION_RECOVERY;
        target.y += (pressurePosition.y - target.y) *
                    MatchTuning::Shape::DEFENSIVE_TRANSITION_BALL_COMPACTNESS;
      }
      else
      {
        // Zonal marking: shade toward the most relevant opponent in this
        // player's channel, without abandoning the formation anchor.
        const MatchPlayer* mark = nullptr;
        float bestMarkScore = std::numeric_limits<float>::max();
        for (const auto& opponent : players)
        {
          if (opponent.isHomeTeam == player.isHomeTeam || !opponent.player ||
              opponent.player->getRole() == PlayerRole::GK)
            continue;
          const float channelDistance =
              std::abs(opponent.position.y - player.basePosition.y);
          const float depthDistance =
              std::abs(opponent.position.x - player.basePosition.x);
          const float dangerDepth = player.isHomeTeam
                                        ? opponent.position.x
                                        : 1.0f - opponent.position.x;
          const float markScore =
              channelDistance * MatchTuning::Shape::CHANNEL_WEIGHT +
              depthDistance -
              dangerDepth * MatchTuning::Shape::DANGER_DEPTH_WEIGHT;
          if (markScore < bestMarkScore)
          {
            bestMarkScore = markScore;
            mark = &opponent;
          }
        }
        if (mark)
        {
          player.intent = PlayerIntent::MARK_OPPONENT;
          const float markWeight =
              MatchTuning::Shape::BASE_MARK_WEIGHT +
              sliders.compactness * MatchTuning::Shape::COMPACTNESS_MARK_WEIGHT;
          target.x += (mark->position.x - target.x) * markWeight;
          target.y += (mark->position.y - target.y) * markWeight;
        }
      }
    }
    else
    {
      const bool receivingPass =
          ball.isPass && ball.passByHome == player.isHomeTeam;
      const auto& pressers = player.isHomeTeam ? homePressers : awayPressers;
      if (receivingPass)
      {
        if (player.player == ball.intendedReceiver)
        {
          player.intent = PlayerIntent::RECEIVE_PASS;
          target = {
              std::clamp(
                  ball.position.x +
                      ball.velocity.x *
                          MatchTuning::Player::PASS_RECEIVER_LOOKAHEAD_SECONDS,
                  MatchTuning::Pitch::PLAYER_MIN_X,
                  MatchTuning::Pitch::PLAYER_MAX_X),
              std::clamp(
                  ball.position.y +
                      ball.velocity.y *
                          MatchTuning::Player::PASS_RECEIVER_LOOKAHEAD_SECONDS,
                  MatchTuning::Pitch::PLAYER_MIN_Y,
                  MatchTuning::Pitch::PLAYER_MAX_Y)};
        }
        else
        {
          player.intent = PlayerIntent::OFFER_SUPPORT;
          // Preserve the lane established before release instead of snapping
          // every supporting player back toward the formation anchor for the
          // duration of each pass.
          target = player.movementTarget;
          target.x +=
              attackDirection * MatchTuning::Shape::IN_FLIGHT_SUPPORT_ADVANCE;
          target.y += (ball.position.y - target.y) *
                      MatchTuning::Shape::IN_FLIGHT_SUPPORT_BALL_PULL;
        }
      }
      // For a genuinely loose ball, only the nearest players contest it.
      // Everybody else either joins a selected transition run or recovers the
      // team shape. A normal pass is handled above so it does not trigger a
      // full-team scramble on every release of the ball.
      else if (pressers[0] == &player ||
               (pressers[1] == &player &&
                sliders.pressing >
                    MatchTuning::Shape::SECOND_LOOSE_BALL_PRESS_THRESHOLD))
      {
        player.intent = pressers[0] == &player ? PlayerIntent::CLAIM_LOOSE_BALL
                                               : PlayerIntent::COVER_PRESS;
        player.isPressing = true;
        target = {std::clamp(
                      pressurePosition.x +
                          ball.velocity.x *
                              MatchTuning::Player::LOOSE_BALL_LOOKAHEAD_SECONDS,
                      MatchTuning::Pitch::PLAYER_MIN_X,
                      MatchTuning::Pitch::PLAYER_MAX_X),
                  std::clamp(
                      pressurePosition.y +
                          ball.velocity.y *
                              MatchTuning::Player::LOOSE_BALL_LOOKAHEAD_SECONDS,
                      MatchTuning::Pitch::PLAYER_MIN_Y,
                      MatchTuning::Pitch::PLAYER_MAX_Y)};
        if (pressers[1] == &player)
        {
          target.x -=
              attackDirection * MatchTuning::Player::COVER_LOOSE_BALL_OFFSET;
        }
      }
      else if (ball.isPass || ball.isShot)
      {
        if (teamPhase == TeamPhase::DEFENSIVE_TRANSITION)
        {
          player.intent = PlayerIntent::RECOVER_SHAPE;
          target.x -= attackDirection *
                      MatchTuning::Shape::DEFENSIVE_TRANSITION_RECOVERY;
          target.y += (pressurePosition.y - target.y) *
                      MatchTuning::Shape::DEFENSIVE_TRANSITION_BALL_COMPACTNESS;
        }
        else
        {
          player.intent = PlayerIntent::HOLD_SHAPE;
        }
      }
      else
      {
        const bool attackingTransition =
            teamPhase == TeamPhase::ATTACKING_TRANSITION;
        if (attackingTransition)
        {
          const auto& runners = player.isHomeTeam ? homeRunners : awayRunners;
          const bool transitionRunner =
              std::ranges::find(runners, &player) != runners.end();
          player.intent = transitionRunner ? PlayerIntent::RUN_IN_BEHIND
                                           : PlayerIntent::OFFER_SUPPORT;
          player.isMakingRun = transitionRunner;
          target.x +=
              attackDirection *
              (transitionRunner
                   ? MatchTuning::Shape::ATTACKING_TRANSITION_RUN_ADVANCE
                   : MatchTuning::Shape::ATTACKING_TRANSITION_SUPPORT_ADVANCE);
          target.y += (pressurePosition.y - target.y) *
                      MatchTuning::Shape::ATTACKING_TRANSITION_BALL_PULL;
          if (transitionRunner && isOffside(player, player.isHomeTeam))
          {
            target.x -= attackDirection * MatchTuning::Shape::ONSIDE_RECOVERY;
          }
        }
        else
        {
          player.intent = PlayerIntent::RECOVER_SHAPE;
          target.x -= attackDirection *
                      MatchTuning::Shape::DEFENSIVE_TRANSITION_RECOVERY;
        }
      }
    }

    if (goalkeeper)
    {
      player.intent = PlayerIntent::GOALKEEP;
      target.x = player.isHomeTeam ? MatchTuning::Pitch::LEFT_GOALKEEPER_X
                                   : MatchTuning::Pitch::RIGHT_GOALKEEPER_X;
      target.y =
          std::clamp(pressurePosition.y, MatchTuning::Pitch::GOALKEEPER_MIN_Y,
                     MatchTuning::Pitch::GOALKEEPER_MAX_Y);
      if ((player.isHomeTeam &&
           pressurePosition.x < MatchTuning::Pitch::GOALKEEPER_SWEEP_DEPTH) ||
          (!player.isHomeTeam &&
           pressurePosition.x >
               1.0f - MatchTuning::Pitch::GOALKEEPER_SWEEP_DEPTH))
      {
        target.x += player.isHomeTeam
                        ? MatchTuning::Pitch::GOALKEEPER_SWEEP_OFFSET
                        : -MatchTuning::Pitch::GOALKEEPER_SWEEP_OFFSET;
      }
    }

    target.x = std::clamp(target.x, MatchTuning::Pitch::PLAYER_MIN_X,
                          MatchTuning::Pitch::PLAYER_MAX_X);
    target.y = std::clamp(target.y, MatchTuning::Pitch::KICKOFF_FORMATION_INSET,
                          1.0f - MatchTuning::Pitch::KICKOFF_FORMATION_INSET);
    const bool urgentTarget = player.intent == PlayerIntent::PRESS_BALL ||
                              player.intent == PlayerIntent::CLAIM_LOOSE_BALL;
    const float targetResponse =
        urgentTarget ? MatchTuning::Player::URGENT_TARGET_RESPONSE_PER_SECOND
                     : MatchTuning::Player::TACTICAL_TARGET_RESPONSE_PER_SECOND;
    const float targetBlend = 1.0f - std::exp(-targetResponse * dt);
    player.movementTarget.x +=
        (target.x - player.movementTarget.x) * targetBlend;
    player.movementTarget.y +=
        (target.y - player.movementTarget.y) * targetBlend;
    const Vector2F targetOffset{player.movementTarget.x - player.position.x,
                                player.movementTarget.y - player.position.y};
    const float targetDistance = length(targetOffset);
    const Vector2F desiredDirection = normalized(targetOffset);
    const float arrivalSpeedScale = std::clamp(
        targetDistance / MatchTuning::Player::ARRIVAL_SLOWING_DISTANCE, 0.0f,
        1.0f);
    const float staminaSpeed =
        MatchTuning::Player::STAMINA_SPEED_BASE +
        player.stamina * MatchTuning::Player::STAMINA_SPEED_BONUS;
    const float intentSpeed = movementSpeedScale(player.intent);
    const Vector2F desiredVelocity{
        desiredDirection.x * player.maxSpeed * staminaSpeed * intentSpeed *
            arrivalSpeedScale,
        desiredDirection.y * player.maxSpeed * staminaSpeed * intentSpeed *
            arrivalSpeedScale};

    const float accelerationFactor =
        std::clamp(player.acceleration * dt, 0.0f, 1.0f);
    player.velocity.x +=
        (desiredVelocity.x - player.velocity.x) * accelerationFactor;
    player.velocity.y +=
        (desiredVelocity.y - player.velocity.y) * accelerationFactor;
    player.position.x = std::clamp(player.position.x + player.velocity.x * dt,
                                   MatchTuning::Pitch::PLAYER_MIN_X,
                                   MatchTuning::Pitch::PLAYER_MAX_X);
    player.position.y = std::clamp(player.position.y + player.velocity.y * dt,
                                   MatchTuning::Pitch::PLAYER_MIN_Y,
                                   MatchTuning::Pitch::PLAYER_MAX_Y);

    if (length(player.velocity) >
        MatchTuning::Player::MOVEMENT_FACING_THRESHOLD)
    {
      player.targetAngle = std::atan2(player.velocity.y, player.velocity.x);
      float angleDifference = player.targetAngle - player.facingAngle;
      while (angleDifference > std::numbers::pi_v<float>)
        angleDifference -= 2.0f * std::numbers::pi_v<float>;
      while (angleDifference < -std::numbers::pi_v<float>)
        angleDifference += 2.0f * std::numbers::pi_v<float>;
      player.facingAngle += std::clamp(angleDifference, -player.turnRate * dt,
                                       player.turnRate * dt);
    }

    const float movementLoad =
        length(player.velocity) / std::max(player.maxSpeed, EPSILON);
    const float drain =
        dt *
        (MatchTuning::Player::IDLE_STAMINA_DRAIN +
         movementLoad * MatchTuning::Player::MOVEMENT_STAMINA_DRAIN +
         (player.isPressing
              ? sliders.pressing * MatchTuning::Player::PRESSING_STAMINA_DRAIN
              : 0.0f));
    player.stamina = std::clamp(player.stamina - drain,
                                MatchTuning::Player::MINIMUM_STAMINA, 1.0f);
  }

  // Resolve simple body spacing after tactical movement. This prevents visual
  // stacking and creates natural passing lanes while keeping the simulation
  // deterministic.
  for (size_t first = 0; first < players.size(); ++first)
  {
    for (size_t second = first + 1; second < players.size(); ++second)
    {
      Vector2F separationMetres{
          (players[second].position.x - players[first].position.x) *
              MatchTuning::Pitch::LENGTH_METRES,
          (players[second].position.y - players[first].position.y) *
              MatchTuning::Pitch::WIDTH_METRES};
      float separationLengthMetres = length(separationMetres);
      if (separationLengthMetres >=
          MatchTuning::Player::MINIMUM_BODY_SEPARATION_METRES)
      {
        continue;
      }
      if (separationLengthMetres <= EPSILON)
      {
        separationMetres = {0.0f, first % 2 == 0 ? 1.0f : -1.0f};
        separationLengthMetres = 1.0f;
      }
      const Vector2F directionMetres{
          separationMetres.x / separationLengthMetres,
          separationMetres.y / separationLengthMetres};
      const float correctionMetres =
          (MatchTuning::Player::MINIMUM_BODY_SEPARATION_METRES -
           separationLengthMetres) *
          MatchTuning::Player::BODY_SEPARATION_SHARE;
      const Vector2F correction{directionMetres.x * correctionMetres /
                                    MatchTuning::Pitch::LENGTH_METRES,
                                directionMetres.y * correctionMetres /
                                    MatchTuning::Pitch::WIDTH_METRES};
      players[first].position.x = std::clamp(
          players[first].position.x - correction.x,
          MatchTuning::Pitch::PLAYER_MIN_X, MatchTuning::Pitch::PLAYER_MAX_X);
      players[first].position.y = std::clamp(
          players[first].position.y - correction.y,
          MatchTuning::Pitch::PLAYER_MIN_Y, MatchTuning::Pitch::PLAYER_MAX_Y);
      players[second].position.x = std::clamp(
          players[second].position.x + correction.x,
          MatchTuning::Pitch::PLAYER_MIN_X, MatchTuning::Pitch::PLAYER_MAX_X);
      players[second].position.y = std::clamp(
          players[second].position.y + correction.y,
          MatchTuning::Pitch::PLAYER_MIN_Y, MatchTuning::Pitch::PLAYER_MAX_Y);
      players[first].velocity.x *=
          MatchTuning::Player::BODY_COLLISION_VELOCITY_RETAINED;
      players[first].velocity.y *=
          MatchTuning::Player::BODY_COLLISION_VELOCITY_RETAINED;
      players[second].velocity.x *=
          MatchTuning::Player::BODY_COLLISION_VELOCITY_RETAINED;
      players[second].velocity.y *=
          MatchTuning::Player::BODY_COLLISION_VELOCITY_RETAINED;
    }
  }
}

void MatchEngine::resolvePossessionAndActions(float /*dt*/)
{
  MatchPlayer* carrier = findMatchPlayer(ball.possessedBy);
  if (!carrier)
  {
    resolveLooseBall();
    return;
  }

  ball.position = carrier->position;
  ball.z = 0.0f;
  ball.lastPossessor = carrier->player;
  if (carrier->isHomeTeam)
    homePossessionMinutes += MatchTuning::Timing::FIXED_STEP_SECONDS;
  else
    awayPossessionMinutes += MatchTuning::Timing::FIXED_STEP_SECONDS;

  const float possessedMinutes = homePossessionMinutes + awayPossessionMinutes;
  if (possessedMinutes > EPSILON)
  {
    stats.homePossession = homePossessionMinutes / possessedMinutes *
                           MatchTuning::Statistics::PERCENT_SCALE;
    stats.awayPossession =
        MatchTuning::Statistics::PERCENT_SCALE - stats.homePossession;
  }

  MatchPlayer* defender =
      findClosestPlayer(carrier->position, !carrier->isHomeTeam, false);
  if (defender &&
      distance(defender->position, carrier->position) <
          MatchTuning::Defending::TACKLE_DISTANCE &&
      defender->tackleCooldown <= 0.0f)
  {
    attemptTackle(*carrier, *defender);
    if (ball.possessedBy != carrier->player || state != MatchState::PLAYING)
    {
      return;
    }
  }

  if (carrier->actionCooldown <= 0.0f) decideAction(*carrier);
}

void MatchEngine::attemptTackle(MatchPlayer& carrier, MatchPlayer& defender)
{
  const StrategySliders defenderStrategy =
      (defender.isHomeTeam ? homeStrategy : awayStrategy).getSliders();
  defender.tackleCooldown =
      randomFloat(MatchTuning::Defending::MIN_TACKLE_COOLDOWN,
                  MatchTuning::Defending::MAX_TACKLE_COOLDOWN) *
      (MatchTuning::Defending::TACKLE_COOLDOWN_BASE_MULTIPLIER -
       defenderStrategy.pressing *
           MatchTuning::Defending::PRESSING_COOLDOWN_REDUCTION);

  const float winChance = std::clamp(
      MatchTuning::Defending::BASE_WIN_CHANCE +
          defender.defending * MatchTuning::Defending::DEFENDING_WIN_BONUS -
          carrier.dribbling * MatchTuning::Defending::DRIBBLING_WIN_PENALTY +
          defenderStrategy.pressing *
              MatchTuning::Defending::PRESSING_WIN_BONUS,
      MatchTuning::Defending::MIN_WIN_CHANCE,
      MatchTuning::Defending::MAX_WIN_CHANCE);
  if (randomFloat(0.0f, 1.0f) < winChance)
  {
    if (defender.isHomeTeam)
      ++stats.homeTackles;
    else
      ++stats.awayTackles;
    setPossession(defender);
    defender.actionCooldown =
        randomFloat(MatchTuning::Defending::MIN_RECOVERY_COOLDOWN,
                    MatchTuning::Defending::MAX_RECOVERY_COOLDOWN);
    return;
  }

  const float foulChance =
      MatchTuning::Defending::BASE_FOUL_CHANCE +
      defenderStrategy.riskTaking * MatchTuning::Defending::RISK_FOUL_BONUS;
  if (randomFloat(0.0f, 1.0f) >= foulChance) return;

  if (defender.isHomeTeam)
    ++stats.homeFouls;
  else
    ++stats.awayFouls;

  const bool yellowCard =
      randomFloat(0.0f, 1.0f) < MatchTuning::Defending::YELLOW_CARD_CHANCE;
  if (yellowCard)
  {
    if (defender.isHomeTeam)
      ++stats.homeYellowCards;
    else
      ++stats.awayYellowCards;
  }

  logEvent(defender.player->getName() +
           (yellowCard ? " commits a foul and is booked" : " commits a foul"));
  const bool penalty =
      (carrier.isHomeTeam &&
       carrier.position.x > MatchTuning::Pitch::RIGHT_PENALTY_AREA_EDGE) ||
      (!carrier.isHomeTeam &&
       carrier.position.x < MatchTuning::Pitch::LEFT_PENALTY_AREA_EDGE);
  if (penalty)
    setupPenalty(carrier.isHomeTeam);
  else
    setupFreeKick(carrier.isHomeTeam, carrier.position);
}

void MatchEngine::decideAction(MatchPlayer& carrier)
{
  const StrategySliders strategy =
      (carrier.isHomeTeam ? homeStrategy : awayStrategy).getSliders();
  const float pressure = std::clamp((MatchTuning::Decision::PRESSURE_RADIUS -
                                     nearestOpponentDistance(carrier)) /
                                        MatchTuning::Decision::PRESSURE_RADIUS,
                                    0.0f, 1.0f);
  const float shotXG = estimateShotXG(carrier);
  const float opennessAhead = openSpaceAhead(carrier);

  // Evaluate the passing candidates first: the task is pure (no random draws)
  // and the deterministic best plus runner-up options are visible in the
  // decision snapshot even when a shot or dribble is eventually chosen.
  const std::optional<PassOption> option = choosePassTarget(carrier);

  // Every candidate is scored in a shared utility currency.
  const float visionNoiseScale =
      (1.0f - carrier.passing) * MatchTuning::Decision::VISION_NOISE_SCALE;

  float passScore = -std::numeric_limits<float>::infinity();
  if (option)
  {
    passScore = option->utility;
    const float carrierDepth =
        carrier.isHomeTeam ? carrier.position.x : 1.0f - carrier.position.x;
    const bool pinnedToByline =
        carrierDepth >= MatchTuning::Rules::HOME_FINAL_THIRD_START &&
        std::abs(MatchTuning::Pitch::CENTRE - carrier.position.y) >=
            MatchTuning::Decision::WIDE_SHOT_WIDTH_DEVIATION;
    if (pinnedToByline && option->targetPoint.x <= carrier.position.x)
    {
      passScore += MatchTuning::Decision::WIDE_RECYCLE_BONUS;
    }
  }

  float shotScore = -std::numeric_limits<float>::infinity();
  if (shotXG >= MatchTuning::Decision::MIN_SHOT_XG)
  {
    // The "have-a-go" inclination only matters from a credible shooting
    // range: it fades out completely for absurd-distance attempts, so the
    // scored decision can never invent a nonsensical long shot.
    const float rangeEligibility =
        std::clamp((shotXG - MatchTuning::Decision::SHOT_ELIGIBILITY_FLOOR) /
                       MatchTuning::Decision::SHOT_ELIGIBILITY_RANGE,
                   0.0f, 1.0f);
    const float carrierDepth =
        carrier.isHomeTeam ? carrier.position.x : 1.0f - carrier.position.x;
    const float finalThirdBonus =
        carrierDepth >= MatchTuning::Rules::HOME_FINAL_THIRD_START
            ? MatchTuning::Decision::FINAL_THIRD_SHOT_BONUS
            : 0.0f;
    shotScore = (shotXG - MatchTuning::Decision::BASE_SHOT_THRESHOLD) *
                    MatchTuning::Decision::SHOT_SCORE_SCALE +
                MatchTuning::Decision::SHOT_BASE_INCLINATION *
                    (0.6f + opennessAhead) * rangeEligibility +
                finalThirdBonus +
                carrier.shooting * MatchTuning::Decision::SHOT_SKILL_BONUS -
                pressure * MatchTuning::Decision::SHOT_PRESSURE_PENALTY;
    if (carrierDepth >= MatchTuning::Rules::HOME_FINAL_THIRD_START &&
        std::abs(MatchTuning::Pitch::CENTRE - carrier.position.y) >=
            MatchTuning::Decision::WIDE_SHOT_WIDTH_DEVIATION)
    {
      shotScore *= MatchTuning::Decision::WIDE_SHOT_DISCOUNT;
    }
  }

  float carryScore =
      opennessAhead * MatchTuning::Decision::CARRY_OPENNESS_WEIGHT +
      carrier.dribbling * MatchTuning::Decision::CARRY_DRIBBLING_BONUS -
      pressure * MatchTuning::Decision::CARRY_PRESSURE_PENALTY +
      strategy.riskTaking * MatchTuning::Decision::CARRY_RISK_BIAS;

  float shieldScore = -std::numeric_limits<float>::infinity();
  const bool passUnavailableOrWeak =
      !option || passScore <= MatchTuning::Passing::MIN_ACCEPTABLE_OPTION_SCORE;
  if (pressure >= MatchTuning::Decision::SHIELD_PRESSURE_THRESHOLD &&
      passUnavailableOrWeak)
  {
    shieldScore = pressure * MatchTuning::Decision::SHIELD_BONUS +
                  carrier.dribbling * MatchTuning::Decision::SHIELD_DRIBBLING;
  }

  // A side protecting a late lead no longer forces speculative long-range
  // attempts; it prefers to retain the ball. Credible chances are unaffected.
  const int carrierScore = carrier.isHomeTeam ? homeScore : awayScore;
  const int opponentScore = carrier.isHomeTeam ? awayScore : homeScore;
  if (matchTimeMinutes >= MatchTuning::Decision::LATE_GAME_MINUTE &&
      carrierScore > opponentScore &&
      shotScore > -std::numeric_limits<float>::infinity() &&
      shotXG < MatchTuning::Decision::BASE_SHOT_THRESHOLD)
  {
    shotScore -= MatchTuning::Decision::LATE_LEAD_SPECULATIVE_PENALTY;
  }

  // Vision scales how much randomness perturbs close choices. The noise is
  // bounded so a truly nonsensical option can never win.
  shotScore += visionNoiseScale * randomFloat(-1.0f, 1.0f);
  carryScore += visionNoiseScale * randomFloat(-1.0f, 1.0f);
  if (shieldScore > -std::numeric_limits<float>::infinity())
  {
    shieldScore += visionNoiseScale * randomFloat(-1.0f, 1.0f);
  }
  if (option)
  {
    passScore += visionNoiseScale * randomFloat(-1.0f, 1.0f);
  }

  lastScenarioDecision.passUtility = passScore;
  lastScenarioDecision.shotUtility = shotScore;
  lastScenarioDecision.carryUtility = carryScore;
  lastScenarioDecision.shieldUtility = shieldScore;

  lastScenarioDecision.action = ScenarioAction::NONE;
  if (shotScore >= passScore && shotScore >= carryScore &&
      shotScore >= shieldScore)
  {
    lastScenarioDecision.action = ScenarioAction::SHOT;
    lastScenarioDecision.reason =
        "shot with estimated xG " + std::to_string(shotXG);
    takeShot(carrier);
    return;
  }
  if (carryScore >= passScore && carryScore >= shieldScore)
  {
    lastScenarioDecision.action = ScenarioAction::CARRY;
    lastScenarioDecision.reason =
        option ? std::string("carry:\"space ahead (value " +
                             std::to_string(opennessAhead) +
                             ") beats the best pass") +
                     std::string(" with intent ") +
                     std::string(passIntentName(option->intent))
               : std::string(
                     "carry:space ahead and no acceptable pass "
                     "candidate in range");
    carrier.actionCooldown =
        randomFloat(MatchTuning::Decision::MIN_DRIBBLE_TIME,
                    MatchTuning::Decision::MAX_DRIBBLE_TIME);
    return;
  }
  if (shieldScore >= passScore)
  {
    lastScenarioDecision.action = ScenarioAction::SHIELD;
    lastScenarioDecision.reason =
        "shield:retain and protect the ball under pressure with no safe "
        "outlet";
    carrier.actionCooldown =
        randomFloat(MatchTuning::Decision::MIN_DRIBBLE_TIME,
                    MatchTuning::Decision::MAX_DRIBBLE_TIME);
    return;
  }

  lastScenarioDecision.action = ScenarioAction::PASS;
  lastScenarioDecision.reason =
      std::string("pass:") + std::string(passIntentName(option->intent)) +
      " to player " +
      std::to_string(option->receiver && option->receiver->player
                         ? option->receiver->player->getId()
                         : 0) +
      " (utility " + std::to_string(option->utility) + ")";
  passBall(carrier, *option);
}

MatchEngine::PassOption MatchEngine::evaluatePassOption(
    MatchPlayer& passer, MatchPlayer& receiver) const
{
  const StrategySliders strategy =
      (passer.isHomeTeam ? homeStrategy : awayStrategy).getSliders();
  const float direction = passer.isHomeTeam ? 1.0f : -1.0f;
  const float passDistance = distance(passer.position, receiver.position);
  const float progression =
      (receiver.position.x - passer.position.x) * direction;
  const float openness = std::clamp(
      nearestOpponentDistance(receiver) / MatchTuning::Passing::OPENNESS_RADIUS,
      0.0f, 1.0f);
  const float laneRisk = passingLaneRisk(passer, receiver);
  const float pressure = std::clamp((MatchTuning::Decision::PRESSURE_RADIUS -
                                     nearestOpponentDistance(passer)) /
                                        MatchTuning::Decision::PRESSURE_RADIUS,
                                    0.0f, 1.0f);
  const PlayerRole role =
      receiver.player ? receiver.player->getRole() : PlayerRole::CM;
  const bool forwardRole = role == PlayerRole::ST || role == PlayerRole::LW ||
                           role == PlayerRole::RW || role == PlayerRole::CAM;
  const float passerDepth =
      passer.isHomeTeam ? passer.position.x : 1.0f - passer.position.x;
  const float receiverDepth =
      receiver.isHomeTeam ? receiver.position.x : 1.0f - receiver.position.x;
  const bool widePasser =
      passer.position.y <= MatchTuning::Passing::WIDE_ATTACK_MINIMUM_Y ||
      passer.position.y >= MatchTuning::Passing::WIDE_ATTACK_MAXIMUM_Y;
  const bool centralReceiver =
      receiver.position.y >= MatchTuning::Passing::CENTRAL_TARGET_MINIMUM_Y &&
      receiver.position.y <= MatchTuning::Passing::CENTRAL_TARGET_MAXIMUM_Y;
  const bool cutbackOption =
      widePasser && centralReceiver &&
      passerDepth >= MatchTuning::Passing::CUTBACK_MINIMUM_PASSER_DEPTH &&
      receiverDepth >= MatchTuning::Passing::CROSS_MINIMUM_RECEIVER_DEPTH &&
      progression >= MatchTuning::Passing::CUTBACK_MINIMUM_PROGRESSION &&
      progression <= MatchTuning::Passing::CUTBACK_MAXIMUM_PROGRESSION;
  const bool crossOption =
      !cutbackOption && widePasser && centralReceiver &&
      passerDepth >= MatchTuning::Passing::CROSS_MINIMUM_PASSER_DEPTH &&
      receiverDepth >= MatchTuning::Passing::CROSS_MINIMUM_RECEIVER_DEPTH;
  const float safeOutlet = pressure * std::max(0.0f, -progression) *
                           MatchTuning::Passing::SAFE_OUTLET_WEIGHT;
  const float completionProbability = std::clamp(
      MatchTuning::Passing::BASE_COMPLETION_PROBABILITY +
          passer.passing * MatchTuning::Passing::PASSING_COMPLETION_BONUS +
          openness * MatchTuning::Passing::OPENNESS_COMPLETION_BONUS -
          laneRisk * MatchTuning::Passing::LANE_COMPLETION_PENALTY -
          (passDistance / MatchTuning::Passing::MAX_DISTANCE) *
              MatchTuning::Passing::DISTANCE_COMPLETION_PENALTY -
          pressure * MatchTuning::Passing::PRESSURE_COMPLETION_PENALTY -
          (crossOption ? MatchTuning::Passing::CROSS_COMPLETION_PENALTY
                       : 0.0f) +
          (cutbackOption ? MatchTuning::Passing::CUTBACK_COMPLETION_BONUS
                         : 0.0f),
      MatchTuning::Passing::MIN_COMPLETION_PROBABILITY,
      MatchTuning::Passing::MAX_COMPLETION_PROBABILITY);
  float utility =
      openness * MatchTuning::Passing::OPENNESS_WEIGHT -
      laneRisk * MatchTuning::Passing::LANE_RISK_WEIGHT +
      progression * (MatchTuning::Passing::BASE_PROGRESS_WEIGHT +
                     strategy.offensiveBias *
                         MatchTuning::Passing::OFFENSIVE_PROGRESS_WEIGHT) -
      std::abs(passDistance - MatchTuning::Passing::IDEAL_DISTANCE) *
          MatchTuning::Passing::DISTANCE_PENALTY +
      safeOutlet +
      (forwardRole && progression > 0.0f
           ? MatchTuning::Passing::FORWARD_ROLE_BONUS
           : 0.0f) +
      completionProbability * MatchTuning::Passing::COMPLETION_UTILITY_WEIGHT;
  if (receiver.isMakingRun && progression > 0.0f)
    utility += MatchTuning::Passing::ACTIVE_RUNNER_UTILITY_BONUS;
  if (crossOption) utility += MatchTuning::Passing::CROSS_UTILITY_BONUS;
  if (cutbackOption) utility += MatchTuning::Passing::CUTBACK_UTILITY_BONUS;

  PassIntent intent = PassIntent::RECYCLE;
  if (cutbackOption)
  {
    intent = PassIntent::CUTBACK;
  }
  else if (crossOption)
  {
    intent = PassIntent::CROSS;
  }
  else if (pressure >= MatchTuning::Passing::PRESSURE_RELEASE_THRESHOLD &&
           progression <=
               MatchTuning::Passing::PRESSURE_RELEASE_MAX_PROGRESSION)
  {
    intent = PassIntent::PRESSURE_RELEASE;
  }
  else if (std::abs(receiver.position.y - passer.position.y) >=
           MatchTuning::Passing::SWITCH_PLAY_MINIMUM_WIDTH)
  {
    intent = PassIntent::SWITCH_PLAY;
  }
  else if (forwardRole && receiver.isMakingRun &&
           progression >=
               MatchTuning::Passing::THROUGH_BALL_MINIMUM_PROGRESSION)
  {
    intent = PassIntent::THROUGH_BALL;
  }
  else if (progression >= MatchTuning::Passing::PROGRESSIVE_PASS_MINIMUM)
  {
    intent = PassIntent::PROGRESSIVE;
  }

  const float flightEstimate =
      passDistance / MatchTuning::Passing::ESTIMATED_BALL_SPEED;
  Vector2F target{std::clamp(receiver.position.x +
                                 receiver.velocity.x * flightEstimate *
                                     MatchTuning::Passing::RECEIVER_LEAD_SCALE,
                             0.0f, 1.0f),
                  std::clamp(receiver.position.y +
                                 receiver.velocity.y * flightEstimate *
                                     MatchTuning::Passing::RECEIVER_LEAD_SCALE,
                             0.0f, 1.0f)};
  if (intent == PassIntent::THROUGH_BALL)
  {
    target.x += (receiver.movementTarget.x - target.x) *
                MatchTuning::Passing::THROUGH_BALL_TARGET_BLEND;
    target.y += (receiver.movementTarget.y - target.y) *
                MatchTuning::Passing::THROUGH_BALL_TARGET_BLEND;
    target.x += direction * MatchTuning::Passing::THROUGH_BALL_FORWARD_LEAD;
    target.x = std::clamp(target.x, MatchTuning::Pitch::PLAYER_MIN_X,
                          MatchTuning::Pitch::PLAYER_MAX_X);
    target.y = std::clamp(target.y, MatchTuning::Pitch::PLAYER_MIN_Y,
                          MatchTuning::Pitch::PLAYER_MAX_Y);
  }
  else if (intent == PassIntent::CROSS)
  {
    target.x += (receiver.movementTarget.x - target.x) *
                MatchTuning::Passing::CROSS_TARGET_BLEND;
    target.y += (receiver.movementTarget.y - target.y) *
                MatchTuning::Passing::CROSS_TARGET_BLEND;
  }
  const float travelDistance = distance(passer.position, target);

  return {&receiver,
          target,
          intent,
          utility,
          progression,
          laneRisk,
          completionProbability,
          travelDistance,
          intent == PassIntent::CROSS ||
              travelDistance > MatchTuning::Passing::LOFTED_DISTANCE};
}

std::optional<MatchEngine::PassOption> MatchEngine::choosePassTarget(
    MatchPlayer& passer)
{
  std::optional<PassOption> best;
  float bestScore = -std::numeric_limits<float>::infinity();
  std::optional<PassOption> runnerUp;
  float runnerUpScore = -std::numeric_limits<float>::infinity();

  auto toDecision = [](const PassOption& option)
  {
    return PassDecision{0,
                        option.receiver && option.receiver->player
                            ? option.receiver->player->getId()
                            : 0,
                        option.targetPoint,
                        option.intent,
                        option.utility,
                        option.progression,
                        option.laneRisk,
                        option.completionProbability};
  };

  for (auto& candidate : players)
  {
    if (&candidate == &passer || candidate.isHomeTeam != passer.isHomeTeam ||
        !candidate.player)
    {
      continue;
    }

    const float passDistance = distance(passer.position, candidate.position);
    if (passDistance < MatchTuning::Passing::MIN_DISTANCE ||
        passDistance > MatchTuning::Passing::MAX_DISTANCE)
      continue;
    if (isOffside(candidate, passer.isHomeTeam)) continue;

    PassOption option = evaluatePassOption(passer, candidate);
    if (option.utility > bestScore)
    {
      runnerUp = std::move(best);
      runnerUpScore = bestScore;
      bestScore = option.utility;
      best = std::move(option);
    }
    else if (option.utility > runnerUpScore)
    {
      runnerUpScore = option.utility;
      runnerUp = std::move(option);
    }
  }

  lastScenarioDecision.best.reset();
  lastScenarioDecision.runnerUp.reset();
  if (best && best->receiver && best->receiver->player)
  {
    lastScenarioDecision.best = toDecision(*best);
  }
  if (runnerUp && runnerUp->receiver && runnerUp->receiver->player)
  {
    lastScenarioDecision.runnerUp = toDecision(*runnerUp);
  }

  if (bestScore < MatchTuning::Passing::MIN_ACCEPTABLE_OPTION_SCORE)
    return std::nullopt;
  return best;
}

void MatchEngine::passBall(MatchPlayer& passer, const PassOption& option,
                           bool forceLofted)
{
  if (!option.receiver || !option.receiver->player) return;
  MatchPlayer& receiver = *option.receiver;
  Vector2F target = option.targetPoint;
  const Vector2F displacement{target.x - passer.position.x,
                              target.y - passer.position.y};
  const float passDistance = option.passDistance;
  if (passDistance <= EPSILON) return;
  const bool lofted = forceLofted || option.lofted;

  ball.position = passer.position;
  ball.z = MatchTuning::Pitch::RESTART_INSET;
  ball.possessedBy = nullptr;
  ball.lastPossessor = passer.player;
  ball.intendedReceiver = receiver.player;
  ball.isPass = true;
  ball.passByHome = passer.isHomeTeam;
  ball.passWasOffside =
      state != MatchState::KICK_OFF && state != MatchState::THROW_IN &&
      state != MatchState::GOAL_KICK && state != MatchState::CORNER_KICK &&
      isOffside(receiver, passer.isHomeTeam);
  ball.isShot = false;
  ball.shotXG = 0.0f;
  ball.passCooldown = MatchTuning::Passing::PASS_RELEASE_COOLDOWN;
  ball.friction = lofted ? MatchTuning::Passing::LOFTED_FRICTION
                         : MatchTuning::Passing::GROUND_FRICTION;

  const float pressure =
      std::clamp((MatchTuning::Passing::PASS_PRESSURE_RADIUS -
                  nearestOpponentDistance(passer)) /
                     MatchTuning::Passing::PASS_PRESSURE_RADIUS,
                 0.0f, 1.0f);
  lastPassDecision = {
      passer.player ? passer.player->getId() : 0,
      receiver.player ? receiver.player->getId() : 0,
      target,
      state == MatchState::PLAYING ? option.intent : PassIntent::SET_PIECE,
      option.utility,
      option.progression,
      option.laneRisk,
      option.completionProbability};
  const Vector2F direct = normalized(displacement);
  const Vector2F perpendicular{-direct.y, direct.x};
  const float error =
      ((1.0f - passer.passing) * MatchTuning::Passing::TECHNICAL_ERROR +
       pressure * MatchTuning::Passing::PRESSURE_ERROR +
       passDistance * MatchTuning::Passing::DISTANCE_ERROR) *
      randomFloat(-1.0f, 1.0f);
  target.x = std::clamp(target.x + perpendicular.x * error, 0.0f, 1.0f);
  target.y = std::clamp(target.y + perpendicular.y * error, 0.0f, 1.0f);
  const Vector2F direction =
      normalized({target.x - passer.position.x, target.y - passer.position.y});
  const float speed =
      std::clamp(passDistance / MatchTuning::Passing::SPEED_DISTANCE_SCALE,
                 MatchTuning::Passing::MIN_BALL_SPEED,
                 MatchTuning::Passing::MAX_BALL_SPEED) *
      (MatchTuning::Passing::BASE_BALL_SPEED +
       passer.passing * MatchTuning::Passing::PASSING_SPEED_BONUS);
  ball.velocity = {direction.x * speed, direction.y * speed};
  ball.velocityZ = option.intent == PassIntent::CROSS
                       ? MatchTuning::Passing::CROSS_VERTICAL_SPEED
                   : lofted ? MatchTuning::Passing::LOFTED_VERTICAL_SPEED
                            : MatchTuning::Passing::GROUND_VERTICAL_SPEED;
  ball.curve = randomFloat(-MatchTuning::Passing::MAX_CURVE,
                           MatchTuning::Passing::MAX_CURVE) *
               (MatchTuning::Passing::CURVE_SKILL_BASE + passer.passing);

  // Keep externally visible AI state consistent with the newly released
  // pass. Without this, the final fixed step of a rendered frame could expose
  // the pre-pass run assignments until the next simulation update.
  for (auto& teammate : players)
  {
    if (teammate.isHomeTeam != passer.isHomeTeam || !teammate.player) continue;
    teammate.isMakingRun = false;
    if (&teammate == &receiver)
      teammate.intent = PlayerIntent::RECEIVE_PASS;
    else if (teammate.player->getRole() != PlayerRole::GK)
      teammate.intent = PlayerIntent::OFFER_SUPPORT;
  }

  if (passer.isHomeTeam)
    ++stats.homePassesAttempted;
  else
    ++stats.awayPassesAttempted;
  if (state == MatchState::PLAYING)
  {
    int* progressivePasses = passer.isHomeTeam ? &stats.homeProgressivePasses
                                               : &stats.awayProgressivePasses;
    int* throughBalls =
        passer.isHomeTeam ? &stats.homeThroughBalls : &stats.awayThroughBalls;
    int* crosses = passer.isHomeTeam ? &stats.homeCrosses : &stats.awayCrosses;
    int* cutbacks =
        passer.isHomeTeam ? &stats.homeCutbacks : &stats.awayCutbacks;
    int* switchesOfPlay = passer.isHomeTeam ? &stats.homeSwitchesOfPlay
                                            : &stats.awaySwitchesOfPlay;
    if (option.intent == PassIntent::PROGRESSIVE)
      ++(*progressivePasses);
    else if (option.intent == PassIntent::THROUGH_BALL)
      ++(*throughBalls);
    else if (option.intent == PassIntent::CROSS)
      ++(*crosses);
    else if (option.intent == PassIntent::CUTBACK)
      ++(*cutbacks);
    else if (option.intent == PassIntent::SWITCH_PLAY)
      ++(*switchesOfPlay);
  }
  passer.actionCooldown =
      randomFloat(MatchTuning::Passing::MIN_ACTION_COOLDOWN,
                  MatchTuning::Passing::MAX_ACTION_COOLDOWN);
}

void MatchEngine::takeShot(MatchPlayer& shooter, float forcedXG)
{
  const float goalX = shooter.isHomeTeam ? MatchTuning::Pitch::RIGHT_SHOT_TARGET
                                         : MatchTuning::Pitch::LEFT_SHOT_TARGET;
  const float dxMetres =
      std::abs(goalX - shooter.position.x) * MatchTuning::Pitch::LENGTH_METRES;
  const float dyMetres =
      std::abs(MatchTuning::Pitch::CENTRE - shooter.position.y) *
      MatchTuning::Pitch::WIDTH_METRES;
  const float metres = std::sqrt(dxMetres * dxMetres + dyMetres * dyMetres);
  float xg = estimateShotXG(shooter);
  if (forcedXG >= 0.0f) xg = forcedXG;
  xg = std::clamp(xg, MatchTuning::Shooting::MIN_GOAL_PROBABILITY,
                  forcedXG >= 0.0f ? MatchTuning::Shooting::MAX_SET_PIECE_XG
                                   : MatchTuning::Shooting::MAX_OPEN_PLAY_XG);

  MatchPlayer* goalkeeper = findGoalkeeper(!shooter.isHomeTeam);
  const float goalkeeperAbility =
      goalkeeper ? goalkeeper->goalkeeping
                 : MatchTuning::Shooting::DEFAULT_GOALKEEPER_ABILITY;
  const float goalProbability =
      std::clamp(xg * (MatchTuning::Shooting::GOAL_PROBABILITY_BASE -
                       goalkeeperAbility *
                           MatchTuning::Shooting::GOALKEEPER_GOAL_REDUCTION),
                 MatchTuning::Shooting::MIN_GOAL_PROBABILITY,
                 MatchTuning::Shooting::MAX_GOAL_PROBABILITY);
  const bool willScore = randomFloat(0.0f, 1.0f) < goalProbability;
  const float onTargetProbability = std::clamp(
      MatchTuning::Shooting::BASE_ON_TARGET_PROBABILITY +
          shooter.shooting * MatchTuning::Shooting::SHOOTING_ON_TARGET_BONUS -
          metres / MatchTuning::Shooting::DISTANCE_ON_TARGET_DIVISOR,
      MatchTuning::Shooting::MIN_ON_TARGET_PROBABILITY,
      MatchTuning::Shooting::MAX_ON_TARGET_PROBABILITY);
  const bool onTarget =
      willScore || randomFloat(0.0f, 1.0f) < onTargetProbability;

  float targetY = MatchTuning::Pitch::CENTRE;
  if (onTarget)
  {
    targetY = randomFloat(
        MatchTuning::Pitch::GOAL_TOP + MatchTuning::Shooting::TARGET_POST_INSET,
        MatchTuning::Pitch::GOAL_BOTTOM -
            MatchTuning::Shooting::TARGET_POST_INSET);
  }
  else if (randomFloat(0.0f, 1.0f) <
           MatchTuning::Shooting::WIDE_TARGET_TOP_SIDE_CHANCE)
  {
    targetY = randomFloat(MatchTuning::Shooting::WIDE_TARGET_MIN_Y,
                          MatchTuning::Pitch::GOAL_TOP -
                              MatchTuning::Shooting::WIDE_TARGET_POST_MARGIN);
  }
  else
  {
    targetY = randomFloat(MatchTuning::Pitch::GOAL_BOTTOM +
                              MatchTuning::Shooting::WIDE_TARGET_POST_MARGIN,
                          MatchTuning::Shooting::WIDE_TARGET_MAX_Y);
  }

  const Vector2F direction =
      normalized({goalX - shooter.position.x, targetY - shooter.position.y});
  const float speed =
      MatchTuning::Shooting::BASE_BALL_SPEED +
      shooter.shooting * MatchTuning::Shooting::SHOOTING_SPEED_BONUS;
  ball.position = shooter.position;
  ball.z = MatchTuning::Shooting::MIN_VERTICAL_SPEED;
  ball.velocity = {direction.x * speed, direction.y * speed};
  ball.velocityZ = randomFloat(MatchTuning::Shooting::MIN_VERTICAL_SPEED,
                               MatchTuning::Shooting::MAX_VERTICAL_SPEED);
  ball.curve = randomFloat(-MatchTuning::Shooting::MAX_CURVE,
                           MatchTuning::Shooting::MAX_CURVE) *
               (MatchTuning::Shooting::CURVE_SKILL_BASE + shooter.shooting);
  ball.friction = MatchTuning::Shooting::BALL_FRICTION;
  ball.possessedBy = nullptr;
  ball.lastPossessor = shooter.player;
  ball.intendedReceiver = nullptr;
  ball.passCooldown = MatchTuning::Shooting::RELEASE_COOLDOWN;
  ball.isPass = false;
  ball.isShot = true;
  ball.shotByHome = shooter.isHomeTeam;
  ball.shotOnTarget = onTarget;
  ball.shotWillScore = willScore;
  ball.shotXG = xg;

  if (shooter.isHomeTeam)
  {
    ++stats.homeShots;
    stats.homeShotXG += xg;
    if (onTarget) ++stats.homeOnTarget;
  }
  else
  {
    ++stats.awayShots;
    stats.awayShotXG += xg;
    if (onTarget) ++stats.awayOnTarget;
  }

  std::ostringstream message;
  message << shooter.player->getName() << " shoots (xG " << std::fixed
          << std::setprecision(2) << xg << ')';
  logEvent(message.str());
  shooter.actionCooldown =
      randomFloat(MatchTuning::Shooting::MIN_ACTION_COOLDOWN,
                  MatchTuning::Shooting::MAX_ACTION_COOLDOWN);
}

void MatchEngine::updateBall(float dt)
{
  if (ball.possessedBy) return;
  ball.passCooldown = std::max(0.0f, ball.passCooldown - dt);

  if (std::abs(ball.curve) > EPSILON)
  {
    const float rotation = ball.curve * dt;
    const float cosine = std::cos(rotation);
    const float sine = std::sin(rotation);
    const Vector2F oldVelocity = ball.velocity;
    ball.velocity.x = oldVelocity.x * cosine - oldVelocity.y * sine;
    ball.velocity.y = oldVelocity.x * sine + oldVelocity.y * cosine;
  }

  ball.position.x += ball.velocity.x * dt;
  ball.position.y += ball.velocity.y * dt;
  ball.z = std::max(0.0f, ball.z + ball.velocityZ * dt);
  ball.velocityZ -= MatchTuning::Ball::GRAVITY * dt;
  if (ball.z <= 0.0f && ball.velocityZ < 0.0f)
  {
    ball.velocityZ = -ball.velocityZ * MatchTuning::Ball::BOUNCE_FACTOR;
    if (ball.velocityZ < MatchTuning::Ball::MIN_BOUNCE_SPEED)
      ball.velocityZ = 0.0f;
  }

  const float drag = std::pow(
      ball.friction, dt / MatchTuning::Timing::PHYSICS_REFERENCE_STEP_SECONDS);
  ball.velocity.x *= drag;
  ball.velocity.y *= drag;
  ball.curve *=
      std::pow(MatchTuning::Ball::CURVE_DECAY,
               dt / MatchTuning::Timing::PHYSICS_REFERENCE_STEP_SECONDS);
  if (length(ball.velocity) < MatchTuning::Ball::STOP_SPEED)
    ball.velocity = {0.0f, 0.0f};
}

void MatchEngine::resolveLooseBall()
{
  if (ball.possessedBy || ball.passCooldown > 0.0f) return;

  MatchPlayer* closest = nullptr;
  float closestDistance = std::numeric_limits<float>::max();
  for (auto& player : players)
  {
    const float playerDistance = distance(player.position, ball.position);
    if (playerDistance < closestDistance)
    {
      closestDistance = playerDistance;
      closest = &player;
    }
  }
  if (!closest || !closest->player) return;

  if (ball.isShot)
  {
    const bool defendingGoal = closest->isHomeTeam != ball.shotByHome;
    const bool goalkeeper = closest->player->getRole() == PlayerRole::GK;
    if (defendingGoal && goalkeeper && ball.shotOnTarget &&
        !ball.shotWillScore &&
        closestDistance < MatchTuning::Ball::SAVE_DISTANCE)
    {
      makeSave(*closest);
      return;
    }
    if (defendingGoal && !goalkeeper &&
        closestDistance < MatchTuning::Defending::BLOCK_DISTANCE &&
        randomFloat(0.0f, 1.0f) <
            MatchTuning::Defending::BASE_BLOCK_CHANCE +
                closest->defending *
                    MatchTuning::Defending::DEFENDING_BLOCK_BONUS)
    {
      ball.lastPossessor = closest->player;
      ball.isShot = false;
      ball.shotOnTarget = false;
      ball.shotWillScore = false;
      ball.velocity.x *= MatchTuning::Defending::DEFLECTION_SPEED_FACTOR;
      ball.velocity.y +=
          randomFloat(-MatchTuning::Defending::MAX_DEFLECTION_Y_SPEED,
                      MatchTuning::Defending::MAX_DEFLECTION_Y_SPEED);
      ball.passCooldown = MatchTuning::Defending::DEFLECTION_COOLDOWN;
      logEvent(closest->player->getName() + " blocks the shot");
    }
    return;
  }

  const float controlRadius = closest->player->getRole() == PlayerRole::GK
                                  ? MatchTuning::Ball::GOALKEEPER_CONTROL_RADIUS
                                  : MatchTuning::Ball::OUTFIELD_CONTROL_RADIUS;
  if (closestDistance > controlRadius) return;

  const float ballSpeed = length(ball.velocity);
  const float touchSkill =
      std::max(closest->dribbling,
               closest->passing * MatchTuning::Ball::PASSING_TOUCH_WEIGHT);
  const float controlChance =
      std::clamp(MatchTuning::Ball::BASE_CONTROL_CHANCE +
                     touchSkill * MatchTuning::Ball::TOUCH_SKILL_BONUS -
                     ballSpeed * MatchTuning::Ball::SPEED_CONTROL_PENALTY,
                 MatchTuning::Ball::MIN_CONTROL_CHANCE,
                 MatchTuning::Ball::MAX_CONTROL_CHANCE);
  if (randomFloat(0.0f, 1.0f) > controlChance)
  {
    closest->isTrapping = true;
    closest->trapTimer = MatchTuning::Ball::FAILED_TRAP_TIME;
    ball.passCooldown = MatchTuning::Ball::FAILED_TOUCH_DELAY;
    return;
  }

  if (ball.isPass && ball.passWasOffside &&
      ball.intendedReceiver == closest->player)
  {
    if (closest->isHomeTeam)
      ++stats.homeOffsides;
    else
      ++stats.awayOffsides;
    logEvent("Offside: " + closest->player->getName());
    setupFreeKick(!closest->isHomeTeam, closest->position);
    return;
  }

  if (ball.isPass && closest->isHomeTeam == ball.passByHome)
  {
    if (closest->isHomeTeam)
      ++stats.homePassesCompleted;
    else
      ++stats.awayPassesCompleted;
  }
  setPossession(*closest);
  closest->isTrapping = true;
  closest->trapTimer =
      MatchTuning::Ball::BASE_TRAP_TIME +
      (1.0f - touchSkill) * MatchTuning::Ball::TRAP_SKILL_PENALTY;
  closest->actionCooldown =
      closest->trapTimer + randomFloat(MatchTuning::Ball::MIN_POST_TOUCH_DELAY,
                                       MatchTuning::Ball::MAX_POST_TOUCH_DELAY);
}

void MatchEngine::setPossession(MatchPlayer& player)
{
  if (state == MatchState::PLAYING && lastControlledTeamHome &&
      *lastControlledTeamHome != player.isHomeTeam)
  {
    transitionSecondsRemaining =
        MatchTuning::Timing::POSSESSION_TRANSITION_SECONDS;
  }
  lastControlledTeamHome = player.isHomeTeam;
  ball.possessedBy = player.player;
  ball.lastPossessor = player.player;
  ball.position = player.position;
  ball.z = 0.0f;
  ball.velocity = {0.0f, 0.0f};
  ball.velocityZ = 0.0f;
  clearFlightState();
  updateTeamPhases();
}

void MatchEngine::clearFlightState()
{
  ball.intendedReceiver = nullptr;
  ball.isPass = false;
  ball.passWasOffside = false;
  ball.isShot = false;
  ball.shotOnTarget = false;
  ball.shotWillScore = false;
  ball.shotXG = 0.0f;
  ball.curve = 0.0f;
}

void MatchEngine::checkOutOfBounds()
{
  if (ball.position.x <= 0.0f || ball.position.x >= 1.0f)
  {
    const bool rightGoalLine = ball.position.x >= 1.0f;
    const bool inGoal = ball.position.y >= MatchTuning::Pitch::GOAL_TOP &&
                        ball.position.y <= MatchTuning::Pitch::GOAL_BOTTOM;
    if (inGoal && ball.isShot && ball.shotOnTarget)
    {
      if (ball.shotWillScore)
      {
        scoreGoal(ball.shotByHome);
      }
      else if (MatchPlayer* goalkeeper = findGoalkeeper(!ball.shotByHome))
      {
        makeSave(*goalkeeper);
      }
      return;
    }

    const bool lastTouchHome = isHomePlayer(ball.lastPossessor);
    if (!rightGoalLine)
    {
      // Home defends the left goal.
      if (lastTouchHome)
        setupCorner(false, ball.position.y < MatchTuning::Pitch::CENTRE);
      else
        setupGoalKick(true);
    }
    else
    {
      // Away defends the right goal.
      if (!lastTouchHome && ball.lastPossessor)
        setupCorner(true, ball.position.y < MatchTuning::Pitch::CENTRE);
      else
        setupGoalKick(false);
    }
    return;
  }

  if (ball.position.y <= 0.0f || ball.position.y >= 1.0f)
  {
    const bool receivingHome = !isHomePlayer(ball.lastPossessor);
    setupThrowIn(receivingHome);
  }
}

void MatchEngine::scoreGoal(bool homeTeam)
{
  if (homeTeam)
    ++homeScore;
  else
    ++awayScore;

  const std::string scorer =
      ball.lastPossessor ? ball.lastPossessor->getName() : "Unknown player";
  logEvent("GOAL! " + scorer + " (" + std::to_string(homeScore) + '-' +
           std::to_string(awayScore) + ')');

  goalScoredByHome = homeTeam;
  goalCelebrationRemaining = MatchTuning::Timing::GOAL_CELEBRATION_SECONDS;
  state = MatchState::GOAL;
  ball.possessedBy = nullptr;
  updateTeamPhases();
}

void MatchEngine::updateBallInNet(float dt)
{
  updateBall(dt);
  const float netDepth = MatchTuning::Ball::GOAL_NET_BALL_DEPTH;
  if (ball.shotByHome)
  {
    ball.position.x = std::clamp(ball.position.x, 1.0f, 1.0f + netDepth);
  }
  else
  {
    ball.position.x = std::clamp(ball.position.x, -netDepth, 0.0f);
  }
  ball.position.y = std::clamp(ball.position.y, MatchTuning::Pitch::GOAL_TOP,
                               MatchTuning::Pitch::GOAL_BOTTOM);
  // The ball settles into the net instead of bouncing out of the goal mouth.
  ball.velocityZ = 0.0f;
  ball.z = 0.0f;
  ball.curve = 0.0f;
}

void MatchEngine::makeSave(MatchPlayer& goalkeeper)
{
  if (goalkeeper.isHomeTeam)
    ++stats.homeSaves;
  else
    ++stats.awaySaves;
  goalkeeper.isDiving = true;
  goalkeeper.diveTimer = MatchTuning::Ball::SAVE_DIVE_TIME;
  setPossession(goalkeeper);
  goalkeeper.actionCooldown = MatchTuning::Ball::SAVE_ACTION_COOLDOWN;
  state = MatchState::PLAYING;
  updateTeamPhases();
  logEvent(goalkeeper.player->getName() + " makes a save");
}

void MatchEngine::setupKickOff(bool homeKickingOff)
{
  resetPositions();
  // At kick-off every player must be in their own half. The normal tactical
  // positions deliberately span more of the pitch and are restored through
  // regular movement once play starts.
  for (auto& player : players)
  {
    if (player.isHomeTeam)
      player.position.x =
          MatchTuning::Pitch::KICKOFF_FORMATION_INSET +
          player.basePosition.x * MatchTuning::Pitch::KICKOFF_FORMATION_SCALE;
    else
      player.position.x = (1.0f - MatchTuning::Pitch::KICKOFF_FORMATION_INSET) -
                          (1.0f - player.basePosition.x) *
                              MatchTuning::Pitch::KICKOFF_FORMATION_SCALE;
  }
  ball = MatchBall{};
  const Vector2F centre{MatchTuning::Pitch::CENTRE, MatchTuning::Pitch::CENTRE};
  restartTaker = findClosestPlayer(centre, homeKickingOff, false);
  if (!restartTaker)
    restartTaker = findClosestPlayer(centre, homeKickingOff, true);
  if (restartTaker)
  {
    restartTaker->position = {homeKickingOff
                                  ? MatchTuning::Pitch::HOME_KICKOFF_X
                                  : MatchTuning::Pitch::AWAY_KICKOFF_X,
                              MatchTuning::Pitch::CENTRE};
    ball.possessedBy = restartTaker->player;
    ball.lastPossessor = restartTaker->player;
  }
  state = MatchState::KICK_OFF;
  setPieceTimer = MatchTuning::Timing::KICKOFF_DELAY_SECONDS;
  updateTeamPhases();
}

void MatchEngine::setupThrowIn(bool homeTeam)
{
  ball.position.x = std::clamp(
      ball.position.x, MatchTuning::Pitch::RESTART_LONGITUDINAL_MARGIN,
      1.0f - MatchTuning::Pitch::RESTART_LONGITUDINAL_MARGIN);
  ball.position.y = ball.position.y < MatchTuning::Pitch::CENTRE
                        ? MatchTuning::Pitch::RESTART_INSET
                        : 1.0f - MatchTuning::Pitch::RESTART_INSET;
  restartTaker = findClosestPlayer(ball.position, homeTeam, false);
  ball.velocity = {0.0f, 0.0f};
  ball.possessedBy = restartTaker ? restartTaker->player : nullptr;
  ball.lastPossessor = ball.possessedBy;
  clearFlightState();
  state = MatchState::THROW_IN;
  setPieceTimer = MatchTuning::Timing::THROW_IN_DELAY_SECONDS;
  updateTeamPhases();
  logEvent(homeTeam ? "Throw-in to home" : "Throw-in to away");
}

void MatchEngine::setupGoalKick(bool homeTeam)
{
  ball.position = {homeTeam ? MatchTuning::Pitch::LEFT_GOAL_KICK_X
                            : MatchTuning::Pitch::RIGHT_GOAL_KICK_X,
                   MatchTuning::Pitch::CENTRE};
  restartTaker = findGoalkeeper(homeTeam);
  if (!restartTaker)
    restartTaker = findClosestPlayer(ball.position, homeTeam, true);
  ball.velocity = {0.0f, 0.0f};
  ball.possessedBy = restartTaker ? restartTaker->player : nullptr;
  ball.lastPossessor = ball.possessedBy;
  clearFlightState();
  state = MatchState::GOAL_KICK;
  setPieceTimer = MatchTuning::Timing::GOAL_KICK_DELAY_SECONDS;
  updateTeamPhases();
  logEvent(homeTeam ? "Goal kick to home" : "Goal kick to away");
}

void MatchEngine::setupCorner(bool homeTeam, bool topCorner)
{
  ball.position = {homeTeam ? 1.0f - MatchTuning::Pitch::RESTART_INSET
                            : MatchTuning::Pitch::RESTART_INSET,
                   topCorner ? MatchTuning::Pitch::RESTART_INSET
                             : 1.0f - MatchTuning::Pitch::RESTART_INSET};
  restartTaker = findClosestPlayer(ball.position, homeTeam, false);
  ball.velocity = {0.0f, 0.0f};
  ball.possessedBy = restartTaker ? restartTaker->player : nullptr;
  ball.lastPossessor = ball.possessedBy;
  clearFlightState();
  state = MatchState::CORNER_KICK;
  setPieceTimer = MatchTuning::Timing::CORNER_DELAY_SECONDS;
  updateTeamPhases();
  if (homeTeam)
    ++stats.homeCorners;
  else
    ++stats.awayCorners;
  logEvent(homeTeam ? "Corner to home" : "Corner to away");
}

void MatchEngine::setupFreeKick(bool homeTeam, Vector2F foulPos)
{
  ball.position = {
      std::clamp(foulPos.x, MatchTuning::Pitch::RESTART_LONGITUDINAL_MARGIN,
                 1.0f - MatchTuning::Pitch::RESTART_LONGITUDINAL_MARGIN),
      std::clamp(foulPos.y, MatchTuning::Pitch::RESTART_LONGITUDINAL_MARGIN,
                 1.0f - MatchTuning::Pitch::RESTART_LONGITUDINAL_MARGIN)};
  restartTaker = findClosestPlayer(ball.position, homeTeam, false);
  ball.velocity = {0.0f, 0.0f};
  ball.possessedBy = restartTaker ? restartTaker->player : nullptr;
  ball.lastPossessor = ball.possessedBy;
  clearFlightState();
  state = MatchState::FREE_KICK;
  setPieceTimer = MatchTuning::Timing::FREE_KICK_DELAY_SECONDS;
  updateTeamPhases();
  logEvent(homeTeam ? "Free kick to home" : "Free kick to away");
}

void MatchEngine::setupPenalty(bool homeTeam)
{
  const Vector2F spot{homeTeam ? MatchTuning::Pitch::RIGHT_PENALTY_SPOT_X
                               : MatchTuning::Pitch::LEFT_PENALTY_SPOT_X,
                      MatchTuning::Pitch::CENTRE};
  MatchPlayer* bestTaker = nullptr;
  for (auto& player : players)
  {
    if (player.isHomeTeam == homeTeam &&
        (!bestTaker || player.shooting > bestTaker->shooting))
    {
      bestTaker = &player;
    }
  }
  restartTaker = bestTaker;
  ball.position = spot;
  ball.velocity = {0.0f, 0.0f};
  ball.possessedBy = restartTaker ? restartTaker->player : nullptr;
  ball.lastPossessor = ball.possessedBy;
  if (restartTaker) restartTaker->position = spot;
  clearFlightState();
  state = MatchState::PENALTY;
  setPieceTimer = MatchTuning::Timing::PENALTY_DELAY_SECONDS;
  updateTeamPhases();
  logEvent(homeTeam ? "Penalty to home" : "Penalty to away");
}

void MatchEngine::completeRestart()
{
  const MatchState restartState = state;
  MatchPlayer* taker = restartTaker;
  restartTaker = nullptr;
  if (!taker || !taker->player)
  {
    state = MatchState::PLAYING;
    updateTeamPhases();
    return;
  }

  if (restartState == MatchState::PENALTY)
  {
    takeShot(*taker, MatchTuning::Shooting::PENALTY_XG);
    state = MatchState::PLAYING;
    updateTeamPhases();
    return;
  }

  std::optional<PassOption> passOption;
  if (restartState == MatchState::CORNER_KICK)
  {
    MatchPlayer* receiver = nullptr;
    float bestScore = -std::numeric_limits<float>::infinity();
    for (auto& candidate : players)
    {
      if (&candidate == taker || candidate.isHomeTeam != taker->isHomeTeam ||
          candidate.player->getRole() == PlayerRole::GK)
      {
        continue;
      }
      const float attackDepth = taker->isHomeTeam ? candidate.position.x
                                                  : 1.0f - candidate.position.x;
      const float score = attackDepth - std::abs(candidate.position.y -
                                                 MatchTuning::Pitch::CENTRE);
      if (score > bestScore)
      {
        bestScore = score;
        receiver = &candidate;
      }
    }
    if (receiver) passOption = evaluatePassOption(*taker, *receiver);
  }
  else
  {
    passOption = choosePassTarget(*taker);
  }

  if (passOption)
    passBall(*taker, *passOption,
             restartState == MatchState::CORNER_KICK ||
                 restartState == MatchState::GOAL_KICK);
  else
    setPossession(*taker);
  state = MatchState::PLAYING;
  updateTeamPhases();
}

void MatchEngine::resetPositions()
{
  for (auto& player : players)
  {
    player.position = player.basePosition;
    player.movementTarget = player.basePosition;
    player.velocity = {0.0f, 0.0f};
    player.intent = PlayerIntent::HOLD_SHAPE;
    player.isPressing = false;
    player.isMakingRun = false;
    player.stamina = std::min(
        1.0f, player.stamina + MatchTuning::Player::HALF_TIME_STAMINA_RECOVERY);
    player.actionCooldown = 0.0f;
  }
}

MatchPlayer* MatchEngine::findClosestPlayer(Vector2F position, bool homeTeam,
                                            bool includeGoalkeeper)
{
  MatchPlayer* closest = nullptr;
  float bestDistance = std::numeric_limits<float>::max();
  for (auto& player : players)
  {
    if (player.isHomeTeam != homeTeam || !player.player ||
        (!includeGoalkeeper && player.player->getRole() == PlayerRole::GK))
    {
      continue;
    }
    const float candidateDistance = distance(position, player.position);
    if (candidateDistance < bestDistance)
    {
      bestDistance = candidateDistance;
      closest = &player;
    }
  }
  return closest;
}

MatchPlayer* MatchEngine::findGoalkeeper(bool homeTeam)
{
  const auto goalkeeper = std::ranges::find_if(
      players,
      [homeTeam](const MatchPlayer& player)
      {
        return player.isHomeTeam == homeTeam && player.player &&
               player.player->getRole() == PlayerRole::GK;
      });
  return goalkeeper == players.end() ? nullptr : &*goalkeeper;
}

MatchPlayer* MatchEngine::findMatchPlayer(const Player* player)
{
  if (!player) return nullptr;
  const auto found =
      std::ranges::find_if(players, [player](const MatchPlayer& matchPlayer)
                           { return matchPlayer.player == player; });
  return found == players.end() ? nullptr : &*found;
}

float MatchEngine::nearestOpponentDistance(const MatchPlayer& player) const
{
  float bestDistance = std::numeric_limits<float>::max();
  for (const auto& opponent : players)
  {
    if (opponent.isHomeTeam != player.isHomeTeam)
    {
      bestDistance =
          std::min(bestDistance, distance(player.position, opponent.position));
    }
  }
  return bestDistance;
}

float MatchEngine::openSpaceAhead(const MatchPlayer& carrier) const
{
  const float direction = carrier.isHomeTeam ? 1.0f : -1.0f;
  const float probeDepth =
      std::clamp(carrier.position.x + direction * 0.16f, 0.02f, 0.98f);
  constexpr float CHANNELS[3] = {0.25f, 0.50f, 0.75f};
  float openness = 0.0f;
  for (const float channelY : CHANNELS)
  {
    const Vector2F probe{probeDepth, channelY};
    float nearest = std::numeric_limits<float>::max();
    for (const auto& opponent : players)
    {
      if (opponent.isHomeTeam == carrier.isHomeTeam) continue;
      nearest = std::min(nearest, distance(probe, opponent.position));
    }
    openness += std::clamp(nearest / 0.16f, 0.0f, 1.0f);
  }
  return openness / 3.0f;
}

float MatchEngine::passingLaneRisk(const MatchPlayer& passer,
                                   const MatchPlayer& receiver) const
{
  const Vector2F segment{receiver.position.x - passer.position.x,
                         receiver.position.y - passer.position.y};
  const float segmentLengthSquared =
      segment.x * segment.x + segment.y * segment.y;
  if (segmentLengthSquared <= EPSILON) return 1.0f;

  float combinedSafety = 1.0f;
  for (const auto& opponent : players)
  {
    if (!opponent.player || opponent.isHomeTeam == passer.isHomeTeam) continue;
    const Vector2F fromPasser{opponent.position.x - passer.position.x,
                              opponent.position.y - passer.position.y};
    const float projection =
        std::clamp((fromPasser.x * segment.x + fromPasser.y * segment.y) /
                       segmentLengthSquared,
                   0.0f, 1.0f);
    if (projection < MatchTuning::Passing::LANE_START_MARGIN ||
        projection > MatchTuning::Passing::LANE_END_MARGIN)
      continue;

    const Vector2F lanePoint{passer.position.x + segment.x * projection,
                             passer.position.y + segment.y * projection};
    const float laneDistance = distance(lanePoint, opponent.position);
    const float interceptionRadius =
        MatchTuning::Passing::BASE_INTERCEPTION_RADIUS +
        opponent.defending *
            MatchTuning::Passing::DEFENDING_INTERCEPTION_BONUS +
        opponent.pace * MatchTuning::Passing::PACE_INTERCEPTION_BONUS;
    const float individualRisk =
        std::clamp(1.0f - laneDistance / std::max(interceptionRadius, EPSILON),
                   0.0f, 1.0f);
    combinedSafety *=
        1.0f -
        individualRisk * (MatchTuning::Passing::BASE_INTERCEPTION_RISK +
                          projection * MatchTuning::Passing::LATE_LANE_RISK);
  }
  return std::clamp(1.0f - combinedSafety, 0.0f, 1.0f);
}

float MatchEngine::estimateShotXG(const MatchPlayer& shooter) const
{
  const float goalX = shooter.isHomeTeam ? 1.0f : 0.0f;
  const float dxMetres =
      std::abs(goalX - shooter.position.x) * MatchTuning::Pitch::LENGTH_METRES;
  const float dyMetres =
      std::abs(MatchTuning::Pitch::CENTRE - shooter.position.y) *
      MatchTuning::Pitch::WIDTH_METRES;
  const float metres = std::sqrt(dxMetres * dxMetres + dyMetres * dyMetres);

  const Vector2F toTop{goalX - shooter.position.x,
                       MatchTuning::Pitch::GOAL_TOP - shooter.position.y};
  const Vector2F toBottom{goalX - shooter.position.x,
                          MatchTuning::Pitch::GOAL_BOTTOM - shooter.position.y};
  const float denominator = length(toTop) * length(toBottom);
  const float cosine =
      denominator > EPSILON
          ? std::clamp(
                (toTop.x * toBottom.x + toTop.y * toBottom.y) / denominator,
                -1.0f, 1.0f)
          : 1.0f;
  const float visibleGoalAngle = std::acos(cosine);
  const float angleFactor = std::clamp(
      visibleGoalAngle / MatchTuning::Shooting::GOAL_ANGLE_REFERENCE_RADIANS,
      MatchTuning::Shooting::MIN_ANGLE_FACTOR, 1.0f);
  const float distanceFactor =
      1.0f /
      (1.0f +
       std::exp((metres - MatchTuning::Shooting::DISTANCE_MIDPOINT_METRES) /
                MatchTuning::Shooting::DISTANCE_CURVE_METRES));
  const float pressure = std::clamp((MatchTuning::Shooting::PRESSURE_RADIUS -
                                     nearestOpponentDistance(shooter)) /
                                        MatchTuning::Shooting::PRESSURE_RADIUS,
                                    0.0f, 1.0f);
  const float centrality =
      std::exp(-dyMetres / MatchTuning::Shooting::CENTRALITY_METRES);
  return std::clamp(
      distanceFactor *
          (MatchTuning::Shooting::BASE_ANGLE_FACTOR +
           angleFactor * MatchTuning::Shooting::ANGLE_FACTOR_BONUS) *
          (MatchTuning::Shooting::BASE_SKILL_FACTOR +
           shooter.shooting * MatchTuning::Shooting::SHOOTING_SKILL_FACTOR) *
          (1.0f - pressure * MatchTuning::Shooting::PRESSURE_PENALTY) *
          (MatchTuning::Shooting::BASE_CENTRALITY +
           centrality * MatchTuning::Shooting::CENTRALITY_BONUS),
      MatchTuning::Shooting::MIN_OPEN_PLAY_XG,
      MatchTuning::Shooting::MAX_OPEN_PLAY_XG);
}

float MatchEngine::offsideLine(bool attackingHome) const
{
  float nearestGoalDefender = attackingHome
                                  ? -std::numeric_limits<float>::infinity()
                                  : std::numeric_limits<float>::infinity();
  float secondNearestGoalDefender = nearestGoalDefender;
  std::size_t defenderCount = 0;
  for (const auto& player : players)
  {
    if (player.isHomeTeam == attackingHome) continue;
    ++defenderCount;
    const float position = player.position.x;
    if (attackingHome)
    {
      if (position >= nearestGoalDefender)
      {
        secondNearestGoalDefender = nearestGoalDefender;
        nearestGoalDefender = position;
      }
      else if (position > secondNearestGoalDefender)
      {
        secondNearestGoalDefender = position;
      }
    }
    else
    {
      if (position <= nearestGoalDefender)
      {
        secondNearestGoalDefender = nearestGoalDefender;
        nearestGoalDefender = position;
      }
      else if (position < secondNearestGoalDefender)
      {
        secondNearestGoalDefender = position;
      }
    }
  }
  if (defenderCount < 2) return attackingHome ? 1.0f : 0.0f;
  return secondNearestGoalDefender;
}

bool MatchEngine::isOffside(const MatchPlayer& receiver,
                            bool attackingHome) const
{
  const float defenderLine = offsideLine(attackingHome);

  if (attackingHome)
  {
    return receiver.position.x > MatchTuning::Pitch::CENTRE &&
           receiver.position.x >
               ball.position.x + MatchTuning::Passing::OFFSIDE_MARGIN &&
           receiver.position.x >
               defenderLine + MatchTuning::Passing::OFFSIDE_MARGIN;
  }

  return receiver.position.x < MatchTuning::Pitch::CENTRE &&
         receiver.position.x <
             ball.position.x - MatchTuning::Passing::OFFSIDE_MARGIN &&
         receiver.position.x <
             defenderLine - MatchTuning::Passing::OFFSIDE_MARGIN;
}

float MatchEngine::attribute(const Player* player, std::string_view name) const
{
  if (!player) return MatchTuning::Player::DEFAULT_ATTRIBUTE;
  const auto stat = player->getStats().find(std::string(name));
  if (stat != player->getStats().end())
  {
    return std::clamp(stat->second / MatchTuning::Player::RATING_SCALE, 0.0f,
                      1.0f);
  }
  return std::clamp(static_cast<float>(player->getOverall(statsConfig)) /
                        MatchTuning::Player::RATING_SCALE,
                    0.0f, 1.0f);
}

float MatchEngine::randomFloat(float minimum, float maximum)
{
  std::uniform_real_distribution<float> distribution(minimum, maximum);
  return distribution(rng);
}

bool MatchEngine::isHomePlayer(const Player* player) const
{
  if (!player) return false;
  const auto found =
      std::ranges::find_if(players, [player](const MatchPlayer& matchPlayer)
                           { return matchPlayer.player == player; });
  return found != players.end() && found->isHomeTeam;
}

void MatchEngine::substitutePlayer(uint32_t outPlayerId, const Player* inPlayer)
{
  if (!inPlayer || state == MatchState::FULL_TIME ||
      std::ranges::any_of(players, [inPlayer](const MatchPlayer& player)
                          { return player.player == inPlayer; }))
  {
    return;
  }

  const auto outgoing = std::ranges::find_if(
      players, [outPlayerId](const MatchPlayer& player)
      { return player.player && player.player->getId() == outPlayerId; });
  if (outgoing == players.end()) return;
  if (outgoing->player->getTeamId() != inPlayer->getTeamId()) return;

  int& substitutionsUsed =
      outgoing->isHomeTeam ? homeSubstitutions : awaySubstitutions;
  if (substitutionsUsed >= MatchTuning::Rules::MAX_SUBSTITUTIONS_PER_TEAM)
    return;

  const bool hadPossession = ball.possessedBy == outgoing->player;
  const std::string outgoingName = outgoing->player->getName();
  outgoing->player = inPlayer;
  outgoing->pace = attribute(inPlayer, "Pace");
  outgoing->shooting = attribute(inPlayer, "Shooting");
  outgoing->passing = attribute(inPlayer, "Passing");
  outgoing->dribbling = attribute(inPlayer, "Dribbling");
  outgoing->defending = attribute(inPlayer, "Defending");
  outgoing->goalkeeping = attribute(inPlayer, "Goalkeeping");
  outgoing->maxSpeed = MatchTuning::Player::BASE_MAX_SPEED +
                       outgoing->pace * MatchTuning::Player::PACE_SPEED_BONUS;
  outgoing->acceleration =
      MatchTuning::Player::BASE_ACCELERATION +
      outgoing->pace * MatchTuning::Player::PACE_ACCELERATION_BONUS;
  outgoing->stamina = 1.0f;
  outgoing->actionCooldown = MatchTuning::Player::SUBSTITUTION_SETTLE_SECONDS;
  ++substitutionsUsed;
  if (hadPossession) ball.possessedBy = inPlayer;
  logEvent(outgoingName + " is replaced by " + inPlayer->getName());
}

bool MatchEngine::applyScenario(const MatchScenario& scenario,
                                MatchState scenarioState,
                                float scenarioMatchTime, int scenarioHomeScore,
                                int scenarioAwayScore)
{
  if (scenario.players.empty() || state == MatchState::FULL_TIME) return false;

  ball = MatchBall{};
  state = scenarioState;
  transitionSecondsRemaining = 0.0f;
  matchTimeMinutes = scenarioMatchTime;
  homeScore = scenarioHomeScore;
  awayScore = scenarioAwayScore;
  accumulator = 0.0f;
  lastPassDecision = PassDecision{};
  lastScenarioDecision.best.reset();
  lastScenarioDecision.runnerUp.reset();
  lastScenarioDecision.reason.clear();
  lastScenarioDecision.passUtility = -std::numeric_limits<float>::infinity();
  lastScenarioDecision.shotUtility = -std::numeric_limits<float>::infinity();
  lastScenarioDecision.carryUtility = -std::numeric_limits<float>::infinity();
  lastScenarioDecision.shieldUtility = -std::numeric_limits<float>::infinity();

  ball.position = scenario.ballPosition;
  bool carrierFound = false;
  for (const auto& placement : scenario.players)
  {
    const auto found = std::ranges::find_if(
        players,
        [placement](const MatchPlayer& matchPlayer)
        {
          return matchPlayer.player &&
                 matchPlayer.player->getId() == placement.playerId;
        });
    if (found == players.end()) continue;
    MatchPlayer& matchPlayer = *found;
    matchPlayer.position = placement.position;
    matchPlayer.velocity = {0.0f, 0.0f};
    matchPlayer.basePosition = placement.position;
    matchPlayer.movementTarget = placement.position;
    matchPlayer.isTrapping = false;
    matchPlayer.trapTimer = 0.0f;
    matchPlayer.isPressing = false;
    matchPlayer.isMakingRun = false;
    matchPlayer.stamina = 1.0f;
    matchPlayer.actionCooldown = 0.0f;
    matchPlayer.tackleCooldown = 0.0f;
    matchPlayer.isMakingRun = placement.makingRun;
    carrierFound = carrierFound || placement.playerId == scenario.carrierId;
  }
  if (!carrierFound) return false;

  const auto carrier = std::ranges::find_if(
      players,
      [&scenario](const MatchPlayer& matchPlayer)
      {
        return matchPlayer.player &&
               matchPlayer.player->getId() == scenario.carrierId;
      });
  if (carrier == players.end()) return false;

  ball.possessedBy = carrier->player;
  ball.lastPossessor = carrier->player;
  if (carrier->isHomeTeam)
  {
    homePhase = TeamPhase::POSSESSION;
    awayPhase = TeamPhase::DEFENSIVE_BLOCK;
  }
  else
  {
    awayPhase = TeamPhase::POSSESSION;
    homePhase = TeamPhase::DEFENSIVE_BLOCK;
  }

  // Make the interpolation baseline equal to the scenario so the snapshot is
  // stable, then evaluate the decision through the normal live path.
  captureInterpolationFrame();
  decideAction(*carrier);
  return true;
}

void MatchEngine::logEvent(const std::string& message)
{
  if (events.size() >= MatchTuning::Timing::MAX_EVENTS)
    events.erase(events.begin());
  events.push_back({matchTimeMinutes, message});
}

namespace
{
std::string jsonFloat(float value)
{
  return std::isfinite(value) ? std::to_string(value) : std::string("null");
}

std::string jsonEscape(std::string_view value)
{
  std::string escaped;
  escaped.reserve(value.size());
  for (const char c : value)
  {
    switch (c)
    {
      case '"':
        escaped += "\\\"";
        break;
      case '\\':
        escaped += "\\\\";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        if (static_cast<unsigned char>(c) < 0x20U)
        {
          escaped += "\\u00";
          constexpr char HEX[] = "0123456789abcdef";
          escaped += HEX[(c >> 4U) & 0x0fU];
          escaped += HEX[c & 0x0fU];
        }
        else
        {
          escaped += c;
        }
    }
  }
  return escaped;
}
}  // namespace

std::string MatchEngine::getDebugSnapshotJson() const
{
  std::ostringstream output;
  output << std::fixed << std::setprecision(4);
  output << "{\"time_minute\":" << matchTimeMinutes << ",\"state\":\""
         << stateName(state) << "\",\"score\":{\"home\":" << homeScore
         << ",\"away\":" << awayScore << "},\"team_phase\":{\"home\":\""
         << teamPhaseName(homePhase) << "\",\"away\":\""
         << teamPhaseName(awayPhase) << '"'
         << ",\"transition_seconds_remaining\":" << transitionSecondsRemaining
         << "},\"performance\":{\"last_update_steps\":" << lastUpdateStepCount
         << ",\"dropped_simulation_steps\":" << droppedSimulationSteps
         << "},\"last_pass\":";
  if (lastPassDecision.receiverId == 0)
  {
    output << "null";
  }
  else
  {
    output << "{\"passer\":" << lastPassDecision.passerId
           << ",\"receiver\":" << lastPassDecision.receiverId
           << ",\"intent\":\"" << passIntentName(lastPassDecision.intent)
           << "\",\"target_x\":" << lastPassDecision.targetPoint.x
           << ",\"target_y\":" << lastPassDecision.targetPoint.y
           << ",\"utility\":" << lastPassDecision.utility
           << ",\"progression\":" << lastPassDecision.progression
           << ",\"lane_risk\":" << lastPassDecision.laneRisk
           << ",\"completion_probability\":"
           << lastPassDecision.completionProbability << '}';
  }
  output << ",\"decision\":{\"reason\":\""
         << jsonEscape(lastScenarioDecision.reason) << "\",\"action\":\""
         << scenarioActionName(lastScenarioDecision.action)
         << "\",\"analysis\":{\"pass\":"
         << jsonFloat(lastScenarioDecision.passUtility)
         << ",\"shot\":" << jsonFloat(lastScenarioDecision.shotUtility)
         << ",\"carry\":" << jsonFloat(lastScenarioDecision.carryUtility)
         << ",\"shield\":" << jsonFloat(lastScenarioDecision.shieldUtility)
         << "},\"best\":";
  if (lastScenarioDecision.best)
  {
    output << "{\"receiver\":" << lastScenarioDecision.best->receiverId
           << ",\"intent\":\""
           << passIntentName(lastScenarioDecision.best->intent)
           << "\",\"utility\":" << lastScenarioDecision.best->utility
           << ",\"progression\":" << lastScenarioDecision.best->progression
           << ",\"lane_risk\":" << lastScenarioDecision.best->laneRisk
           << ",\"completion_probability\":"
           << lastScenarioDecision.best->completionProbability << '}';
  }
  else
  {
    output << "null";
  }
  output << ",\"rejected\":";
  if (lastScenarioDecision.runnerUp)
  {
    output << "{\"receiver\":" << lastScenarioDecision.runnerUp->receiverId
           << ",\"intent\":\""
           << passIntentName(lastScenarioDecision.runnerUp->intent)
           << "\",\"utility\":" << lastScenarioDecision.runnerUp->utility
           << ",\"progression\":" << lastScenarioDecision.runnerUp->progression
           << ",\"lane_risk\":" << lastScenarioDecision.runnerUp->laneRisk
           << ",\"completion_probability\":"
           << lastScenarioDecision.runnerUp->completionProbability << '}';
  }
  else
  {
    output << "null";
  }
  output << "},\"ball\":{\"x\":" << ball.position.x
         << ",\"y\":" << ball.position.y << ",\"z\":" << ball.z
         << ",\"possessed_by\":";
  if (ball.possessedBy)
    output << ball.possessedBy->getId();
  else
    output << "null";
  output << ",\"intended_receiver\":";
  if (ball.intendedReceiver)
    output << ball.intendedReceiver->getId();
  else
    output << "null";
  output << "},\"stats\":{\"home_shots\":" << stats.homeShots
         << ",\"away_shots\":" << stats.awayShots
         << ",\"home_xg\":" << stats.homeShotXG
         << ",\"away_xg\":" << stats.awayShotXG
         << ",\"home_possession\":" << stats.homePossession
         << ",\"away_possession\":" << stats.awayPossession
         << ",\"home_substitutions\":" << homeSubstitutions
         << ",\"away_substitutions\":" << awaySubstitutions
         << ",\"home_passes_attempted\":" << stats.homePassesAttempted
         << ",\"away_passes_attempted\":" << stats.awayPassesAttempted
         << ",\"home_passes_completed\":" << stats.homePassesCompleted
         << ",\"away_passes_completed\":" << stats.awayPassesCompleted
         << ",\"home_progressive_passes\":" << stats.homeProgressivePasses
         << ",\"away_progressive_passes\":" << stats.awayProgressivePasses
         << ",\"home_through_balls\":" << stats.homeThroughBalls
         << ",\"away_through_balls\":" << stats.awayThroughBalls
         << ",\"home_crosses\":" << stats.homeCrosses
         << ",\"away_crosses\":" << stats.awayCrosses
         << ",\"home_cutbacks\":" << stats.homeCutbacks
         << ",\"away_cutbacks\":" << stats.awayCutbacks
         << ",\"home_switches_of_play\":" << stats.homeSwitchesOfPlay
         << ",\"away_switches_of_play\":" << stats.awaySwitchesOfPlay
         << ",\"home_tackles\":" << stats.homeTackles
         << ",\"away_tackles\":" << stats.awayTackles
         << ",\"home_offsides\":" << stats.homeOffsides
         << ",\"away_offsides\":" << stats.awayOffsides << "},\"players\":[";
  for (size_t index = 0; index < players.size(); ++index)
  {
    const auto& player = players[index];
    if (index > 0) output << ',';
    output << "{\"id\":" << (player.player ? player.player->getId() : 0)
           << ",\"name\":"
           << std::quoted(player.player ? player.player->getName() : "")
           << ",\"home\":" << (player.isHomeTeam ? "true" : "false")
           << ",\"x\":" << player.position.x << ",\"y\":" << player.position.y
           << ",\"target_x\":" << player.movementTarget.x
           << ",\"target_y\":" << player.movementTarget.y << ",\"intent\":\""
           << intentName(player.intent) << "\",\"stamina\":" << player.stamina
           << ",\"pressing\":" << (player.isPressing ? "true" : "false")
           << ",\"making_run\":" << (player.isMakingRun ? "true" : "false")
           << '}';
  }
  output << "]}";
  return output.str();
}

bool MatchEngine::writeDebugSnapshot(std::string_view path) const
{
  std::ofstream output{std::string(path)};
  if (!output.is_open()) return false;
  output << getDebugSnapshotJson() << '\n';
  return output.good();
}
