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

bool active(const MatchPlayer& player)
{
  return player.player != nullptr && player.onPitch;
}

/** Distance between two pitch points in metres (anisotropic pitch units). */
float metresBetween(Vector2F first, Vector2F second)
{
  const float dx = (first.x - second.x) * MatchTuning::Pitch::LENGTH_METRES;
  const float dy = (first.y - second.y) * MatchTuning::Pitch::WIDTH_METRES;
  return std::sqrt(dx * dx + dy * dy);
}

/** Whether a point lies in the penalty area defended by the given side. */
bool inPenaltyArea(Vector2F position, bool defendingHome)
{
  constexpr float HALF_WIDTH = 20.16f / MatchTuning::Pitch::WIDTH_METRES;
  if (std::abs(position.y - MatchTuning::Pitch::CENTRE) > HALF_WIDTH)
    return false;
  return defendingHome
             ? position.x <= MatchTuning::Pitch::LEFT_PENALTY_AREA_EDGE
             : position.x >= MatchTuning::Pitch::RIGHT_PENALTY_AREA_EDGE;
}

bool isDefensiveRole(PlayerRole role)
{
  return role == PlayerRole::CB || role == PlayerRole::LB ||
         role == PlayerRole::RB || role == PlayerRole::CDM;
}

bool isAttackingRole(PlayerRole role)
{
  return role == PlayerRole::ST || role == PlayerRole::LW ||
         role == PlayerRole::RW || role == PlayerRole::CAM;
}

/** Coarse role group used to pick like-for-like substitutes. */
int roleGroup(PlayerRole role)
{
  if (role == PlayerRole::GK) return 0;
  if (role == PlayerRole::CB || role == PlayerRole::LB ||
      role == PlayerRole::RB)
    return 1;
  if (isAttackingRole(role)) return 3;
  return 2;
}

/** Stable insertion sort of the first `count` entries (tiny fixed arrays). */
template <typename Array, typename Less>
void insertionSort(Array& values, std::size_t count, Less less)
{
  for (std::size_t index = 1; index < count; ++index)
  {
    auto value = values[index];
    std::size_t slot = index;
    while (slot > 0 && less(value, values[slot - 1]))
    {
      values[slot] = values[slot - 1];
      --slot;
    }
    values[slot] = value;
  }
}

std::uint64_t splitMix64(std::uint64_t value)
{
  value += 0x9e3779b97f4a7c15ULL;
  value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
  value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
  return value ^ (value >> 31U);
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
    case ScenarioAction::CLEAR:
      return "clear";
    case ScenarioAction::NONE:
      return "none";
  }
  return "unknown";
}
}  // namespace

std::string_view goalkeeperStateName(GoalkeeperState state)
{
  switch (state)
  {
    case GoalkeeperState::SET_POSITION:
      return "set_position";
    case GoalkeeperState::SWEEP:
      return "sweep";
    case GoalkeeperState::RUSH:
      return "rush";
    case GoalkeeperState::CLAIM:
      return "claim";
    case GoalkeeperState::DIVE:
      return "dive";
    case GoalkeeperState::HOLD:
      return "hold";
    case GoalkeeperState::DISTRIBUTE:
      return "distribute";
    case GoalkeeperState::RECOVER:
      return "recover";
  }
  return "unknown";
}

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
      rng(seed),
      incidentRng(static_cast<std::uint32_t>(splitMix64(seed))),
      matchSeed(seed)
{
  players.reserve(22);
  playerStats.reserve(36);
  events.reserve(256);
  initializePlayers(home_lineup, true);
  initializePlayers(away_lineup, false);
  homeBench = home_lineup.getReserves();
  awayBench = away_lineup.getReserves();
  std::erase(homeBench, nullptr);
  std::erase(awayBench, nullptr);

  std::normal_distribution<float> strictness(
      1.0f, MatchTuning::Discipline::STRICTNESS_SD);
  refereeStrictness = std::clamp(strictness(incidentRng),
                                 MatchTuning::Discipline::MIN_STRICTNESS,
                                 MatchTuning::Discipline::MAX_STRICTNESS);

  setupKickOff(true);
  updateTeamPhases();
  logEvent(MatchEventType::KICK_OFF, "Kick-off");
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
    loadAttributes(matchPlayer, player);
    matchPlayer.statsIndex = addPlayerStats(matchPlayer, true);
    destination.push_back(matchPlayer);
  };

  addPlayer(
      lineup.getGoalkeeper(),
      {MatchTuning::Pitch::LINEUP_GOALKEEPER_X, MatchTuning::Pitch::CENTRE},
      players);
  if (lineup.getGoalkeeper() && !players.empty())
    players.back().isGoalkeeper = true;
  for (const auto& positioned : lineup.getOutfieldPlayers())
  {
    addPlayer(positioned.player, positioned.position, players);
  }
}

void MatchEngine::loadAttributes(MatchPlayer& matchPlayer,
                                 const Player* player) const
{
  matchPlayer.pace = attribute(player, "Pace");
  matchPlayer.shooting = attribute(player, "Shooting");
  matchPlayer.passing = attribute(player, "Passing");
  matchPlayer.dribbling = attribute(player, "Dribbling");
  matchPlayer.defending = attribute(player, "Defending");
  matchPlayer.goalkeeping = attribute(player, "Goalkeeping");
  matchPlayer.physicality = attribute(player, "Physicality");
  matchPlayer.endurance = attribute(player, "Stamina");
  matchPlayer.vision = attribute(player, "Vision");
  matchPlayer.heightMetres =
      player ? MatchRules::playerHeightMetres(player->getHeight())
             : MatchTuning::Units::DEFAULT_PLAYER_HEIGHT_METRES;
  matchPlayer.maxSpeed =
      MatchTuning::Player::BASE_MAX_SPEED +
      matchPlayer.pace * MatchTuning::Player::PACE_SPEED_BONUS;
  matchPlayer.acceleration =
      MatchTuning::Player::BASE_ACCELERATION +
      matchPlayer.pace * MatchTuning::Player::PACE_ACCELERATION_BONUS;
}

std::size_t MatchEngine::addPlayerStats(const MatchPlayer& matchPlayer,
                                        bool started)
{
  PlayerMatchStats entry;
  entry.playerId = matchPlayer.player ? matchPlayer.player->getId() : 0;
  entry.isHomeTeam = matchPlayer.isHomeTeam;
  entry.role =
      matchPlayer.player ? matchPlayer.player->getRole() : PlayerRole::UNKNOWN;
  entry.started = started;
  entry.substitutedOn = !started;
  entry.condition = matchPlayer.stamina;
  playerStats.push_back(entry);
  return playerStats.size() - 1;
}

PlayerMatchStats& MatchEngine::statsOf(const MatchPlayer& matchPlayer)
{
  return playerStats[matchPlayer.statsIndex];
}

const PlayerMatchStats* MatchEngine::findPlayerStats(PlayerID playerId) const
{
  const auto found = std::ranges::find_if(
      playerStats, [playerId](const PlayerMatchStats& entry)
      { return entry.playerId == playerId; });
  return found == playerStats.end() ? nullptr : &*found;
}

std::optional<float> MatchEngine::getPlayerCondition(PlayerID playerId) const
{
  for (const auto& player : players)
  {
    if (player.player && player.player->getId() == playerId)
      return player.stamina;
  }
  if (const PlayerMatchStats* entry = findPlayerStats(playerId))
    return entry->condition;
  return std::nullopt;
}

bool MatchEngine::setPlayerCondition(PlayerID playerId, float condition)
{
  if (state != MatchState::KICK_OFF || elapsedMatchMinutes > 0.0f) return false;
  for (auto& player : players)
  {
    if (player.player && player.player->getId() == playerId)
    {
      player.stamina =
          std::clamp(condition, MatchTuning::Player::MINIMUM_STAMINA, 1.0f);
      statsOf(player).condition = player.stamina;
      return true;
    }
  }
  return false;
}

void MatchEngine::refreshRatings()
{
  for (auto& entry : playerStats)
  {
    const int goalDifference =
        entry.isHomeTeam ? homeScore - awayScore : awayScore - homeScore;
    const bool goalkeeper = entry.role == PlayerRole::GK;
    const bool defender = entry.role == PlayerRole::CB ||
                          entry.role == PlayerRole::LB ||
                          entry.role == PlayerRole::RB;
    entry.rating = MatchRules::computeMatchRating(entry, goalkeeper, defender,
                                                  goalDifference);
  }
}

namespace
{
/** Counter-based noise keyed by (seed, tick, salt, key), uniform in [-1, 1). */
float counterNoise(std::uint32_t seed, std::uint64_t tick, std::uint32_t salt,
                   std::uint32_t key)
{
  const std::uint64_t mixed =
      splitMix64((static_cast<std::uint64_t>(seed) << 32U) ^ tick ^
                 (static_cast<std::uint64_t>(salt) << 48U) ^
                 (static_cast<std::uint64_t>(key) << 16U));
  return static_cast<float>(mixed >> 40U) / static_cast<float>(1U << 23U) -
         1.0f;
}
}  // namespace

float MatchEngine::hashNoise(std::uint32_t salt, std::uint32_t key) const
{
  return counterNoise(matchSeed, stepCounter, salt, key);
}

float MatchEngine::epochNoise(std::uint32_t salt, std::uint32_t key,
                              std::uint32_t epochSteps) const
{
  return counterNoise(matchSeed, stepCounter / std::max(epochSteps, 1U), salt,
                      key);
}

float MatchEngine::executionErrorScale(const MatchPlayer& player) const
{
  // Tired players lose precision, and a home crowd lifts the home side a
  // little: both scale the technical error of passes and shots.
  const float fatigue = 1.0f + (1.0f - player.stamina) *
                                   MatchTuning::Fatigue::TECHNIQUE_ERROR_GAIN;
  return fatigue * (player.isHomeTeam
                        ? 1.0f - MatchTuning::Rules::HOME_EXECUTION_BONUS
                        : 1.0f);
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
  ++stepCounter;
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
    runAiSubstitutions();
    if (state == MatchState::FULL_TIME) return;
    setPieceTimer -= dt;
    if (setPieceTimer <= 0.0f)
    {
      period = 2;
      matchTimeMinutes = MatchTuning::Timing::HALF_TIME_MINUTE;
      setupKickOff(false);
      logEvent(MatchEventType::SECOND_HALF, "Second half");
    }
    return;
  }

  const float clockDelta =
      dt * MatchTuning::Timing::MATCH_MINUTES_PER_REAL_SECOND;
  if (state != MatchState::PLAYING)
  {
    runAiSubstitutions();
    if (state == MatchState::FULL_TIME) return;
    if (state != MatchState::KICK_OFF) updateRestartMovement(dt);
    if (stateClockRuns(state))
    {
      matchTimeMinutes += clockDelta;
      elapsedMatchMinutes += clockDelta;
      accumulatePlayerLoad(clockDelta);
    }
    setPieceTimer -= dt;
    if (setPieceTimer <= 0.0f) completeRestart();
  }
  else
  {
    matchTimeMinutes += clockDelta;
    elapsedMatchMinutes += clockDelta;
    stats.ballInPlayMinutes += clockDelta;
    updateMovement(dt);
    accumulatePlayerLoad(clockDelta);
    resolvePossessionAndActions(dt);

    if (!ball.possessedBy && state == MatchState::PLAYING)
    {
      updateBall(dt);
      checkOutOfBounds();
      if (state == MatchState::PLAYING) resolveLooseBall();
    }
    updatePendingAdvantage(dt);
    setPiecePhaseRemaining = std::max(0.0f, setPiecePhaseRemaining - dt);
    injuryCheckTimer += clockDelta;
    if (injuryCheckTimer >= MatchTuning::Injury::CHECK_INTERVAL_SECONDS)
    {
      injuryCheckTimer -= MatchTuning::Injury::CHECK_INTERVAL_SECONDS;
      checkInjuries();
    }
  }
  if (state == MatchState::FULL_TIME) return;

  if (stepCounter % 60U == 0U) refreshRatings();
  updateMatchClock();
}

bool MatchEngine::isInAddedTime() const
{
  const float regulationEnd = period == 1
                                  ? MatchTuning::Timing::HALF_TIME_MINUTE
                                  : MatchTuning::Timing::FULL_TIME_MINUTE;
  return state != MatchState::HALF_TIME && state != MatchState::FULL_TIME &&
         matchTimeMinutes >= regulationEnd;
}

void MatchEngine::updateMatchClock()
{
  if (state == MatchState::HALF_TIME || state == MatchState::FULL_TIME ||
      state == MatchState::GOAL)
  {
    return;
  }
  const auto half = static_cast<std::size_t>(period - 1);
  const float regulationEnd = period == 1
                                  ? MatchTuning::Timing::HALF_TIME_MINUTE
                                  : MatchTuning::Timing::FULL_TIME_MINUTE;
  if (matchTimeMinutes < regulationEnd) return;
  if (addedMinutes[half] == 0) announceAddedTime();

  // The announced added time is a minimum: stoppages during it extend it.
  const int required =
      std::max(addedMinutes[half],
               MatchRules::computeAddedMinutes(stoppageLogs[half], period));
  const float end = regulationEnd + static_cast<float>(required);
  if (matchTimeMinutes < end) return;
  const bool dangerousPhase = state == MatchState::PENALTY ||
                              state == MatchState::CORNER_KICK || ball.isShot;
  if (dangerousPhase &&
      matchTimeMinutes < end + MatchTuning::Stoppage::MAX_OVERRUN_MINUTES)
  {
    return;
  }
  endPeriod();
}

void MatchEngine::announceAddedTime()
{
  const auto half = static_cast<std::size_t>(period - 1);
  addedMinutes[half] =
      MatchRules::computeAddedMinutes(stoppageLogs[half], period);
  MatchEvent& event = logEvent(
      MatchEventType::ADDED_TIME,
      "Added time: +" + std::to_string(addedMinutes[half]) + " minutes");
  event.addedMinute = 0.0f;
}

void MatchEngine::endPeriod()
{
  restartTaker = nullptr;
  ball.possessedBy = nullptr;
  ball.velocity = {0.0f, 0.0f};
  ball.velocityZ = 0.0f;
  clearFlightState();
  pendingAdvantage.active = false;
  if (period == 1)
  {
    state = MatchState::HALF_TIME;
    setPieceTimer = MatchTuning::Timing::HALF_TIME_PAUSE_SECONDS;
    beginStoppage();
    for (auto& player : players)
    {
      if (!active(player)) continue;
      player.stamina = std::min(
          1.0f, player.stamina + MatchTuning::Fatigue::HALF_TIME_RECOVERY);
    }
    updateTeamPhases();
    logEvent(MatchEventType::HALF_TIME, "Half-time");
    return;
  }

  state = MatchState::FULL_TIME;
  for (const auto& player : players)
  {
    if (active(player))
      playerStats[player.statsIndex].condition = player.stamina;
  }
  refreshRatings();
  updateTeamPhases();
  logEvent(MatchEventType::FULL_TIME, "Full-time");
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
      if (!active(candidate) || candidate.isHomeTeam != homeTeam ||
          candidate.isGoalkeeper)
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
      if (intended && active(*intended) && !intended->isGoalkeeper &&
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
    const int lead = homeTeam ? homeScore - awayScore : awayScore - homeScore;
    const bool managingGame = lead >= MatchTuning::Decision::COMFORTABLE_LEAD;
    const std::size_t runnerCount =
        !managingGame &&
                (phase == TeamPhase::ATTACKING_TRANSITION ||
                 attackingProgress >=
                     MatchTuning::Shape::SECOND_RUNNER_PROGRESS_THRESHOLD ||
                 attackingCommitment >=
                     MatchTuning::Shape::SECOND_RUNNER_ATTACK_THRESHOLD)
            ? MatchTuning::Shape::MAX_COMMITTED_RUNNERS
            : MatchTuning::Shape::MIN_COMMITTED_RUNNERS;

    for (auto& candidate : players)
    {
      if (&candidate == carrier || !active(candidate) ||
          candidate.isHomeTeam != homeTeam || candidate.isInjured)
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
      if (!active(candidate) || candidate.isHomeTeam != homeTeam ||
          &candidate == carrier || candidate.isInjured)
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
      if (!active(candidate) || candidate.isHomeTeam != homeTeam ||
          &candidate == carrier || candidate.isGoalkeeper ||
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
      if (!active(candidate) || &candidate == carrier ||
          candidate.isHomeTeam != carrier->isHomeTeam || candidate.isGoalkeeper)
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
    if (!active(player)) continue;
    const bool goalkeeper = player.isGoalkeeper;
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
        if (opponent.isHomeTeam == player.isHomeTeam || !active(opponent))
          continue;
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
      else if (const PlayerRole role = player.player->getRole();
               (role == PlayerRole::LM || role == PlayerRole::RM ||
                role == PlayerRole::LW || role == PlayerRole::RW ||
                role == PlayerRole::LB || role == PlayerRole::RB) &&
               std::abs(player.position.y - MatchTuning::Pitch::CENTRE) >
                   MatchTuning::Shape::WIDE_LANE_DEVIATION)
      {
        // Stay in the wide lane to get to the byline and cross.
        target.y = player.position.y;
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
        // Run timing is imperfect: runners hover around the line and now and
        // then drift beyond it, which is where offsides come from.
        const float timing =
            defenderLine * attackDirection >= ball.position.x * attackDirection
                ? (epochNoise(player.player->getId(), 0x0ff51deU,
                              MatchTuning::Passing::RUN_TIMING_EPOCH_STEPS) *
                       0.5f +
                   0.25f) *
                      MatchTuning::Passing::RUN_TIMING_GAMBLE
                : 0.0f;
        const float onsideTarget =
            legalRunLine -
            attackDirection * (MatchTuning::Shape::RUN_ONSIDE_BUFFER - timing);
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

        // Only a runner caught clearly beyond the line checks back; one who
        // is marginally off relies on his timing (and sometimes gets caught).
        if (isOffside(player, player.isHomeTeam,
                      MatchTuning::Passing::RUN_TIMING_GAMBLE * 0.75f))
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
          if (opponent.isHomeTeam == player.isHomeTeam || !active(opponent) ||
              opponent.isGoalkeeper)
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
          // Meet the ball on its path instead of chasing where it is: the
          // closest point of the remaining flight line ahead of the ball.
          player.intent = PlayerIntent::RECEIVE_PASS;
          const float ballSpeed = length(ball.velocity);
          Vector2F meet{
              ball.position.x +
                  ball.velocity.x *
                      MatchTuning::Player::PASS_RECEIVER_LOOKAHEAD_SECONDS,
              ball.position.y +
                  ball.velocity.y *
                      MatchTuning::Player::PASS_RECEIVER_LOOKAHEAD_SECONDS};
          if (ballSpeed > EPSILON)
          {
            const Vector2F heading{ball.velocity.x / ballSpeed,
                                   ball.velocity.y / ballSpeed};
            const float along =
                (player.position.x - ball.position.x) * heading.x +
                (player.position.y - ball.position.y) * heading.y;
            if (along > 0.0f)
              meet = {ball.position.x + heading.x * along,
                      ball.position.y + heading.y * along};
          }
          target = {std::clamp(meet.x, MatchTuning::Pitch::PLAYER_MIN_X,
                               MatchTuning::Pitch::PLAYER_MAX_X),
                    std::clamp(meet.y, MatchTuning::Pitch::PLAYER_MIN_Y,
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

    // A lofted delivery into the box is attacked by the receiver while the
    // defenders hold their marks instead of drifting back into shape.
    if (ball.isAerialDelivery && !carrier &&
        ball.passByHome != player.isHomeTeam &&
        player.intent != PlayerIntent::CLAIM_LOOSE_BALL)
    {
      target = player.movementTarget;
    }

    if (goalkeeper)
    {
      player.intent = PlayerIntent::GOALKEEP;
      if (keepers[player.isHomeTeam ? 0 : 1].state == GoalkeeperState::DIVE)
      {
        diveGoalkeeper(player, dt);
        continue;
      }
      target = goalkeeperTarget(player, carrier);
    }

    const bool urgentTarget = player.intent == PlayerIntent::PRESS_BALL ||
                              player.intent == PlayerIntent::CLAIM_LOOSE_BALL;
    integrateMovement(player, target, dt, urgentTarget);
  }

  separatePlayers();
}

void MatchEngine::integrateMovement(MatchPlayer& player, Vector2F target,
                                    float dt, bool urgent)
{
  target.x = std::clamp(target.x, MatchTuning::Pitch::PLAYER_MIN_X,
                        MatchTuning::Pitch::PLAYER_MAX_X);
  target.y = std::clamp(target.y, MatchTuning::Pitch::KICKOFF_FORMATION_INSET,
                        1.0f - MatchTuning::Pitch::KICKOFF_FORMATION_INSET);
  const float targetResponse =
      urgent ? MatchTuning::Player::URGENT_TARGET_RESPONSE_PER_SECOND
             : MatchTuning::Player::TACTICAL_TARGET_RESPONSE_PER_SECOND;
  const float targetBlend = 1.0f - std::exp(-targetResponse * dt);
  player.movementTarget.x += (target.x - player.movementTarget.x) * targetBlend;
  player.movementTarget.y += (target.y - player.movementTarget.y) * targetBlend;
  const Vector2F targetOffset{player.movementTarget.x - player.position.x,
                              player.movementTarget.y - player.position.y};
  const float targetDistance = length(targetOffset);
  const Vector2F desiredDirection = normalized(targetOffset);
  const float arrivalSpeedScale =
      std::clamp(targetDistance / MatchTuning::Player::ARRIVAL_SLOWING_DISTANCE,
                 0.0f, 1.0f);
  const float staminaSpeed =
      MatchTuning::Player::STAMINA_SPEED_BASE +
      player.stamina * MatchTuning::Player::STAMINA_SPEED_BONUS;
  const float injuryScale =
      player.isInjured ? MatchTuning::Injury::INJURED_SPEED_SCALE : 1.0f;
  const float speed = player.maxSpeed * staminaSpeed *
                      movementSpeedScale(player.intent) * arrivalSpeedScale *
                      injuryScale;
  const Vector2F desiredVelocity{desiredDirection.x * speed,
                                 desiredDirection.y * speed};

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

  if (length(player.velocity) > MatchTuning::Player::MOVEMENT_FACING_THRESHOLD)
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
}

void MatchEngine::updateRestartMovement(float dt)
{
  // During a stoppage players walk to their restart positions (set-piece
  // slots or their last tactical target); the taker stays on the ball.
  for (auto& player : players)
  {
    if (!active(player) || &player == restartTaker) continue;
    if (player.isGoalkeeper)
    {
      keepers[player.isHomeTeam ? 0 : 1].state = GoalkeeperState::SET_POSITION;
      if (!restartIsSetPiece)
        player.movementTarget = goalkeeperTarget(player, nullptr);
    }
    player.intent = player.isGoalkeeper ? PlayerIntent::GOALKEEP
                                        : PlayerIntent::RECOVER_SHAPE;
    integrateMovement(player, player.movementTarget, dt, false);
  }
  separatePlayers();
}

void MatchEngine::separatePlayers()
{
  // Resolve simple body spacing after tactical movement. This prevents visual
  // stacking and creates natural passing lanes while keeping the simulation
  // deterministic. Players who left the pitch are static obstacles.
  for (size_t first = 0; first < players.size(); ++first)
  {
    for (size_t second = first + 1; second < players.size(); ++second)
    {
      const bool firstMoves = players[first].onPitch;
      const bool secondMoves = players[second].onPitch;
      if (!firstMoves && !secondMoves) continue;
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
      const float share = firstMoves && secondMoves
                              ? MatchTuning::Player::BODY_SEPARATION_SHARE
                              : 1.0f;
      const float correctionMetres =
          (MatchTuning::Player::MINIMUM_BODY_SEPARATION_METRES -
           separationLengthMetres) *
          share;
      const Vector2F correction{directionMetres.x * correctionMetres /
                                    MatchTuning::Pitch::LENGTH_METRES,
                                directionMetres.y * correctionMetres /
                                    MatchTuning::Pitch::WIDTH_METRES};
      const auto push = [](MatchPlayer& player, Vector2F offset)
      {
        player.position.x = std::clamp(player.position.x + offset.x,
                                       MatchTuning::Pitch::PLAYER_MIN_X,
                                       MatchTuning::Pitch::PLAYER_MAX_X);
        player.position.y = std::clamp(player.position.y + offset.y,
                                       MatchTuning::Pitch::PLAYER_MIN_Y,
                                       MatchTuning::Pitch::PLAYER_MAX_Y);
        player.velocity.x *=
            MatchTuning::Player::BODY_COLLISION_VELOCITY_RETAINED;
        player.velocity.y *=
            MatchTuning::Player::BODY_COLLISION_VELOCITY_RETAINED;
      };
      if (firstMoves) push(players[first], {-correction.x, -correction.y});
      if (secondMoves) push(players[second], correction);
    }
  }
}

void MatchEngine::accumulatePlayerLoad(float clockDelta)
{
  const float dt =
      clockDelta / MatchTuning::Timing::MATCH_MINUTES_PER_REAL_SECOND;
  for (std::size_t index = 0; index < players.size(); ++index)
  {
    MatchPlayer& player = players[index];
    if (!active(player)) continue;
    PlayerMatchStats& entry = statsOf(player);
    entry.minutesPlayed += clockDelta;

    const Vector2F previous = index < previousPlayerPositions.size()
                                  ? previousPlayerPositions[index]
                                  : player.position;
    const float stepMetres = metresBetween(previous, player.position);
    const float stepNormalised = distance(previous, player.position);
    // Restart repositioning teleports are not running.
    if (stepNormalised > player.maxSpeed * dt * 2.5f) continue;
    const float reported =
        stepMetres * MatchTuning::Units::DISTANCE_REPORT_SCALE;
    entry.distanceMetres += reported;
    if (period == 2) entry.secondHalfDistanceMetres += reported;

    const float speedRatio =
        stepNormalised / std::max(player.maxSpeed * dt, EPSILON);
    const StrategySliders& sliders =
        (player.isHomeTeam ? homeStrategy : awayStrategy).getSliders();
    const float drain = MatchRules::staminaDrainPerSecond(
        speedRatio, player.endurance,
        player.isPressing ? sliders.pressing : 0.0f);
    player.stamina = std::clamp(player.stamina - drain * dt,
                                MatchTuning::Player::MINIMUM_STAMINA, 1.0f);
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
      defender->tackleCooldown <= 0.0f && !defender->isInjured)
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
  // A booked player picks his challenges more carefully.
  const float caution = defender.yellowCards > 0
                            ? MatchTuning::Discipline::BOOKED_PLAYER_CAUTION
                            : 1.0f;
  defender.tackleCooldown =
      randomFloat(MatchTuning::Defending::MIN_TACKLE_COOLDOWN,
                  MatchTuning::Defending::MAX_TACKLE_COOLDOWN) *
      (MatchTuning::Defending::TACKLE_COOLDOWN_BASE_MULTIPLIER -
       defenderStrategy.pressing *
           MatchTuning::Defending::PRESSING_COOLDOWN_REDUCTION) /
      caution;

  if (defender.isHomeTeam)
    ++stats.homeTackleAttempts;
  else
    ++stats.awayTackleAttempts;
  ++statsOf(defender).tacklesAttempted;

  const float winChance = std::clamp(
      MatchTuning::Defending::BASE_WIN_CHANCE +
          defender.defending * MatchTuning::Defending::DEFENDING_WIN_BONUS -
          carrier.dribbling * MatchTuning::Defending::DRIBBLING_WIN_PENALTY +
          (defender.physicality - carrier.physicality) *
              MatchTuning::Defending::PHYSICALITY_DUEL_WEIGHT +
          defenderStrategy.pressing *
              MatchTuning::Defending::PRESSING_WIN_BONUS,
      MatchTuning::Defending::MIN_WIN_CHANCE,
      MatchTuning::Defending::MAX_WIN_CHANCE);
  // Defenders are far more careful inside their own penalty area.
  const float areaCaution =
      inPenaltyArea(carrier.position, defender.isHomeTeam)
          ? MatchTuning::Defending::PENALTY_AREA_FOUL_SCALE
          : 1.0f;
  const float foulPropensity =
      (MatchTuning::Defending::BASE_FOUL_CHANCE +
       defenderStrategy.riskTaking * MatchTuning::Defending::RISK_FOUL_BONUS +
       (1.0f - defender.defending) *
           MatchTuning::Defending::TECHNIQUE_FOUL_BONUS) *
      caution * areaCaution;
  if (randomFloat(0.0f, 1.0f) < winChance)
  {
    // Even a challenge that reaches the ball can be late or through the man.
    std::uniform_real_distribution<float> roll(0.0f, 1.0f);
    if (roll(incidentRng) <
        foulPropensity * MatchTuning::Defending::WINNING_TACKLE_FOUL_SHARE)
    {
      commitFoul(defender, carrier, true, false);
      return;
    }
    if (defender.isHomeTeam)
      ++stats.homeTackles;
    else
      ++stats.awayTackles;
    ++statsOf(defender).tacklesWon;
    if (randomFloat(0.0f, 1.0f) < MatchTuning::Defending::POKE_LOOSE_CHANCE)
    {
      // The challenge knocks the ball away rather than winning it cleanly.
      const float angle =
          randomFloat(-std::numbers::pi_v<float>, std::numbers::pi_v<float>);
      const float speed = randomFloat(MatchTuning::Defending::MIN_POKE_SPEED,
                                      MatchTuning::Defending::MAX_POKE_SPEED);
      ball.possessedBy = nullptr;
      ball.lastPossessor = defender.player;
      clearFlightState();
      ball.position = carrier.position;
      ball.velocity = {std::cos(angle) * speed, std::sin(angle) * speed};
      ball.velocityZ = 0.0f;
      ball.friction = MatchTuning::Passing::GROUND_FRICTION;
      ball.passCooldown = MatchTuning::Defending::DEFLECTION_COOLDOWN;
      carrier.trapTimer = MatchTuning::Ball::FAILED_TRAP_TIME;
      lastControlledTeamHome = defender.isHomeTeam;
      transitionSecondsRemaining =
          MatchTuning::Timing::POSSESSION_TRANSITION_SECONDS;
      updateTeamPhases();
      return;
    }
    setPossession(defender);
    defender.actionCooldown =
        randomFloat(MatchTuning::Defending::MIN_RECOVERY_COOLDOWN,
                    MatchTuning::Defending::MAX_RECOVERY_COOLDOWN);
    return;
  }

  std::uniform_real_distribution<float> roll(0.0f, 1.0f);
  if (roll(incidentRng) >= foulPropensity) return;
  commitFoul(defender, carrier, false, false);
}

bool MatchEngine::deniesGoalChance(const MatchPlayer& victim,
                                   const MatchPlayer& offender) const
{
  // Only the player in control of the ball can be denied an obvious chance.
  if (ball.possessedBy != victim.player) return false;
  const float goalX = victim.isHomeTeam ? 1.0f : 0.0f;
  const Vector2F goal{goalX, MatchTuning::Pitch::CENTRE};
  if (metresBetween(victim.position, goal) >
          MatchTuning::Discipline::DOGSO_MAX_DISTANCE_METRES ||
      std::abs(victim.position.y - MatchTuning::Pitch::CENTRE) >
          MatchTuning::Discipline::DOGSO_MAX_WIDTH_DEVIATION)
  {
    return false;
  }
  // Obvious chance: no covering outfield defender between victim and goal.
  const float direction = victim.isHomeTeam ? 1.0f : -1.0f;
  for (const auto& other : players)
  {
    if (!active(other) || other.isHomeTeam == victim.isHomeTeam ||
        other.isGoalkeeper || &other == &offender)
    {
      continue;
    }
    if ((other.position.x - victim.position.x) * direction > 0.0f) return false;
  }
  return true;
}

void MatchEngine::commitFoul(MatchPlayer& offender, MatchPlayer& victim,
                             bool ballWon, bool reckless)
{
  if (offender.isHomeTeam)
    ++stats.homeFouls;
  else
    ++stats.awayFouls;
  ++statsOf(offender).foulsCommitted;
  ++statsOf(victim).foulsSuffered;

  std::uniform_real_distribution<float> roll(0.0f, 1.0f);
  const Vector2F foulPosition = victim.position;
  const bool inBox = inPenaltyArea(foulPosition, offender.isHomeTeam);
  const float carrierDepth =
      victim.isHomeTeam ? victim.position.x : 1.0f - victim.position.x;
  const TeamPhase victimPhase = victim.isHomeTeam ? homePhase : awayPhase;
  const bool promisingAttack =
      carrierDepth > MatchTuning::Pitch::CENTRE &&
      (victimPhase == TeamPhase::ATTACKING_TRANSITION ||
       victimPhase == TeamPhase::FINAL_THIRD ||
       openSpaceAhead(victim) >
           MatchTuning::Discipline::ADVANTAGE_MIN_OPENNESS);

  MatchRules::FoulContext context;
  context.severityRoll =
      reckless ? std::max(roll(incidentRng), 0.9f) : roll(incidentRng);
  context.cardRoll = roll(incidentRng);
  context.strictness = refereeStrictness;
  context.tactical =
      promisingAttack &&
      (victimPhase == TeamPhase::ATTACKING_TRANSITION || !ballWon);
  context.denyingGoalChance = deniesGoalChance(victim, offender);
  context.inPenaltyArea = inBox;
  context.offenderAlreadyBooked = offender.yellowCards > 0;
  context.offenderIsAway = !offender.isHomeTeam;
  const MatchRules::FoulSanction sanction =
      MatchRules::decideFoulSanction(context);

  MatchEvent& foulEvent =
      logEvent(MatchEventType::FOUL,
               offender.player->getName() + " commits a foul", offender);
  foulEvent.secondaryPlayerId = victim.player ? victim.player->getId() : 0;
  foulEvent.position = foulPosition;

  // Advantage: the fouled side kept the ball in a promising attack outside
  // the box, so play continues unless the attack breaks down quickly.
  const bool victimKeepsBall = ball.possessedBy == victim.player;
  const bool playAdvantage =
      !ballWon && victimKeepsBall && !inBox && promisingAttack &&
      sanction != MatchRules::FoulSanction::RED &&
      roll(incidentRng) < MatchTuning::Discipline::ADVANTAGE_PLAY_CHANCE;
  const bool contactInjury =
      roll(incidentRng) <
      (context.severityRoll >= 1.0f - MatchTuning::Discipline::RECKLESS_SHARE
           ? MatchTuning::Injury::RECKLESS_CONTACT_INJURY_CHANCE
           : MatchTuning::Injury::CONTACT_INJURY_CHANCE);

  applySanction(offender, sanction);
  if (state == MatchState::FULL_TIME) return;

  if (playAdvantage && !contactInjury)
  {
    if (victim.isHomeTeam)
      ++stats.homeAdvantagesPlayed;
    else
      ++stats.awayAdvantagesPlayed;
    MatchEvent& advantage =
        logEvent(MatchEventType::ADVANTAGE, "Advantage played", victim);
    advantage.position = foulPosition;
    pendingAdvantage = {true, victim.isHomeTeam, foulPosition,
                        MatchTuning::Discipline::ADVANTAGE_WINDOW_SECONDS};
    return;
  }

  if (contactInjury && victim.onPitch) injurePlayer(victim, true);
  if (inBox)
    setupPenalty(victim.isHomeTeam);
  else
    setupFreeKick(victim.isHomeTeam, foulPosition);
}

void MatchEngine::applySanction(MatchPlayer& offender,
                                MatchRules::FoulSanction sanction)
{
  if (sanction == MatchRules::FoulSanction::NONE || !offender.player) return;
  PlayerMatchStats& entry = statsOf(offender);
  ++stoppageLogs[static_cast<std::size_t>(period - 1)].cards;
  if (sanction == MatchRules::FoulSanction::YELLOW)
  {
    ++offender.yellowCards;
    ++entry.yellowCards;
    if (offender.isHomeTeam)
      ++stats.homeYellowCards;
    else
      ++stats.awayYellowCards;
    logEvent(MatchEventType::YELLOW_CARD,
             offender.player->getName() + " is booked", offender);
    return;
  }

  if (sanction == MatchRules::FoulSanction::SECOND_YELLOW)
  {
    ++offender.yellowCards;
    ++entry.yellowCards;
    if (offender.isHomeTeam)
      ++stats.homeYellowCards;
    else
      ++stats.awayYellowCards;
    logEvent(MatchEventType::SECOND_YELLOW,
             offender.player->getName() +
                 " receives a second yellow card and is sent off",
             offender);
  }
  else
  {
    logEvent(MatchEventType::RED_CARD,
             offender.player->getName() + " is sent off", offender);
  }
  ++entry.redCards;
  entry.sentOff = true;
  // A sending-off always stops play; open the stoppage before any forced
  // goalkeeper change so it shares the restart's substitution window.
  beginStoppage();
  if (offender.isHomeTeam)
    ++stats.homeRedCards;
  else
    ++stats.awayRedCards;
  removeFromPitch(offender);
}

void MatchEngine::updatePendingAdvantage(float dt)
{
  if (!pendingAdvantage.active) return;
  const MatchPlayer* carrier = findMatchPlayer(ball.possessedBy);
  if (carrier && carrier->isHomeTeam != pendingAdvantage.fouledTeamHome)
  {
    // The advantage did not materialise: back for the original free kick.
    pendingAdvantage.active = false;
    setupFreeKick(pendingAdvantage.fouledTeamHome, pendingAdvantage.position);
    return;
  }
  pendingAdvantage.secondsRemaining -= dt;
  if (pendingAdvantage.secondsRemaining <= 0.0f)
    pendingAdvantage.active = false;
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
  // Vision (scanning and reading the play) dominates decision quality, with
  // passing technique contributing to how well options are judged.
  const float decisionQuality =
      carrier.vision * MatchTuning::Decision::VISION_DECISION_WEIGHT +
      carrier.passing * (1.0f - MatchTuning::Decision::VISION_DECISION_WEIGHT);
  const float visionNoiseScale =
      (1.0f - decisionQuality) * MatchTuning::Decision::VISION_NOISE_SCALE;

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
  // Game state: a side two or more goals up manages the game, keeping the
  // ball rather than forcing half-chances.
  const int lead = carrierScore - opponentScore;
  if (lead >= MatchTuning::Decision::COMFORTABLE_LEAD)
  {
    const float margin =
        static_cast<float>(lead - MatchTuning::Decision::COMFORTABLE_LEAD + 1);
    if (shotScore > -std::numeric_limits<float>::infinity() &&
        shotXG < MatchTuning::Decision::CLEAR_CHANCE_XG)
    {
      shotScore -=
          MatchTuning::Decision::COMFORTABLE_LEAD_SHOT_PENALTY * margin;
    }
    carryScore -=
        MatchTuning::Decision::COMFORTABLE_LEAD_CARRY_PENALTY * margin;
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
        option ? "carry:\"space ahead (value " + std::to_string(opennessAhead) +
                     ") beats the best pass with intent " +
                     std::string(passIntentName(option->intent))
               : std::string(
                     "carry:space ahead and no acceptable pass "
                     "candidate in range");
    carrier.actionCooldown =
        randomFloat(MatchTuning::Decision::MIN_DRIBBLE_TIME,
                    MatchTuning::Decision::MAX_DRIBBLE_TIME);
    return;
  }
  const float ownDepth =
      carrier.isHomeTeam ? carrier.position.x : 1.0f - carrier.position.x;
  if (shieldScore >= passScore &&
      ownDepth < MatchTuning::Defending::CLEARANCE_MAX_DEPTH)
  {
    lastScenarioDecision.action = ScenarioAction::CLEAR;
    lastScenarioDecision.reason =
        "clear:pressed in the own third with no safe outlet";
    clearBall(carrier);
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
        !active(candidate))
    {
      continue;
    }

    const float passDistance = distance(passer.position, candidate.position);
    if (passDistance < MatchTuning::Passing::MIN_DISTANCE ||
        passDistance > MatchTuning::Passing::MAX_DISTANCE)
      continue;
    // The passer judges the offside line from what he sees: poorer vision
    // misreads tight lines, which is where real offsides come from.
    const float perceivedLineError =
        hashNoise(passer.player->getId(), candidate.player->getId()) *
        (1.0f - passer.vision) * MatchTuning::Passing::OFFSIDE_PERCEPTION_ERROR;
    if (isOffside(candidate, passer.isHomeTeam, perceivedLineError)) continue;

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
  ball.z = MatchTuning::Passing::GROUND_PASS_RELEASE_HEIGHT;
  ball.possessedBy = nullptr;
  ball.lastPossessor = passer.player;
  ball.intendedReceiver = receiver.player;
  ball.isPass = true;
  ball.passByHome = passer.isHomeTeam;
  ball.passWasOffside =
      state != MatchState::KICK_OFF && state != MatchState::THROW_IN &&
      state != MatchState::GOAL_KICK && state != MatchState::CORNER_KICK &&
      isOffside(receiver, passer.isHomeTeam);
  if (!ball.passWasOffside && state == MatchState::PLAYING &&
      option.progression > 0.0f &&
      (receiver.isMakingRun || isAttackingRole(receiver.player->getRole())))
  {
    // A runner level with the line at the moment of the pass has often
    // timed his run a fraction early: the tighter, the likelier.
    const float line = offsideLine(passer.isHomeTeam);
    const float gap =
        (line - receiver.position.x) * (passer.isHomeTeam ? 1.0f : -1.0f);
    const bool opponentHalf =
        passer.isHomeTeam ? receiver.position.x > MatchTuning::Pitch::CENTRE
                          : receiver.position.x < MatchTuning::Pitch::CENTRE;
    if (opponentHalf && gap >= 0.0f &&
        gap < MatchTuning::Passing::OFFSIDE_TIMING_WINDOW)
    {
      const float chance =
          MatchTuning::Passing::OFFSIDE_TIMING_CHANCE *
          (1.0f - gap / MatchTuning::Passing::OFFSIDE_TIMING_WINDOW);
      ball.passWasOffside =
          (hashNoise(receiver.player->getId(), 0x5eedU) + 1.0f) * 0.5f < chance;
    }
  }
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
      executionErrorScale(passer) * randomFloat(-1.0f, 1.0f);
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
  const bool delivery = option.intent == PassIntent::CROSS ||
                        state == MatchState::CORNER_KICK ||
                        (state == MatchState::FREE_KICK && lofted);
  ball.isAerialDelivery = lofted && delivery;
  ball.fromThrowIn = state == MatchState::THROW_IN;
  if (lofted)
  {
    // Lofted balls are launched so they drop at the target: long passes at
    // chest height to be controlled, crosses at head height to be attacked.
    const float arrivalMetres =
        delivery
            ? MatchTuning::Aerial::ARRIVAL_HEIGHT_METRES +
                  randomFloat(-MatchTuning::Passing::DELIVERY_HEIGHT_SPREAD,
                              MatchTuning::Passing::DELIVERY_HEIGHT_SPREAD)
            : MatchTuning::Passing::LOFTED_ARRIVAL_HEIGHT_METRES;
    ball.velocityZ = verticalSpeedFor(
        distance(passer.position, target), speed, ball.friction, ball.z,
        arrivalMetres / MatchTuning::Units::BALL_Z_METRES);
    ball.curve = 0.0f;
  }
  else
  {
    ball.velocityZ = MatchTuning::Passing::GROUND_VERTICAL_SPEED;
    ball.curve = randomFloat(-MatchTuning::Passing::MAX_CURVE,
                             MatchTuning::Passing::MAX_CURVE) *
                 (MatchTuning::Passing::CURVE_SKILL_BASE + passer.passing);
  }
  if (passer.isGoalkeeper)
  {
    GoalkeeperControl& control = keepers[passer.isHomeTeam ? 0 : 1];
    control.state = GoalkeeperState::DISTRIBUTE;
    control.timer = 0.0f;
  }

  // Keep externally visible AI state consistent with the newly released
  // pass. Without this, the final fixed step of a rendered frame could expose
  // the pre-pass run assignments until the next simulation update.
  for (auto& teammate : players)
  {
    if (teammate.isHomeTeam != passer.isHomeTeam || !active(teammate)) continue;
    teammate.isMakingRun = false;
    if (&teammate == &receiver)
      teammate.intent = PlayerIntent::RECEIVE_PASS;
    else if (!teammate.isGoalkeeper)
      teammate.intent = PlayerIntent::OFFER_SUPPORT;
  }

  if (passer.isHomeTeam)
    ++stats.homePassesAttempted;
  else
    ++stats.awayPassesAttempted;
  ++statsOf(passer).passesAttempted;
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

void MatchEngine::takeShot(MatchPlayer& shooter, float forcedXG, bool header)
{
  const bool penalty = state == MatchState::PENALTY;
  const float goalX = shooter.isHomeTeam ? 1.0f : 0.0f;
  const float metres =
      metresBetween(shooter.position, {goalX, MatchTuning::Pitch::CENTRE});
  float xg = forcedXG >= 0.0f ? forcedXG : estimateShotXG(shooter);
  if (header && forcedXG < 0.0f) xg *= MatchTuning::Shooting::HEADER_XG_FACTOR;
  xg = std::clamp(xg, MatchTuning::Shooting::MIN_GOAL_PROBABILITY,
                  forcedXG >= 0.0f ? MatchTuning::Shooting::MAX_SET_PIECE_XG
                                   : MatchTuning::Shooting::MAX_OPEN_PLAY_XG);

  const MatchPlayer* keeper = findGoalkeeper(!shooter.isHomeTeam);
  const float pressure = std::clamp((MatchTuning::Shooting::PRESSURE_RADIUS -
                                     nearestOpponentDistance(shooter)) /
                                        MatchTuning::Shooting::PRESSURE_RADIUS,
                                    0.0f, 1.0f);

  // Aim for the side away from the keeper, a little inside the post; better
  // finishers pick tighter spots. The execution error then grows with
  // distance, pressure, weak technique, fatigue and headers.
  constexpr float HALF_GOAL = MatchTuning::Shooting::GOAL_HALF_WIDTH_METRES;
  const float keeperOffset =
      keeper ? (keeper->position.y - MatchTuning::Pitch::CENTRE) *
                   MatchTuning::Pitch::WIDTH_METRES
             : 0.0f;
  float side = keeperOffset > 0.0f ? -1.0f : 1.0f;
  if (std::abs(keeperOffset) < MatchTuning::Shooting::KEEPER_CENTRED_METRES ||
      randomFloat(0.0f, 1.0f) < MatchTuning::Shooting::NEAR_SIDE_AIM_CHANCE)
  {
    side = randomFloat(0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f;
  }
  // From the spot a taker can pick a corner; in open play the spot is
  // chosen on the move and lands further inside the post.
  const float inset =
      MatchTuning::Shooting::AIM_POST_INSET_BASE +
      (1.0f - shooter.shooting) * MatchTuning::Shooting::AIM_POST_INSET_SKILL +
      randomFloat(0.0f, penalty
                            ? MatchTuning::Shooting::PENALTY_AIM_INSET_SPREAD
                            : MatchTuning::Shooting::AIM_INSET_SPREAD);
  float aimY = side * std::max(0.0f, HALF_GOAL - inset);
  if (!penalty && randomFloat(0.0f, 1.0f) <
                      MatchTuning::Shooting::POWER_SHOT_BASE_CHANCE -
                          shooter.shooting *
                              MatchTuning::Shooting::POWER_SHOT_SKILL_REDUCTION)
  {
    // Struck hard at the frame rather than placed: often near the keeper.
    aimY = keeperOffset +
           randomFloat(-MatchTuning::Shooting::POWER_SHOT_WIDTH_METRES,
                       MatchTuning::Shooting::POWER_SHOT_WIDTH_METRES);
  }
  const float aimZ =
      randomFloat(MatchTuning::Shooting::AIM_MIN_HEIGHT_METRES,
                  header ? MatchTuning::Shooting::HEADER_AIM_MAX_HEIGHT_METRES
                         : MatchTuning::Shooting::AIM_MAX_HEIGHT_METRES);
  float spread =
      (MatchTuning::Shooting::ERROR_BASE_METRES +
       (1.0f - shooter.shooting) * MatchTuning::Shooting::ERROR_SKILL_METRES +
       pressure * MatchTuning::Shooting::ERROR_PRESSURE_METRES) *
      std::max(metres / MatchTuning::Shooting::ERROR_REFERENCE_METRES,
               MatchTuning::Shooting::ERROR_MIN_DISTANCE_SCALE) *
      executionErrorScale(shooter);
  if (header) spread *= MatchTuning::Aerial::HEADER_ACCURACY_PENALTY;
  if (penalty) spread *= MatchTuning::Shooting::PENALTY_ERROR_SCALE;
  std::normal_distribution<float> error(0.0f, 1.0f);
  const float crossingY = aimY + error(rng) * spread;
  const float crossingZ = std::clamp(
      aimZ + error(rng) * spread * MatchTuning::Shooting::VERTICAL_ERROR_SHARE,
      MatchTuning::Shooting::MIN_CROSSING_HEIGHT_METRES,
      MatchTuning::Shooting::MAX_CROSSING_HEIGHT_METRES);
  const bool onTarget =
      std::abs(crossingY) <
          HALF_GOAL - MatchTuning::Units::BALL_RADIUS_METRES &&
      crossingZ < MatchTuning::Units::CROSSBAR_HEIGHT_METRES -
                      MatchTuning::Units::BALL_RADIUS_METRES;
  const float targetY =
      MatchTuning::Pitch::CENTRE + crossingY / MatchTuning::Pitch::WIDTH_METRES;

  const float speed =
      (MatchTuning::Shooting::BASE_BALL_SPEED +
       shooter.shooting * MatchTuning::Shooting::SHOOTING_SPEED_BONUS +
       shooter.physicality * MatchTuning::Shooting::POWER_SPEED_BONUS) *
      randomFloat(MatchTuning::Shooting::MIN_SPEED_VARIATION, 1.0f) *
      (header ? MatchTuning::Aerial::HEADER_SPEED_SCALE : 1.0f);
  const Vector2F goalPoint{goalX, targetY};
  const Vector2F direction = normalized(
      {goalPoint.x - shooter.position.x, goalPoint.y - shooter.position.y});
  ball.position = shooter.position;
  if (!header)
  {
    ball.z = MatchTuning::Shooting::RELEASE_HEIGHT_METRES /
             MatchTuning::Units::BALL_Z_METRES;
  }
  ball.velocity = {direction.x * speed, direction.y * speed};
  ball.friction = MatchTuning::Shooting::BALL_FRICTION;
  ball.velocityZ = verticalSpeedFor(
      distance(shooter.position, goalPoint), speed, ball.friction, ball.z,
      crossingZ / MatchTuning::Units::BALL_Z_METRES);
  ball.curve = 0.0f;
  ball.possessedBy = nullptr;
  ball.lastPossessor = shooter.player;
  ball.intendedReceiver = nullptr;
  ball.passCooldown = MatchTuning::Shooting::RELEASE_COOLDOWN;
  ball.isPass = false;
  ball.passWasOffside = false;
  ball.isAerialDelivery = false;
  ball.isShot = true;
  ball.shotByHome = shooter.isHomeTeam;
  ball.shotOnTarget = onTarget;
  ball.shotXG = xg;
  ball.shotTargetY = targetY;
  ball.shotTargetHeightMetres = crossingZ;
  ball.shotElapsedSeconds = 0.0f;
  ball.shotIsHeader = header;
  ball.shotIsPenalty = penalty;
  ball.shotSaveResolved = false;
  lastShooter = shooter.player;

  PlayerMatchStats& shooterStats = statsOf(shooter);
  ++shooterStats.shots;
  shooterStats.expectedGoals += xg;
  const bool insideBox = inPenaltyArea(shooter.position, !shooter.isHomeTeam);
  const bool setPiece =
      penalty || restartIsSetPiece || setPiecePhaseRemaining > 0.0f;
  ball.shotFromSetPiece = setPiece;
  if (shooter.isHomeTeam)
  {
    ++stats.homeShots;
    stats.homeShotXG += xg;
    if (insideBox) ++stats.homeShotsInsideBox;
    if (header) ++stats.homeHeadedShots;
    if (setPiece) ++stats.homeSetPieceShots;
  }
  else
  {
    ++stats.awayShots;
    stats.awayShotXG += xg;
    if (insideBox) ++stats.awayShotsInsideBox;
    if (header) ++stats.awayHeadedShots;
    if (setPiece) ++stats.awaySetPieceShots;
  }

  // Credit the pass that created the chance.
  if (!header)
  {
    shotAssistCandidate =
        lastCompletedReceiver == shooter.player ? lastCompletedPasser : nullptr;
  }
  if (MatchPlayer* creator = findMatchPlayer(shotAssistCandidate);
      creator && creator->isHomeTeam == shooter.isHomeTeam)
  {
    ++statsOf(*creator).keyPasses;
  }

  std::ostringstream message;
  message << shooter.player->getName()
          << (header ? " heads at goal" : " shoots") << " (xG " << std::fixed
          << std::setprecision(2) << xg << ')';
  MatchEvent& event = logEvent(MatchEventType::SHOT, message.str(), shooter);
  event.xg = xg;
  event.position = shooter.position;
  shooter.actionCooldown =
      randomFloat(MatchTuning::Shooting::MIN_ACTION_COOLDOWN,
                  MatchTuning::Shooting::MAX_ACTION_COOLDOWN);
  planGoalkeeperDive(!shooter.isHomeTeam, penalty);
}

float MatchEngine::verticalSpeedFor(float horizontalDistance, float speed,
                                    float friction, float startZ,
                                    float arrivalZ) const
{
  // Mirror updateBall's discrete integration to find the step count needed
  // to cover the distance, then solve the launch speed that lands the ball at
  // the requested height at that step.
  constexpr float DT = MatchTuning::Timing::FIXED_STEP_SECONDS;
  const float drag = std::pow(
      friction, DT / MatchTuning::Timing::PHYSICS_REFERENCE_STEP_SECONDS);
  float travelled = 0.0f;
  float velocity = speed;
  int steps = 0;
  while (travelled < horizontalDistance &&
         steps < MatchTuning::Ball::MAX_FLIGHT_STEPS &&
         velocity > MatchTuning::Ball::STOP_SPEED)
  {
    travelled += velocity * DT;
    velocity *= drag;
    ++steps;
  }
  steps = std::max(steps, 1);
  const float flightTime = static_cast<float>(steps) * DT;
  const float gravityDrop = MatchTuning::Ball::GRAVITY * DT * DT *
                            static_cast<float>(steps * (steps - 1)) * 0.5f;
  return (arrivalZ - startZ + gravityDrop) / flightTime;
}

void MatchEngine::planGoalkeeperDive(bool defendingHome, bool penalty)
{
  MatchPlayer* keeper = findGoalkeeper(defendingHome);
  if (!keeper) return;
  GoalkeeperControl& control = keepers[defendingHome ? 0 : 1];
  control.state = GoalkeeperState::DIVE;
  control.timer = 0.0f;
  control.lateralVelocity = 0.0f;
  std::normal_distribution<float> error(0.0f, 1.0f);
  if (penalty)
  {
    // From 11 m the keeper must commit at the kick: he picks a side, and a
    // good keeper reads the taker a little more often.
    const float shotSide =
        ball.shotTargetY < MatchTuning::Pitch::CENTRE ? -1.0f : 1.0f;
    float guess = randomFloat(0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f;
    if (randomFloat(0.0f, 1.0f) <
        MatchTuning::Goalkeeper::PENALTY_READ_BASE +
            keeper->goalkeeping * MatchTuning::Goalkeeper::PENALTY_READ_SKILL)
    {
      guess = shotSide;
    }
    if (randomFloat(0.0f, 1.0f) < MatchTuning::Goalkeeper::PENALTY_STAY_CHANCE)
      guess = 0.0f;
    control.diveTargetY = MatchTuning::Pitch::CENTRE +
                          guess * MatchTuning::Goalkeeper::PENALTY_DIVE_METRES /
                              MatchTuning::Pitch::WIDTH_METRES;
    control.reactionRemaining = 0.0f;
    // He is already moving as the ball is struck.
    constexpr float SCALE = MatchTuning::Units::ACTION_SECONDS_PER_SIM_SECOND;
    const float maximumSpeed =
        (MatchTuning::Goalkeeper::DIVE_BASE_SPEED_METRES +
         keeper->goalkeeping *
             MatchTuning::Goalkeeper::DIVE_SKILL_SPEED_METRES) *
        SCALE / MatchTuning::Pitch::WIDTH_METRES;
    control.lateralVelocity =
        guess * maximumSpeed *
        MatchTuning::Goalkeeper::PENALTY_START_SPEED_SHARE;
    keeper->position.y += guess *
                          MatchTuning::Goalkeeper::PENALTY_PRE_MOVE_METRES /
                          MatchTuning::Pitch::WIDTH_METRES;
    return;
  }

  // Predict where the shot crosses the keeper's plane and react after a
  // skill-dependent delay; defenders between ball and goal screen the shot.
  const float goalX = defendingHome ? 0.0f : 1.0f;
  const float span = goalX - ball.position.x;
  const float t =
      std::abs(span) > EPSILON
          ? std::clamp((keeper->position.x - ball.position.x) / span, 0.0f,
                       1.0f)
          : 1.0f;
  const float predictedY =
      ball.position.y + (ball.shotTargetY - ball.position.y) * t;
  const float readError =
      error(rng) * MatchTuning::Goalkeeper::READ_ERROR_METRES *
      (1.0f - keeper->goalkeeping * 0.6f) / MatchTuning::Pitch::WIDTH_METRES;
  control.diveTargetY = std::clamp(
      predictedY + readError,
      MatchTuning::Pitch::GOAL_TOP - MatchTuning::Goalkeeper::DIVE_POST_MARGIN,
      MatchTuning::Pitch::GOAL_BOTTOM +
          MatchTuning::Goalkeeper::DIVE_POST_MARGIN);

  bool screened = false;
  for (const auto& other : players)
  {
    if (!active(other) || other.isGoalkeeper) continue;
    const float along = (other.position.x - ball.position.x) / span;
    if (along <= 0.05f || along >= 0.95f) continue;
    const float laneY =
        ball.position.y + (ball.shotTargetY - ball.position.y) * along;
    if (std::abs(other.position.y - laneY) * MatchTuning::Pitch::WIDTH_METRES <
        MatchTuning::Goalkeeper::SCREEN_WIDTH_METRES)
    {
      screened = true;
      break;
    }
  }
  const float reactionSeconds =
      MatchTuning::Goalkeeper::REACTION_BASE_SECONDS +
      (1.0f - keeper->goalkeeping) *
          MatchTuning::Goalkeeper::REACTION_SKILL_SECONDS +
      (screened ? MatchTuning::Goalkeeper::SCREENED_REACTION_SECONDS : 0.0f);
  control.reactionRemaining =
      reactionSeconds / MatchTuning::Units::ACTION_SECONDS_PER_SIM_SECOND;
}

void MatchEngine::diveGoalkeeper(MatchPlayer& keeper, float dt)
{
  GoalkeeperControl& control = keepers[keeper.isHomeTeam ? 0 : 1];
  control.timer += dt;
  if (!ball.isShot || ball.shotSaveResolved)
  {
    control.state = GoalkeeperState::RECOVER;
    control.timer = 0.0f;
    keeper.diveTimer = std::max(keeper.diveTimer,
                                MatchTuning::Goalkeeper::RECOVER_TIME_SECONDS);
    keeper.velocity = {0.0f, 0.0f};
    return;
  }
  if (control.reactionRemaining > 0.0f)
  {
    control.reactionRemaining -= dt;
    keeper.velocity = {0.0f, 0.0f};
    return;
  }
  constexpr float SCALE = MatchTuning::Units::ACTION_SECONDS_PER_SIM_SECOND;
  const float acceleration = MatchTuning::Goalkeeper::DIVE_ACCELERATION_METRES *
                             SCALE * SCALE / MatchTuning::Pitch::WIDTH_METRES;
  const float maximumSpeed =
      (MatchTuning::Goalkeeper::DIVE_BASE_SPEED_METRES +
       keeper.goalkeeping * MatchTuning::Goalkeeper::DIVE_SKILL_SPEED_METRES) *
      SCALE / MatchTuning::Pitch::WIDTH_METRES;
  const float offset = control.diveTargetY - keeper.position.y;
  const float direction = offset >= 0.0f ? 1.0f : -1.0f;
  control.lateralVelocity =
      std::clamp(control.lateralVelocity + direction * acceleration * dt,
                 -maximumSpeed, maximumSpeed);
  float step = control.lateralVelocity * dt;
  if (std::abs(step) >= std::abs(offset))
  {
    step = offset;
    control.lateralVelocity = 0.0f;
  }
  keeper.position.y =
      std::clamp(keeper.position.y + step, MatchTuning::Pitch::PLAYER_MIN_Y,
                 MatchTuning::Pitch::PLAYER_MAX_Y);
  keeper.velocity = {0.0f, step / std::max(dt, EPSILON)};
  keeper.movementTarget = keeper.position;
  keeper.isDiving = true;
}

Vector2F MatchEngine::goalkeeperTarget(MatchPlayer& keeper,
                                       const MatchPlayer* carrier)
{
  GoalkeeperControl& control = keepers[keeper.isHomeTeam ? 0 : 1];
  const auto setState = [&control](GoalkeeperState next)
  {
    if (control.state != next)
    {
      control.state = next;
      control.timer = 0.0f;
    }
  };
  control.timer += MatchTuning::Timing::FIXED_STEP_SECONDS;
  const float goalX = keeper.isHomeTeam ? 0.0f : 1.0f;
  const float outward = keeper.isHomeTeam ? 1.0f : -1.0f;
  const Vector2F goalCentre{goalX, MatchTuning::Pitch::CENTRE};
  const float ballDepth =
      keeper.isHomeTeam ? ball.position.x : 1.0f - ball.position.x;

  if (ball.possessedBy == keeper.player)
  {
    setState(GoalkeeperState::HOLD);
    return keeper.position;
  }
  if (keeper.diveTimer > 0.0f)
  {
    setState(GoalkeeperState::RECOVER);
    return keeper.position;
  }
  if (control.state == GoalkeeperState::DISTRIBUTE &&
      control.timer < MatchTuning::Goalkeeper::DISTRIBUTE_TIME_SECONDS)
  {
    return keeper.position;
  }

  // Claim a high delivery dropping into the own box.
  if (!ball.possessedBy && ball.isAerialDelivery &&
      ball.passByHome != keeper.isHomeTeam)
  {
    const float lookahead = MatchTuning::Goalkeeper::CLAIM_LOOKAHEAD_SECONDS;
    const Vector2F landing{ball.position.x + ball.velocity.x * lookahead,
                           ball.position.y + ball.velocity.y * lookahead};
    if (inPenaltyArea(landing, keeper.isHomeTeam) &&
        distance(landing, keeper.position) <
            MatchTuning::Goalkeeper::CLAIM_BOX_DEPTH)
    {
      setState(GoalkeeperState::CLAIM);
      return landing;
    }
  }

  // Sweep a loose ball or through ball he can reach before any attacker.
  if (!carrier && !ball.isShot &&
      ballDepth < MatchTuning::Pitch::GOALKEEPER_SWEEP_DEPTH &&
      !(ball.isPass && ball.passByHome == keeper.isHomeTeam))
  {
    const float keeperDistance = distance(keeper.position, ball.position);
    bool keeperFirst = true;
    for (const auto& other : players)
    {
      if (!active(other) || other.isHomeTeam == keeper.isHomeTeam) continue;
      if (distance(other.position, ball.position) <
          keeperDistance * MatchTuning::Goalkeeper::SWEEP_ADVANTAGE)
      {
        keeperFirst = false;
        break;
      }
    }
    if (keeperFirst)
    {
      setState(GoalkeeperState::SWEEP);
      return {ball.position.x +
                  ball.velocity.x *
                      MatchTuning::Player::LOOSE_BALL_LOOKAHEAD_SECONDS,
              ball.position.y +
                  ball.velocity.y *
                      MatchTuning::Player::LOOSE_BALL_LOOKAHEAD_SECONDS};
    }
  }

  // Rush a carrier who is through on goal inside the box.
  if (carrier && carrier->isHomeTeam != keeper.isHomeTeam &&
      inPenaltyArea(carrier->position, keeper.isHomeTeam))
  {
    bool covered = false;
    for (const auto& other : players)
    {
      if (!active(other) || other.isHomeTeam != keeper.isHomeTeam ||
          other.isGoalkeeper)
      {
        continue;
      }
      if ((carrier->position.x - other.position.x) * outward > 0.0f &&
          distance(other.position, carrier->position) <
              MatchTuning::Goalkeeper::RUSH_COVER_DISTANCE)
      {
        covered = true;
        break;
      }
    }
    if (!covered)
    {
      setState(GoalkeeperState::RUSH);
      return {goalCentre.x + (carrier->position.x - goalCentre.x) *
                                 MatchTuning::Goalkeeper::RUSH_CLOSING_SHARE,
              goalCentre.y + (carrier->position.y - goalCentre.y) *
                                 MatchTuning::Goalkeeper::RUSH_CLOSING_SHARE};
    }
  }

  // Set position on the ball-goal line, advancing to narrow the angle as the
  // threat approaches and acting as a sweeper when play is far away.
  setState(GoalkeeperState::SET_POSITION);
  const Vector2F threat = carrier ? carrier->position : ball.position;
  const float threatMetres = metresBetween(threat, goalCentre);
  float depthMetres = MatchTuning::Goalkeeper::NEAR_DEPTH_METRES +
                      threatMetres * MatchTuning::Goalkeeper::DEPTH_PER_METRE;
  if (threatMetres > MatchTuning::Goalkeeper::SWEEPER_DISTANCE_METRES)
  {
    depthMetres +=
        (threatMetres - MatchTuning::Goalkeeper::SWEEPER_DISTANCE_METRES) *
        MatchTuning::Goalkeeper::SWEEPER_DEPTH_PER_METRE;
  }
  depthMetres =
      std::min(depthMetres, MatchTuning::Goalkeeper::MAX_DEPTH_METRES);
  const float dxMetres =
      (threat.x - goalCentre.x) * MatchTuning::Pitch::LENGTH_METRES;
  const float dyMetres =
      (threat.y - goalCentre.y) * MatchTuning::Pitch::WIDTH_METRES;
  const float norm =
      std::max(std::sqrt(dxMetres * dxMetres + dyMetres * dyMetres), EPSILON);
  return {goalCentre.x +
              dxMetres / norm * depthMetres / MatchTuning::Pitch::LENGTH_METRES,
          std::clamp(goalCentre.y + dyMetres / norm * depthMetres /
                                        MatchTuning::Pitch::WIDTH_METRES,
                     MatchTuning::Pitch::GOALKEEPER_MIN_Y,
                     MatchTuning::Pitch::GOALKEEPER_MAX_Y)};
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
    // A delivery that has bounced is no longer a clean aerial ball.
    ball.isAerialDelivery = false;
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

  if (!ball.isShot) return;
  ball.shotElapsedSeconds += dt;
  if (ball.shotSaveResolved) return;
  MatchPlayer* keeper = findGoalkeeper(!ball.shotByHome);
  if (!keeper) return;
  // Resolve the save when the ball reaches the keeper's plane.
  const float keeperX = keeper->position.x;
  const float before = previousBallPosition.x - keeperX;
  const float after = ball.position.x - keeperX;
  if ((before > 0.0f) == (after > 0.0f) && std::abs(after) > EPSILON) return;
  resolveShotAtGoalkeeper(*keeper);
}

void MatchEngine::resolveShotAtGoalkeeper(MatchPlayer& keeper)
{
  ball.shotSaveResolved = true;
  if (!ball.shotOnTarget) return;
  const float span = ball.position.x - previousBallPosition.x;
  const float t =
      std::abs(span) > EPSILON
          ? std::clamp((keeper.position.x - previousBallPosition.x) / span,
                       0.0f, 1.0f)
          : 1.0f;
  const float crossingY =
      previousBallPosition.y + (ball.position.y - previousBallPosition.y) * t;
  const float crossingHeight = (previousBallZ + (ball.z - previousBallZ) * t) *
                               MatchTuning::Units::BALL_Z_METRES;
  const float gapMetres = std::abs(crossingY - keeper.position.y) *
                          MatchTuning::Pitch::WIDTH_METRES;

  float lateralReach =
      MatchTuning::Goalkeeper::BODY_REACH_METRES +
      (keeper.heightMetres - MatchTuning::Units::DEFAULT_PLAYER_HEIGHT_METRES) *
          MatchTuning::Goalkeeper::HEIGHT_REACH_GAIN;
  if (crossingHeight > MatchTuning::Goalkeeper::HIGH_BALL_METRES)
    lateralReach *= MatchTuning::Goalkeeper::HIGH_BALL_REACH_SCALE;
  const float verticalReach = MatchRules::goalkeeperReachMetres(
      keeper.heightMetres, keeper.goalkeeping);
  if (gapMetres > lateralReach || crossingHeight > verticalReach) return;

  const float stretch = gapMetres / std::max(lateralReach, EPSILON);
  const float realSpeed = length(ball.velocity) *
                          MatchTuning::Pitch::LENGTH_METRES /
                          MatchTuning::Units::ACTION_SECONDS_PER_SIM_SECOND;
  const float speedExcess =
      std::max(0.0f, realSpeed - MatchTuning::Goalkeeper::COMFORT_SPEED_METRES);
  const float saveChance = std::clamp(
      MatchTuning::Goalkeeper::SAVE_BASE -
          stretch * stretch * MatchTuning::Goalkeeper::SAVE_STRETCH_PENALTY -
          speedExcess * MatchTuning::Goalkeeper::SAVE_SPEED_PENALTY +
          (keeper.goalkeeping - MatchTuning::Player::DEFAULT_ATTRIBUTE) *
              MatchTuning::Goalkeeper::SAVE_SKILL_BONUS,
      MatchTuning::Goalkeeper::MIN_SAVE_CHANCE,
      MatchTuning::Goalkeeper::MAX_SAVE_CHANCE);
  if (randomFloat(0.0f, 1.0f) >= saveChance) return;

  const float holdChance =
      std::clamp(MatchTuning::Goalkeeper::HOLD_BASE +
                     keeper.goalkeeping * MatchTuning::Goalkeeper::HOLD_SKILL -
                     stretch * MatchTuning::Goalkeeper::HOLD_STRETCH_PENALTY -
                     speedExcess * MatchTuning::Goalkeeper::HOLD_SPEED_PENALTY,
                 MatchTuning::Goalkeeper::MIN_HOLD_CHANCE,
                 MatchTuning::Goalkeeper::MAX_HOLD_CHANCE);
  if (randomFloat(0.0f, 1.0f) < holdChance)
  {
    makeSave(keeper);
    return;
  }
  parryShot(keeper, crossingHeight > MatchTuning::Goalkeeper::TIP_OVER_METRES ||
                        stretch > MatchTuning::Goalkeeper::TIP_AROUND_STRETCH);
}

void MatchEngine::parryShot(MatchPlayer& keeper, bool overTheBar)
{
  MatchPlayer* shooter = findMatchPlayer(lastShooter);
  if (keeper.isHomeTeam)
  {
    ++stats.homeSaves;
    ++stats.awayOnTarget;
  }
  else
  {
    ++stats.awaySaves;
    ++stats.homeOnTarget;
  }
  ++statsOf(keeper).saves;
  if (shooter) ++statsOf(*shooter).shotsOnTarget;
  keeper.diveTimer = MatchTuning::Goalkeeper::RECOVER_TIME_SECONDS;
  keeper.isDiving = true;
  MatchEvent& event =
      logEvent(MatchEventType::SAVE,
               keeper.player->getName() +
                   (overTheBar ? " tips the shot behind" : " parries the shot"),
               keeper);
  event.secondaryPlayerId = lastShooter ? lastShooter->getId() : 0;
  ball.isShot = false;
  ball.shotOnTarget = false;
  ball.lastPossessor = keeper.player;
  if (overTheBar)
  {
    setupCorner(!keeper.isHomeTeam,
                ball.position.y < MatchTuning::Pitch::CENTRE);
    return;
  }
  // The ball spills back into the box where a rebound can be contested.
  const float outward = keeper.isHomeTeam ? 1.0f : -1.0f;
  const float speed =
      length(ball.velocity) * MatchTuning::Goalkeeper::PARRY_SPEED_SHARE;
  ball.velocity = {outward * speed * randomFloat(0.35f, 0.9f),
                   speed * randomFloat(-0.9f, 0.9f)};
  ball.velocityZ =
      randomFloat(0.0f, MatchTuning::Goalkeeper::PARRY_DEFLECTION_Z);
  ball.position.x = keeper.position.x + outward * 0.004f;
  ball.passCooldown = MatchTuning::Goalkeeper::PARRY_COOLDOWN;
}

void MatchEngine::recordPassCompletion(MatchPlayer& receiver)
{
  if (!ball.isPass) return;
  MatchPlayer* passer = findMatchPlayer(ball.lastPossessor);
  if (receiver.isHomeTeam == ball.passByHome)
  {
    if (receiver.isHomeTeam)
      ++stats.homePassesCompleted;
    else
      ++stats.awayPassesCompleted;
    if (passer && passer->isHomeTeam == receiver.isHomeTeam &&
        passer != &receiver)
    {
      ++statsOf(*passer).passesCompleted;
      lastCompletedPasser = passer->player;
      lastCompletedReceiver = receiver.player;
    }
    return;
  }
  ++statsOf(receiver).interceptions;
}

void MatchEngine::resolveLooseBall()
{
  if (ball.possessedBy || ball.passCooldown > 0.0f) return;

  const float heightMetres = ball.z * MatchTuning::Units::BALL_Z_METRES;
  MatchPlayer* closest = nullptr;
  float closestDistance = std::numeric_limits<float>::max();
  for (auto& player : players)
  {
    if (!active(player)) continue;
    const float playerDistance = distance(player.position, ball.position);
    if (playerDistance < closestDistance)
    {
      closestDistance = playerDistance;
      closest = &player;
    }
  }
  if (!closest) return;

  if (ball.isShot)
  {
    // Saves are resolved at the keeper's plane; here only a wide shot that
    // reaches the keeper or an outfield block can stop it.
    const bool defendingGoal = closest->isHomeTeam != ball.shotByHome;
    if (defendingGoal && closest->isGoalkeeper && !ball.shotOnTarget &&
        closestDistance < MatchTuning::Ball::SAVE_DISTANCE &&
        heightMetres <= MatchRules::goalkeeperReachMetres(closest->heightMetres,
                                                          closest->goalkeeping))
    {
      setPossession(*closest);
      return;
    }
    const bool reachable =
        heightMetres <= MatchRules::headerReachMetres(closest->heightMetres,
                                                      closest->physicality);
    if (defendingGoal && !closest->isGoalkeeper && reachable &&
        closestDistance < MatchTuning::Defending::BLOCK_DISTANCE &&
        randomFloat(0.0f, 1.0f) <
            MatchTuning::Defending::BASE_BLOCK_CHANCE +
                closest->defending *
                    MatchTuning::Defending::DEFENDING_BLOCK_BONUS)
    {
      ball.lastPossessor = closest->player;
      ball.isShot = false;
      ball.shotOnTarget = false;
      ball.fromThrowIn = false;
      ball.velocity.x *= MatchTuning::Defending::DEFLECTION_SPEED_FACTOR;
      ball.velocity.y +=
          randomFloat(-MatchTuning::Defending::MAX_DEFLECTION_Y_SPEED,
                      MatchTuning::Defending::MAX_DEFLECTION_Y_SPEED);
      ball.passCooldown = MatchTuning::Defending::DEFLECTION_COOLDOWN;
      const float ownDepth = closest->isHomeTeam ? closest->position.x
                                                 : 1.0f - closest->position.x;
      if (ownDepth < MatchTuning::SetPiece::BLOCK_BEHIND_DEPTH &&
          randomFloat(0.0f, 1.0f) < MatchTuning::SetPiece::BLOCK_BEHIND_CHANCE)
      {
        clearBehind(*closest);
      }
      ++statsOf(*closest).clearances;
      logEvent(MatchEventType::SHOT_BLOCKED,
               closest->player->getName() + " blocks the shot", *closest);
    }
    return;
  }

  if (heightMetres > MatchTuning::Aerial::CONTROL_CEILING_METRES)
  {
    resolveAerialContest();
    return;
  }

  // A defender meeting a cross close to his own goal line often plays safe
  // and puts it behind.
  const float closestOwnDepth =
      closest->isHomeTeam ? closest->position.x : 1.0f - closest->position.x;
  if (ball.isPass && ball.passByHome != closest->isHomeTeam &&
      !closest->isGoalkeeper &&
      (lastPassDecision.intent == PassIntent::CROSS ||
       lastPassDecision.intent == PassIntent::CUTBACK ||
       lastPassDecision.intent == PassIntent::SET_PIECE) &&
      closestOwnDepth < MatchTuning::SetPiece::CROSS_CLEARANCE_DEPTH &&
      closestDistance <= MatchTuning::Ball::OUTFIELD_CONTROL_RADIUS &&
      randomFloat(0.0f, 1.0f) < MatchTuning::SetPiece::CLEARANCE_BEHIND_CHANCE)
  {
    ++statsOf(*closest).interceptions;
    clearFlightState();
    clearBehind(*closest);
    return;
  }

  const bool handsAllowed =
      closest->isGoalkeeper &&
      inPenaltyArea(closest->position, closest->isHomeTeam);
  const float controlRadius = handsAllowed
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
    // A heavy first touch: the ball is deadened but pops a metre or two away
    // from the player instead of rolling on at full pace.
    closest->isTrapping = true;
    closest->trapTimer = MatchTuning::Ball::FAILED_TRAP_TIME;
    ball.passCooldown = MatchTuning::Ball::FAILED_TOUCH_DELAY;
    const float angle =
        randomFloat(-std::numbers::pi_v<float>, std::numbers::pi_v<float>);
    const float popSpeed =
        randomFloat(MatchTuning::Ball::MIN_HEAVY_TOUCH_SPEED,
                    MatchTuning::Ball::MAX_HEAVY_TOUCH_SPEED);
    ball.velocity = {ball.velocity.x * MatchTuning::Ball::HEAVY_TOUCH_RETAINED +
                         std::cos(angle) * popSpeed,
                     ball.velocity.y * MatchTuning::Ball::HEAVY_TOUCH_RETAINED +
                         std::sin(angle) * popSpeed};
    ball.velocityZ = 0.0f;
    ball.lastPossessor = closest->player;
    ball.friction = MatchTuning::Passing::GROUND_FRICTION;
    return;
  }

  if (ball.isPass && ball.passWasOffside &&
      ball.intendedReceiver == closest->player)
  {
    if (closest->isHomeTeam)
      ++stats.homeOffsides;
    else
      ++stats.awayOffsides;
    MatchEvent& event =
        logEvent(MatchEventType::OFFSIDE,
                 "Offside: " + closest->player->getName(), *closest);
    event.position = closest->position;
    setupFreeKick(!closest->isHomeTeam, closest->position);
    return;
  }

  recordPassCompletion(*closest);
  setPossession(*closest);
  closest->isTrapping = true;
  closest->trapTimer =
      MatchTuning::Ball::BASE_TRAP_TIME +
      (1.0f - touchSkill) * MatchTuning::Ball::TRAP_SKILL_PENALTY;
  closest->actionCooldown =
      closest->trapTimer + randomFloat(MatchTuning::Ball::MIN_POST_TOUCH_DELAY,
                                       MatchTuning::Ball::MAX_POST_TOUCH_DELAY);
}

bool MatchEngine::resolveAerialContest()
{
  const float heightMetres = ball.z * MatchTuning::Units::BALL_Z_METRES;
  // A goalkeeper in his own box claims first when he can reach the ball.
  for (auto& player : players)
  {
    if (!active(player) || !player.isGoalkeeper ||
        !inPenaltyArea(player.position, player.isHomeTeam) ||
        distance(player.position, ball.position) >
            MatchTuning::Aerial::GOALKEEPER_CLAIM_RADIUS ||
        heightMetres > MatchRules::goalkeeperReachMetres(player.heightMetres,
                                                         player.goalkeeping))
    {
      continue;
    }
    if (ball.isPass && ball.passByHome == player.isHomeTeam) continue;
    int crowding = 0;
    for (const auto& other : players)
    {
      if (active(other) && other.isHomeTeam != player.isHomeTeam &&
          distance(other.position, ball.position) <
              MatchTuning::Aerial::DUEL_RADIUS)
      {
        ++crowding;
      }
    }
    const float claimChance = std::clamp(
        MatchTuning::Aerial::BASE_CLAIM_CHANCE +
            player.goalkeeping * MatchTuning::Aerial::SKILL_CLAIM_BONUS -
            static_cast<float>(crowding) *
                MatchTuning::Aerial::CROWDING_PENALTY,
        0.05f, 0.97f);
    GoalkeeperControl& control = keepers[player.isHomeTeam ? 0 : 1];
    control.state = GoalkeeperState::CLAIM;
    control.timer = 0.0f;
    if (randomFloat(0.0f, 1.0f) < claimChance)
    {
      recordPassCompletion(player);
      setPossession(player);
      player.actionCooldown = MatchTuning::Ball::SAVE_ACTION_COOLDOWN;
      return true;
    }
    // Punched clear under pressure.
    ++statsOf(player).clearances;
    const float outward = player.isHomeTeam ? 1.0f : -1.0f;
    ball.lastPossessor = player.player;
    clearFlightState();
    ball.velocity = {outward * MatchTuning::Aerial::HEADER_CLEARANCE_SPEED *
                         randomFloat(0.5f, 0.9f),
                     randomFloat(-0.35f, 0.35f)};
    ball.velocityZ = MatchTuning::Aerial::HEADER_LIFT;
    ball.passCooldown = MatchTuning::Ball::FAILED_TOUCH_DELAY;
    return true;
  }

  // Otherwise everyone who can reach the ball contests it in the air.
  std::array<MatchPlayer*, 6> candidates{};
  std::array<float, 6> strengths{};
  std::size_t count = 0;
  float totalStrength = 0.0f;
  bool homeInvolved = false;
  bool awayInvolved = false;
  for (auto& player : players)
  {
    if (!active(player) || player.isGoalkeeper || count == candidates.size())
      continue;
    if (distance(player.position, ball.position) >
        MatchTuning::Aerial::DUEL_RADIUS)
      continue;
    const float strength = MatchRules::aerialDuelStrength(
        player.heightMetres, player.physicality, heightMetres);
    if (strength <= 0.0f) continue;
    candidates[count] = &player;
    strengths[count] = strength;
    totalStrength += strength;
    ++count;
    (player.isHomeTeam ? homeInvolved : awayInvolved) = true;
  }
  if (count == 0) return false;

  float pick = randomFloat(0.0f, totalStrength);
  std::size_t winnerIndex = count - 1;
  for (std::size_t index = 0; index < count; ++index)
  {
    pick -= strengths[index];
    if (pick <= 0.0f)
    {
      winnerIndex = index;
      break;
    }
  }
  MatchPlayer& winner = *candidates[winnerIndex];
  const bool contested = homeInvolved && awayInvolved;
  if (contested)
  {
    for (std::size_t index = 0; index < count; ++index)
    {
      MatchPlayer& candidate = *candidates[index];
      if (&candidate == &winner)
        ++statsOf(candidate).aerialDuelsWon;
      else if (candidate.isHomeTeam != winner.isHomeTeam)
        ++statsOf(candidate).aerialDuelsLost;
    }
    if (winner.isHomeTeam)
      ++stats.homeAerialDuelsWon;
    else
      ++stats.awayAerialDuelsWon;

    // Pushing or holding in the air: the defender's foul in his own box is a
    // penalty; an attacker's push gives the defence a free kick.
    std::uniform_real_distribution<float> roll(0.0f, 1.0f);
    if (roll(incidentRng) < MatchTuning::Aerial::DUEL_FOUL_CHANCE)
    {
      MatchPlayer* opponent = nullptr;
      for (std::size_t index = 0; index < count; ++index)
      {
        if (candidates[index]->isHomeTeam != winner.isHomeTeam)
        {
          opponent = candidates[index];
          break;
        }
      }
      if (opponent)
      {
        const bool attackerFouls = roll(incidentRng) < 0.6f;
        const bool winnerAttacking = ball.passByHome == winner.isHomeTeam;
        MatchPlayer& attacker = winnerAttacking ? winner : *opponent;
        MatchPlayer& defender = winnerAttacking ? *opponent : winner;
        clearFlightState();
        ball.velocity = {0.0f, 0.0f};
        ball.velocityZ = 0.0f;
        if (attackerFouls)
          commitFoul(attacker, defender, true, false);
        else
          commitFoul(defender, attacker, true, false);
        return true;
      }
    }
  }
  headBall(winner);
  return true;
}

void MatchEngine::clearBehind(MatchPlayer& defender)
{
  // Knock the ball over the own goal line, wide of the posts.
  const float goalX = defender.isHomeTeam ? 0.0f : 1.0f;
  const float side =
      defender.position.y < MatchTuning::Pitch::CENTRE ? -1.0f : 1.0f;
  const Vector2F target{
      goalX + (defender.isHomeTeam ? -0.02f : 0.02f),
      MatchTuning::Pitch::CENTRE +
          side * randomFloat(MatchTuning::Pitch::GOAL_BOTTOM -
                                 MatchTuning::Pitch::CENTRE + 0.06f,
                             0.45f)};
  const Vector2F direction = normalized(
      {target.x - defender.position.x, target.y - defender.position.y});
  const float speed = MatchTuning::Aerial::HEADER_CLEARANCE_SPEED;
  ball.lastPossessor = defender.player;
  ball.position = defender.position;
  ball.velocity = {direction.x * speed, direction.y * speed};
  ball.velocityZ = MatchTuning::Aerial::HEADER_LIFT * 0.5f;
  ball.friction = MatchTuning::Passing::LOFTED_FRICTION;
  ball.passCooldown = MatchTuning::Ball::FAILED_TOUCH_DELAY;
  lastShooter = nullptr;
}

void MatchEngine::clearBall(MatchPlayer& defender)
{
  // A long, high clearance upfield and towards the nearer touchline.
  const float forward = defender.isHomeTeam ? 1.0f : -1.0f;
  const float touchline =
      defender.position.y < MatchTuning::Pitch::CENTRE ? -1.0f : 1.0f;
  const float length =
      randomFloat(MatchTuning::Defending::CLEARANCE_MIN_DISTANCE,
                  MatchTuning::Defending::CLEARANCE_MAX_DISTANCE);
  const Vector2F direction = normalized(
      {forward, touchline * randomFloat(0.0f, 1.0f) *
                    MatchTuning::Defending::CLEARANCE_TOUCHLINE_BIAS * 2.0f});
  const Vector2F target{defender.position.x + direction.x * length,
                        defender.position.y + direction.y * length};
  const float speed =
      MatchTuning::Defending::CLEARANCE_SPEED * randomFloat(0.85f, 1.0f);
  ++statsOf(defender).clearances;
  ball.possessedBy = nullptr;
  ball.lastPossessor = defender.player;
  clearFlightState();
  ball.position = defender.position;
  ball.z = MatchTuning::Passing::GROUND_PASS_RELEASE_HEIGHT;
  ball.friction = MatchTuning::Passing::LOFTED_FRICTION;
  ball.velocity = {direction.x * speed, direction.y * speed};
  ball.velocityZ = verticalSpeedFor(distance(defender.position, target), speed,
                                    ball.friction, ball.z, 0.0f);
  ball.passCooldown = MatchTuning::Passing::PASS_RELEASE_COOLDOWN;
  lastShooter = nullptr;
  defender.actionCooldown =
      randomFloat(MatchTuning::Passing::MIN_ACTION_COOLDOWN,
                  MatchTuning::Passing::MAX_ACTION_COOLDOWN);
}

void MatchEngine::headBall(MatchPlayer& header)
{
  const float attackDirection = header.isHomeTeam ? 1.0f : -1.0f;
  const float depth =
      header.isHomeTeam ? header.position.x : 1.0f - header.position.x;
  const bool fromTeammate = ball.isPass && ball.passByHome == header.isHomeTeam;
  const MatchPlayer* deliverer = findMatchPlayer(ball.lastPossessor);
  if (fromTeammate && ball.passWasOffside &&
      ball.intendedReceiver == header.player)
  {
    ++(header.isHomeTeam ? stats.homeOffsides : stats.awayOffsides);
    MatchEvent& event =
        logEvent(MatchEventType::OFFSIDE,
                 "Offside: " + header.player->getName(), header);
    event.position = header.position;
    setupFreeKick(!header.isHomeTeam, header.position);
    return;
  }

  if (fromTeammate && depth >= MatchTuning::Aerial::HEADER_SHOT_MIN_DEPTH &&
      std::abs(header.position.y - MatchTuning::Pitch::CENTRE) <=
          MatchTuning::Aerial::HEADER_SHOT_WIDTH)
  {
    if (deliverer && deliverer->isHomeTeam == header.isHomeTeam &&
        deliverer != &header)
    {
      ++statsOf(*deliverer).passesCompleted;
      if (header.isHomeTeam)
        ++stats.homePassesCompleted;
      else
        ++stats.awayPassesCompleted;
      shotAssistCandidate = deliverer->player;
    }
    else
    {
      shotAssistCandidate = nullptr;
    }
    ball.position = header.position;
    takeShot(header, -1.0f, true);
    return;
  }

  if (fromTeammate)
    recordPassCompletion(header);
  else if (ball.isPass)
    ++statsOf(header).interceptions;
  const float ownDepth = 1.0f - depth;
  clearFlightState();
  ball.lastPossessor = header.player;
  lastShooter = nullptr;
  ball.position = header.position;
  if (!fromTeammate &&
      ownDepth > 1.0f - MatchTuning::SetPiece::CROSS_CLEARANCE_DEPTH &&
      randomFloat(0.0f, 1.0f) < MatchTuning::SetPiece::CLEARANCE_BEHIND_CHANCE)
  {
    ++statsOf(header).clearances;
    clearBehind(header);
  }
  else if (!fromTeammate && ownDepth > 0.55f)
  {
    // Defensive header: clear the danger, upfield and away from the middle.
    ++statsOf(header).clearances;
    const float wide =
        header.position.y < MatchTuning::Pitch::CENTRE ? -1.0f : 1.0f;
    const Vector2F direction =
        normalized({attackDirection * randomFloat(0.5f, 1.0f),
                    wide * randomFloat(0.1f, 0.8f)});
    const float speed =
        MatchTuning::Aerial::HEADER_CLEARANCE_SPEED * randomFloat(0.7f, 1.0f);
    ball.velocity = {direction.x * speed, direction.y * speed};
    ball.velocityZ = MatchTuning::Aerial::HEADER_LIFT;
  }
  else
  {
    // Knock-down or flick-on toward the nearest teammate.
    MatchPlayer* target = nullptr;
    float best = std::numeric_limits<float>::max();
    for (auto& other : players)
    {
      if (!active(other) || &other == &header ||
          other.isHomeTeam != header.isHomeTeam || other.isGoalkeeper)
        continue;
      const float d = distance(other.position, header.position);
      if (d < best)
      {
        best = d;
        target = &other;
      }
    }
    const Vector2F aim =
        target ? target->position
               : Vector2F{header.position.x + attackDirection * 0.1f,
                          header.position.y};
    const Vector2F direction =
        normalized({aim.x - header.position.x + randomFloat(-0.03f, 0.03f),
                    aim.y - header.position.y + randomFloat(-0.03f, 0.03f)});
    const float speed = MatchTuning::Aerial::HEADER_PASS_SPEED;
    ball.velocity = {direction.x * speed, direction.y * speed};
    ball.velocityZ = MatchTuning::Aerial::HEADER_LIFT * 0.5f;
  }
  ball.friction = MatchTuning::Passing::LOFTED_FRICTION;
  ball.passCooldown = MatchTuning::Ball::FAILED_TOUCH_DELAY;
}

void MatchEngine::setPossession(MatchPlayer& player)
{
  if (state == MatchState::PLAYING && lastControlledTeamHome &&
      *lastControlledTeamHome != player.isHomeTeam)
  {
    transitionSecondsRemaining =
        MatchTuning::Timing::POSSESSION_TRANSITION_SECONDS;
    lastCompletedPasser = nullptr;
    lastCompletedReceiver = nullptr;
  }
  lastControlledTeamHome = player.isHomeTeam;
  lastShooter = nullptr;
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
  ball.shotXG = 0.0f;
  ball.shotIsHeader = false;
  ball.shotIsPenalty = false;
  ball.shotFromSetPiece = false;
  ball.shotSaveResolved = false;
  ball.shotElapsedSeconds = 0.0f;
  ball.isAerialDelivery = false;
  ball.fromThrowIn = false;
  ball.curve = 0.0f;
}

void MatchEngine::checkOutOfBounds()
{
  if (ball.position.x <= 0.0f || ball.position.x >= 1.0f)
  {
    const bool rightGoalLine = ball.position.x >= 1.0f;
    const float lineX = rightGoalLine ? 1.0f : 0.0f;
    const float span = ball.position.x - previousBallPosition.x;
    const float t =
        std::abs(span) > EPSILON
            ? std::clamp((lineX - previousBallPosition.x) / span, 0.0f, 1.0f)
            : 1.0f;
    const float crossingY =
        previousBallPosition.y + (ball.position.y - previousBallPosition.y) * t;
    const float crossingHeight =
        (previousBallZ + (ball.z - previousBallZ) * t) *
        MatchTuning::Units::BALL_Z_METRES;
    const float lateralMetres =
        std::abs(crossingY - MatchTuning::Pitch::CENTRE) *
        MatchTuning::Pitch::WIDTH_METRES;
    constexpr float RADIUS = MatchTuning::Units::BALL_RADIUS_METRES;
    constexpr float HALF_GOAL = MatchTuning::Shooting::GOAL_HALF_WIDTH_METRES;
    constexpr float BAR = MatchTuning::Units::CROSSBAR_HEIGHT_METRES;
    if (lateralMetres < HALF_GOAL - RADIUS && crossingHeight < BAR - RADIUS &&
        !ball.fromThrowIn)
    {
      // Any ball that crosses the line inside the frame is a goal.
      ball.position.y = crossingY;
      scoreGoal(rightGoalLine);
      return;
    }
    const bool hitsPost =
        std::abs(lateralMetres - HALF_GOAL) <
            RADIUS + MatchTuning::Shooting::WOODWORK_BAND_METRES &&
        crossingHeight < BAR + RADIUS;
    const bool hitsBar =
        lateralMetres < HALF_GOAL + RADIUS &&
        std::abs(crossingHeight - BAR) <
            RADIUS + MatchTuning::Shooting::WOODWORK_BAND_METRES;
    if (ball.isShot && (hitsPost || hitsBar))
    {
      MatchPlayer* shooter = findMatchPlayer(lastShooter);
      MatchEvent& event = logEvent(
          MatchEventType::WOODWORK,
          (shooter ? shooter->player->getName() : std::string("The shot")) +
              " hits the woodwork");
      if (shooter)
      {
        event.hasTeam = true;
        event.isHomeTeam = shooter->isHomeTeam;
        event.primaryPlayerId = shooter->player->getId();
      }
      ball.isShot = false;
      ball.shotOnTarget = false;
      ball.position.x = rightGoalLine ? 1.0f - 0.004f : 0.004f;
      ball.position.y = crossingY;
      ball.velocity = {
          -ball.velocity.x * MatchTuning::Shooting::WOODWORK_REBOUND,
          ball.velocity.y + randomFloat(-0.2f, 0.2f)};
      ball.velocityZ = std::abs(ball.velocityZ) * 0.3f;
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
  MatchPlayer* last = findMatchPlayer(ball.lastPossessor);
  MatchPlayer* shooter = findMatchPlayer(lastShooter);
  MatchPlayer* scorer = last;
  // A shot parried or deflected in stays the shooter's goal; only a
  // defender's own deliberate touch makes it an own goal.
  if (shooter && shooter->isHomeTeam == homeTeam &&
      (!last || last->isHomeTeam != homeTeam))
  {
    scorer = shooter;
  }
  const bool ownGoal = scorer && scorer->isHomeTeam != homeTeam;

  if (homeTeam)
    ++homeScore;
  else
    ++awayScore;
  ++stoppageLogs[static_cast<std::size_t>(period - 1)].goals;

  if (ball.isShot && scorer && !ownGoal)
  {
    if (ball.shotIsHeader)
      ++(homeTeam ? stats.homeHeadedGoals : stats.awayHeadedGoals);
    if (ball.shotFromSetPiece)
      ++(homeTeam ? stats.homeSetPieceGoals : stats.awaySetPieceGoals);
    if (ball.shotIsPenalty)
      ++(homeTeam ? stats.homePenaltyGoals : stats.awayPenaltyGoals);
  }
  if (scorer && !ownGoal)
  {
    // Every goal is an attempt on target; a knock-down or a ball bundled in
    // without a recorded shot is logged as one here.
    PlayerMatchStats& entry = statsOf(*scorer);
    if (scorer != shooter)
    {
      ++entry.shots;
      ++(homeTeam ? stats.homeShots : stats.awayShots);
    }
    ++entry.shotsOnTarget;
    ++(homeTeam ? stats.homeOnTarget : stats.awayOnTarget);
  }
  PlayerID assisterId = 0;
  if (scorer && !ownGoal)
  {
    ++statsOf(*scorer).goals;
    if (MatchPlayer* assister = findMatchPlayer(shotAssistCandidate);
        assister && assister != scorer && assister->isHomeTeam == homeTeam)
    {
      ++statsOf(*assister).assists;
      assisterId = assister->player->getId();
    }
  }
  else if (scorer)
  {
    ++statsOf(*scorer).ownGoals;
  }
  for (const auto& player : players)
  {
    if (active(player) && player.isHomeTeam != homeTeam)
      ++playerStats[player.statsIndex].goalsConceded;
  }

  const std::string scorerName =
      scorer ? scorer->player->getName() : std::string("Unknown player");
  const std::string score =
      " (" + std::to_string(homeScore) + '-' + std::to_string(awayScore) + ')';
  MatchEvent& event =
      logEvent(ownGoal ? MatchEventType::OWN_GOAL : MatchEventType::GOAL,
               "GOAL! " + scorerName + (ownGoal ? " (own goal)" : "") + score);
  // The team fields describe the primary player's side, so an own goal is
  // reported for the defender's team (it counts for the other side).
  event.hasTeam = true;
  event.isHomeTeam = scorer ? scorer->isHomeTeam : homeTeam;
  event.primaryPlayerId = scorer ? scorer->player->getId() : 0;
  event.secondaryPlayerId = assisterId;
  event.position = scorer ? scorer->position : ball.position;
  event.xg = ball.isShot ? ball.shotXG : 0.0f;

  goalScoredByHome = homeTeam;
  goalCelebrationRemaining = MatchTuning::Timing::GOAL_CELEBRATION_SECONDS;
  state = MatchState::GOAL;
  ball.possessedBy = nullptr;
  ball.shotByHome = homeTeam;
  pendingAdvantage.active = false;
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
  MatchPlayer* shooter = findMatchPlayer(lastShooter);
  if (goalkeeper.isHomeTeam)
  {
    ++stats.homeSaves;
    ++stats.awayOnTarget;
  }
  else
  {
    ++stats.awaySaves;
    ++stats.homeOnTarget;
  }
  ++statsOf(goalkeeper).saves;
  if (shooter) ++statsOf(*shooter).shotsOnTarget;
  const PlayerID shooterId = lastShooter ? lastShooter->getId() : 0;
  goalkeeper.isDiving = true;
  goalkeeper.diveTimer = MatchTuning::Ball::SAVE_DIVE_TIME;
  setPossession(goalkeeper);
  goalkeeper.actionCooldown = MatchTuning::Ball::SAVE_ACTION_COOLDOWN;
  state = MatchState::PLAYING;
  keepers[goalkeeper.isHomeTeam ? 0 : 1].state = GoalkeeperState::HOLD;
  updateTeamPhases();
  MatchEvent& event =
      logEvent(MatchEventType::SAVE,
               goalkeeper.player->getName() + " makes a save", goalkeeper);
  event.secondaryPlayerId = shooterId;
}

void MatchEngine::beginStoppage()
{
  // Play has stopped: a pending advantage lapses. Several stoppage causes in
  // the same step (a red card and its free kick) form one stoppage, so they
  // share one substitution window.
  pendingAdvantage.active = false;
  if (stoppageSequence != 0 && lastStoppageStep == stepCounter) return;
  lastStoppageStep = stepCounter;
  ++stoppageSequence;
}

void MatchEngine::setupKickOff(bool homeKickingOff)
{
  beginStoppage();
  restartIsSetPiece = false;
  pendingAdvantage.active = false;
  resetPositions();
  // At kick-off every player must be in their own half. The normal tactical
  // positions deliberately span more of the pitch and are restored through
  // regular movement once play starts.
  for (auto& player : players)
  {
    if (!active(player)) continue;
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
  lastShooter = nullptr;
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
  beginStoppage();
  restartIsSetPiece = false;
  ball.position.x = std::clamp(
      ball.position.x, MatchTuning::Pitch::RESTART_LONGITUDINAL_MARGIN,
      1.0f - MatchTuning::Pitch::RESTART_LONGITUDINAL_MARGIN);
  ball.position.y = ball.position.y < MatchTuning::Pitch::CENTRE
                        ? MatchTuning::Pitch::RESTART_INSET
                        : 1.0f - MatchTuning::Pitch::RESTART_INSET;
  ball.z = 0.0f;
  ball.velocityZ = 0.0f;
  restartTaker = findClosestPlayer(ball.position, homeTeam, false);
  ball.velocity = {0.0f, 0.0f};
  ball.possessedBy = restartTaker ? restartTaker->player : nullptr;
  ball.lastPossessor = ball.possessedBy;
  clearFlightState();
  state = MatchState::THROW_IN;
  setPieceTimer = MatchTuning::Timing::THROW_IN_DELAY_SECONDS;
  updateTeamPhases();
  MatchEvent& event =
      logEvent(MatchEventType::THROW_IN,
               homeTeam ? "Throw-in to home" : "Throw-in to away");
  event.hasTeam = true;
  event.isHomeTeam = homeTeam;
  event.position = ball.position;
}

void MatchEngine::setupGoalKick(bool homeTeam)
{
  beginStoppage();
  restartIsSetPiece = false;
  ball.position = {homeTeam ? MatchTuning::Pitch::LEFT_GOAL_KICK_X
                            : MatchTuning::Pitch::RIGHT_GOAL_KICK_X,
                   MatchTuning::Pitch::CENTRE};
  ball.z = 0.0f;
  ball.velocityZ = 0.0f;
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
  MatchEvent& event =
      logEvent(MatchEventType::GOAL_KICK,
               homeTeam ? "Goal kick to home" : "Goal kick to away");
  event.hasTeam = true;
  event.isHomeTeam = homeTeam;
  event.position = ball.position;
}

void MatchEngine::setupCorner(bool homeTeam, bool topCorner)
{
  beginStoppage();
  restartIsSetPiece = true;
  ball.position = {homeTeam ? 1.0f - MatchTuning::Pitch::RESTART_INSET
                            : MatchTuning::Pitch::RESTART_INSET,
                   topCorner ? MatchTuning::Pitch::RESTART_INSET
                             : 1.0f - MatchTuning::Pitch::RESTART_INSET};
  ball.z = 0.0f;
  ball.velocityZ = 0.0f;
  restartTaker = bestSetPieceTaker(homeTeam, false);
  if (!restartTaker)
    restartTaker = findClosestPlayer(ball.position, homeTeam, false);
  if (restartTaker) placeTaker(*restartTaker, ball.position);
  ball.velocity = {0.0f, 0.0f};
  ball.possessedBy = restartTaker ? restartTaker->player : nullptr;
  ball.lastPossessor = ball.possessedBy;
  clearFlightState();
  arrangeSetPiece(homeTeam, ball.position);
  state = MatchState::CORNER_KICK;
  setPieceTimer = MatchTuning::Timing::CORNER_DELAY_SECONDS;
  updateTeamPhases();
  if (homeTeam)
    ++stats.homeCorners;
  else
    ++stats.awayCorners;
  MatchEvent& event = logEvent(MatchEventType::CORNER,
                               homeTeam ? "Corner to home" : "Corner to away");
  event.hasTeam = true;
  event.isHomeTeam = homeTeam;
  event.position = ball.position;
}

void MatchEngine::setupFreeKick(bool homeTeam, Vector2F foulPos)
{
  beginStoppage();
  pendingAdvantage.active = false;
  ball.position = {
      std::clamp(foulPos.x, MatchTuning::Pitch::RESTART_LONGITUDINAL_MARGIN,
                 1.0f - MatchTuning::Pitch::RESTART_LONGITUDINAL_MARGIN),
      std::clamp(foulPos.y, MatchTuning::Pitch::RESTART_LONGITUDINAL_MARGIN,
                 1.0f - MatchTuning::Pitch::RESTART_LONGITUDINAL_MARGIN)};
  ball.z = 0.0f;
  ball.velocityZ = 0.0f;
  const Vector2F goal{homeTeam ? 1.0f : 0.0f, MatchTuning::Pitch::CENTRE};
  const float goalMetres = metresBetween(ball.position, goal);
  // Free kicks in the attacking third are set pieces taken by the specialist.
  restartIsSetPiece =
      goalMetres <= MatchTuning::SetPiece::CROSSING_FREE_KICK_METRES;
  restartTaker =
      restartIsSetPiece ? bestSetPieceTaker(homeTeam, true) : nullptr;
  if (!restartTaker)
    restartTaker = findClosestPlayer(ball.position, homeTeam, false);
  if (restartTaker && restartIsSetPiece)
    placeTaker(*restartTaker, ball.position);
  ball.velocity = {0.0f, 0.0f};
  ball.possessedBy = restartTaker ? restartTaker->player : nullptr;
  ball.lastPossessor = ball.possessedBy;
  clearFlightState();
  if (restartIsSetPiece) arrangeSetPiece(homeTeam, ball.position);
  state = MatchState::FREE_KICK;
  setPieceTimer = MatchTuning::Timing::FREE_KICK_DELAY_SECONDS;
  updateTeamPhases();
  MatchEvent& event =
      logEvent(MatchEventType::FREE_KICK,
               homeTeam ? "Free kick to home" : "Free kick to away");
  event.hasTeam = true;
  event.isHomeTeam = homeTeam;
  event.position = ball.position;
}

void MatchEngine::setupPenalty(bool homeTeam)
{
  beginStoppage();
  restartIsSetPiece = true;
  pendingAdvantage.active = false;
  const Vector2F spot{homeTeam ? MatchTuning::Pitch::RIGHT_PENALTY_SPOT_X
                               : MatchTuning::Pitch::LEFT_PENALTY_SPOT_X,
                      MatchTuning::Pitch::CENTRE};
  restartTaker = bestSetPieceTaker(homeTeam, true);
  ball.position = spot;
  ball.z = 0.0f;
  ball.velocityZ = 0.0f;
  ball.velocity = {0.0f, 0.0f};
  ball.possessedBy = restartTaker ? restartTaker->player : nullptr;
  ball.lastPossessor = ball.possessedBy;
  if (restartTaker) placeTaker(*restartTaker, spot);
  // Everyone else waits outside the area; the keeper stands on his line.
  const float edge = homeTeam ? MatchTuning::Pitch::RIGHT_PENALTY_AREA_EDGE -
                                    MatchTuning::SetPiece::PENALTY_WAIT_OFFSET
                              : MatchTuning::Pitch::LEFT_PENALTY_AREA_EDGE +
                                    MatchTuning::SetPiece::PENALTY_WAIT_OFFSET;
  for (auto& player : players)
  {
    if (!active(player) || &player == restartTaker) continue;
    if (player.isGoalkeeper && player.isHomeTeam != homeTeam)
    {
      // Law 14: the defending keeper stays on his goal line.
      player.movementTarget = {homeTeam ? MatchTuning::Pitch::PLAYER_MAX_X
                                        : MatchTuning::Pitch::PLAYER_MIN_X,
                               MatchTuning::Pitch::CENTRE};
      player.position = player.movementTarget;
      continue;
    }
    if (player.isGoalkeeper) continue;
    const bool inside =
        homeTeam ? player.position.x > edge : player.position.x < edge;
    if (inside) player.movementTarget.x = edge;
  }
  clearFlightState();
  if (homeTeam)
    ++stats.homePenalties;
  else
    ++stats.awayPenalties;
  ++stoppageLogs[static_cast<std::size_t>(period - 1)].penalties;
  state = MatchState::PENALTY;
  setPieceTimer = MatchTuning::Timing::PENALTY_DELAY_SECONDS;
  updateTeamPhases();
  MatchEvent& event =
      logEvent(MatchEventType::PENALTY,
               homeTeam ? "Penalty to home" : "Penalty to away");
  event.hasTeam = true;
  event.isHomeTeam = homeTeam;
  event.position = spot;
  if (restartTaker && restartTaker->player)
    event.primaryPlayerId = restartTaker->player->getId();
}

void MatchEngine::placeTaker(MatchPlayer& taker, Vector2F spot)
{
  taker.position = {std::clamp(spot.x, MatchTuning::Pitch::PLAYER_MIN_X,
                               MatchTuning::Pitch::PLAYER_MAX_X),
                    std::clamp(spot.y, MatchTuning::Pitch::PLAYER_MIN_Y,
                               MatchTuning::Pitch::PLAYER_MAX_Y)};
  taker.movementTarget = taker.position;
  taker.velocity = {0.0f, 0.0f};
  // Anyone standing on the spot steps aside for the taker.
  for (auto& other : players)
  {
    if (!active(other) || &other == &taker) continue;
    const float dx = (other.position.x - taker.position.x) *
                     MatchTuning::Pitch::LENGTH_METRES;
    const float dy = (other.position.y - taker.position.y) *
                     MatchTuning::Pitch::WIDTH_METRES;
    const float gap = std::sqrt(dx * dx + dy * dy);
    if (gap >= MatchTuning::Player::MINIMUM_BODY_SEPARATION_METRES) continue;
    const float towardCentre =
        taker.position.y < MatchTuning::Pitch::CENTRE ? 1.0f : -1.0f;
    const Vector2F away = gap > EPSILON ? Vector2F{dx / gap, dy / gap}
                                        : Vector2F{0.0f, towardCentre};
    const float push =
        MatchTuning::Player::MINIMUM_BODY_SEPARATION_METRES - gap;
    other.position.x = std::clamp(
        other.position.x + away.x * push / MatchTuning::Pitch::LENGTH_METRES,
        MatchTuning::Pitch::PLAYER_MIN_X, MatchTuning::Pitch::PLAYER_MAX_X);
    other.position.y = std::clamp(
        other.position.y + away.y * push / MatchTuning::Pitch::WIDTH_METRES,
        MatchTuning::Pitch::PLAYER_MIN_Y, MatchTuning::Pitch::PLAYER_MAX_Y);
  }
}

MatchPlayer* MatchEngine::bestSetPieceTaker(bool homeTeam, bool shooting)
{
  MatchPlayer* best = nullptr;
  float bestScore = -std::numeric_limits<float>::infinity();
  for (auto& player : players)
  {
    if (!active(player) || player.isHomeTeam != homeTeam ||
        player.isGoalkeeper || player.isInjured)
      continue;
    const float score = shooting
                            ? player.shooting * 0.75f + player.passing * 0.15f +
                                  player.vision * 0.10f
                            : player.passing * 0.6f + player.vision * 0.4f;
    if (score > bestScore)
    {
      bestScore = score;
      best = &player;
    }
  }
  return best;
}

void MatchEngine::arrangeSetPiece(bool attackingHome, Vector2F ballPosition)
{
  // Box targets for the attacking side, in metres from the goal line and
  // from the centre of the goal (positive towards the ball's side).
  const float goalX = attackingHome ? 1.0f : 0.0f;
  const float inward = attackingHome ? -1.0f : 1.0f;
  const float ballSide =
      ballPosition.y < MatchTuning::Pitch::CENTRE ? -1.0f : 1.0f;
  const auto spot = [&](float depthMetres, float lateralMetres)
  {
    return Vector2F{
        goalX + inward * depthMetres / MatchTuning::Pitch::LENGTH_METRES,
        MatchTuning::Pitch::CENTRE +
            ballSide * lateralMetres / MatchTuning::Pitch::WIDTH_METRES};
  };
  const std::array<Vector2F, 5> attackSlots = {
      spot(5.5f, 3.0f), spot(6.5f, -3.5f), spot(10.5f, 0.0f), spot(8.0f, 1.0f),
      spot(17.5f, -2.0f)};

  // Rank attackers by aerial threat; the best go into the box.
  std::array<MatchPlayer*, 11> attackers{};
  std::size_t attackerCount = 0;
  for (auto& player : players)
  {
    if (!active(player) || player.isHomeTeam != attackingHome ||
        player.isGoalkeeper || &player == restartTaker ||
        attackerCount == attackers.size())
      continue;
    attackers[attackerCount++] = &player;
  }
  insertionSort(attackers, attackerCount,
                [](const MatchPlayer* first, const MatchPlayer* second)
                {
                  const float a = MatchRules::headerReachMetres(
                      first->heightMetres, first->physicality);
                  const float b = MatchRules::headerReachMetres(
                      second->heightMetres, second->physicality);
                  if (a != b) return a > b;
                  return first->player->getId() < second->player->getId();
                });
  const std::size_t boxAttackers =
      std::min(attackSlots.size(),
               attackerCount > MatchTuning::SetPiece::REST_DEFENDERS
                   ? attackerCount - MatchTuning::SetPiece::REST_DEFENDERS
                   : std::size_t{0});
  for (std::size_t index = 0; index < boxAttackers; ++index)
    attackers[index]->movementTarget = attackSlots[index];

  // Defenders: the keeper on his line, markers goal-side of each attacker in
  // the box, and the rest zonal at the near post and the edge of the area.
  std::array<MatchPlayer*, 11> defenders{};
  std::size_t defenderCount = 0;
  for (auto& player : players)
  {
    if (!active(player) || player.isHomeTeam == attackingHome ||
        defenderCount == defenders.size())
      continue;
    if (player.isGoalkeeper)
    {
      player.movementTarget = spot(0.8f, 0.0f);
      continue;
    }
    defenders[defenderCount++] = &player;
  }
  insertionSort(defenders, defenderCount,
                [](const MatchPlayer* first, const MatchPlayer* second)
                {
                  const float a = first->defending + first->physicality;
                  const float b = second->defending + second->physicality;
                  if (a != b) return a > b;
                  return first->player->getId() < second->player->getId();
                });
  std::size_t next = 0;
  for (std::size_t index = 0; index < boxAttackers && next < defenderCount;
       ++index, ++next)
  {
    const Vector2F mark = attackers[index]->movementTarget;
    defenders[next]->movementTarget = {
        mark.x - inward * MatchTuning::SetPiece::MARKING_GOAL_SIDE_OFFSET,
        mark.y};
  }
  const std::array<Vector2F, 3> zones = {spot(3.0f, 2.5f), spot(6.0f, 0.0f),
                                         spot(16.0f, 0.0f)};
  for (std::size_t zone = 0; zone < zones.size() && next < defenderCount;
       ++zone, ++next)
  {
    defenders[next]->movementTarget = zones[zone];
  }
}

void MatchEngine::completeRestart()
{
  const MatchState restartState = state;
  MatchPlayer* taker = restartTaker;
  restartTaker = nullptr;
  if (!taker || !active(*taker))
  {
    state = MatchState::PLAYING;
    updateTeamPhases();
    return;
  }

  if (restartState == MatchState::PENALTY)
  {
    ball.position = taker->position;
    takeShot(*taker, MatchTuning::Shooting::PENALTY_XG);
    state = MatchState::PLAYING;
    restartIsSetPiece = false;
    updateTeamPhases();
    return;
  }

  if (restartIsSetPiece)
    setPiecePhaseRemaining = MatchTuning::SetPiece::SET_PIECE_PHASE_SECONDS;
  takeSetPiece(*taker, restartState);
  if (state == restartState) state = MatchState::PLAYING;
  restartIsSetPiece = false;
  updateTeamPhases();
}

void MatchEngine::takeSetPiece(MatchPlayer& taker, MatchState restartState)
{
  if (restartState == MatchState::CORNER_KICK)
  {
    takeCorner(taker);
    return;
  }
  if (restartState == MatchState::FREE_KICK && restartIsSetPiece)
  {
    const Vector2F goal{taker.isHomeTeam ? 1.0f : 0.0f,
                        MatchTuning::Pitch::CENTRE};
    const float goalMetres = metresBetween(ball.position, goal);
    const float lateralMetres =
        std::abs(ball.position.y - MatchTuning::Pitch::CENTRE) *
        MatchTuning::Pitch::WIDTH_METRES;
    const bool shootingRange =
        goalMetres <= MatchTuning::SetPiece::DIRECT_FREE_KICK_METRES &&
        lateralMetres <= MatchTuning::SetPiece::DIRECT_FREE_KICK_WIDTH_METRES;
    const float shootChance = std::clamp(
        MatchTuning::SetPiece::DIRECT_SHOT_BASE +
            taker.shooting * MatchTuning::SetPiece::DIRECT_SHOT_SKILL -
            goalMetres * MatchTuning::SetPiece::DIRECT_SHOT_DISTANCE_PENALTY,
        0.0f, 0.95f);
    if (shootingRange && randomFloat(0.0f, 1.0f) < shootChance)
    {
      takeDirectFreeKick(taker);
      return;
    }
    const bool wide =
        lateralMetres > MatchTuning::SetPiece::CROSS_MIN_WIDTH_METRES ||
        goalMetres > MatchTuning::SetPiece::DIRECT_FREE_KICK_METRES;
    if (wide)
    {
      takeCorner(taker);
      return;
    }
  }

  std::optional<PassOption> passOption = choosePassTarget(taker);
  if (passOption)
    passBall(taker, *passOption, restartState == MatchState::GOAL_KICK);
  else
    setPossession(taker);
}

void MatchEngine::takeCorner(MatchPlayer& taker)
{
  // Routine: short, near post, far post or penalty spot, delivered at head
  // height to the attacker assigned to that zone.
  MatchPlayer* target = nullptr;
  const float roll = randomFloat(0.0f, 1.0f);
  if (roll < MatchTuning::SetPiece::SHORT_CORNER_CHANCE)
  {
    std::optional<PassOption> shortOption = choosePassTarget(taker);
    if (shortOption && shortOption->passDistance <
                           MatchTuning::SetPiece::SHORT_CORNER_MAX_DISTANCE)
    {
      PassOption groundPass = *shortOption;
      groundPass.lofted = false;
      passBall(taker, groundPass);
      return;
    }
  }
  const Vector2F goal{taker.isHomeTeam ? 1.0f : 0.0f,
                      MatchTuning::Pitch::CENTRE};
  float bestScore = -std::numeric_limits<float>::infinity();
  const float nearPostShare = MatchTuning::SetPiece::NEAR_POST_CHANCE;
  const bool nearPost =
      roll < MatchTuning::SetPiece::SHORT_CORNER_CHANCE + nearPostShare;
  for (auto& candidate : players)
  {
    if (!active(candidate) || &candidate == &taker ||
        candidate.isHomeTeam != taker.isHomeTeam || candidate.isGoalkeeper)
      continue;
    if (!inPenaltyArea(candidate.movementTarget, !taker.isHomeTeam)) continue;
    const float targetMetres = metresBetween(candidate.movementTarget, goal);
    const float sideMetres =
        (candidate.movementTarget.y - MatchTuning::Pitch::CENTRE) *
        (ball.position.y < MatchTuning::Pitch::CENTRE ? -1.0f : 1.0f) *
        MatchTuning::Pitch::WIDTH_METRES;
    const float threat = MatchRules::headerReachMetres(candidate.heightMetres,
                                                       candidate.physicality);
    const float zone = nearPost ? sideMetres : -sideMetres;
    const float score =
        threat + zone * 0.15f - targetMetres * 0.05f +
        randomFloat(0.0f, MatchTuning::SetPiece::TARGET_RANDOMNESS);
    if (score > bestScore)
    {
      bestScore = score;
      target = &candidate;
    }
  }
  if (!target)
  {
    std::optional<PassOption> fallback = choosePassTarget(taker);
    if (fallback)
      passBall(taker, *fallback, true);
    else
      setPossession(taker);
    return;
  }
  PassOption delivery = evaluatePassOption(taker, *target);
  delivery.targetPoint = target->movementTarget;
  delivery.passDistance = distance(taker.position, delivery.targetPoint);
  delivery.lofted = true;
  delivery.intent = PassIntent::CROSS;
  passBall(taker, delivery, true);
}

void MatchEngine::takeDirectFreeKick(MatchPlayer& taker)
{
  // The wall blocks a share of attempts; the rest are struck over it with
  // a little less precision, and the keeper's view is partly screened.
  const float wallChance =
      std::clamp(MatchTuning::SetPiece::WALL_BLOCK_BASE -
                     taker.shooting * MatchTuning::SetPiece::WALL_BLOCK_SKILL,
                 0.05f, 0.6f);
  if (randomFloat(0.0f, 1.0f) < wallChance)
  {
    takeShot(taker);
    const float outward = taker.isHomeTeam ? -1.0f : 1.0f;
    const float speed =
        length(ball.velocity) * MatchTuning::SetPiece::WALL_REBOUND_SPEED_SHARE;
    MatchPlayer* wallPlayer = nullptr;
    float best = std::numeric_limits<float>::max();
    for (auto& player : players)
    {
      if (!active(player) || player.isHomeTeam == taker.isHomeTeam ||
          player.isGoalkeeper)
        continue;
      const float d = distance(player.position, ball.position);
      if (d < best)
      {
        best = d;
        wallPlayer = &player;
      }
    }
    ball.isShot = false;
    ball.shotOnTarget = false;
    ball.shotSaveResolved = true;
    ball.velocity = {outward * speed * randomFloat(0.3f, 1.0f),
                     speed * randomFloat(-0.8f, 0.8f)};
    ball.velocityZ = randomFloat(0.0f, MatchTuning::Aerial::HEADER_LIFT);
    ball.passCooldown = MatchTuning::Defending::DEFLECTION_COOLDOWN;
    if (wallPlayer)
    {
      ball.lastPossessor = wallPlayer->player;
      ++statsOf(*wallPlayer).clearances;
      logEvent(MatchEventType::SHOT_BLOCKED, "The free kick hits the wall",
               *wallPlayer);
    }
    return;
  }
  takeShot(taker, MatchTuning::SetPiece::DIRECT_FREE_KICK_XG);
  if (MatchPlayer* keeper = findGoalkeeper(!taker.isHomeTeam))
  {
    keepers[keeper->isHomeTeam ? 0 : 1].reactionRemaining +=
        MatchTuning::SetPiece::WALL_SCREEN_SECONDS /
        MatchTuning::Units::ACTION_SECONDS_PER_SIM_SECOND;
  }
}

void MatchEngine::resetPositions()
{
  for (auto& player : players)
  {
    if (!active(player)) continue;
    player.position = player.basePosition;
    player.movementTarget = player.basePosition;
    player.velocity = {0.0f, 0.0f};
    player.intent = PlayerIntent::HOLD_SHAPE;
    player.isPressing = false;
    player.isMakingRun = false;
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
    if (player.isHomeTeam != homeTeam || !active(player) ||
        (!includeGoalkeeper && player.isGoalkeeper))
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
  const auto goalkeeper =
      std::ranges::find_if(players,
                           [homeTeam](const MatchPlayer& player)
                           {
                             return player.isHomeTeam == homeTeam &&
                                    active(player) && player.isGoalkeeper;
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
    if (opponent.isHomeTeam != player.isHomeTeam && active(opponent))
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
      if (opponent.isHomeTeam == carrier.isHomeTeam || !active(opponent))
        continue;
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
    if (!active(opponent) || opponent.isHomeTeam == passer.isHomeTeam) continue;
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
    if (player.isHomeTeam == attackingHome || !active(player)) continue;
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

bool MatchEngine::isOffside(const MatchPlayer& receiver, bool attackingHome,
                            float lineTolerance) const
{
  const float defenderLine = offsideLine(attackingHome);
  const float margin = MatchTuning::Passing::OFFSIDE_MARGIN + lineTolerance;

  if (attackingHome)
  {
    return receiver.position.x > MatchTuning::Pitch::CENTRE &&
           receiver.position.x >
               ball.position.x + MatchTuning::Passing::OFFSIDE_MARGIN &&
           receiver.position.x > defenderLine + margin;
  }

  return receiver.position.x < MatchTuning::Pitch::CENTRE &&
         receiver.position.x <
             ball.position.x - MatchTuning::Passing::OFFSIDE_MARGIN &&
         receiver.position.x < defenderLine - margin;
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

int MatchEngine::getSubstitutionsUsed(bool homeTeam) const
{
  return homeTeam ? stats.homeSubstitutions : stats.awaySubstitutions;
}

int MatchEngine::getSubstitutionWindowsUsed(bool homeTeam) const
{
  return homeTeam ? homeSubstitutionWindows : awaySubstitutionWindows;
}

bool MatchEngine::canSubstitute(bool homeTeam) const
{
  if (state == MatchState::FULL_TIME) return false;
  if (getSubstitutionsUsed(homeTeam) >=
      MatchTuning::Rules::MAX_SUBSTITUTIONS_PER_TEAM)
    return false;
  // Half-time changes and further changes in an already used stoppage do
  // not consume a new window.
  const std::uint32_t lastWindow =
      homeTeam ? homeLastWindowStoppage : awayLastWindowStoppage;
  const bool sameStoppage =
      lastWindow == stoppageSequence &&
      (state != MatchState::PLAYING || manualSubstitutionStep == stepCounter);
  return state == MatchState::HALF_TIME ||
         getSubstitutionWindowsUsed(homeTeam) <
             MatchTuning::Substitution::MAX_WINDOWS ||
         sameStoppage;
}

void MatchEngine::setAutoSubstitutions(bool home, bool away)
{
  homeAutoSubstitutions = home;
  awayAutoSubstitutions = away;
}

bool MatchEngine::substitutePlayer(uint32_t outPlayerId, const Player* inPlayer)
{
  if (!inPlayer || state == MatchState::FULL_TIME ||
      std::ranges::any_of(players, [inPlayer](const MatchPlayer& player)
                          { return player.player == inPlayer; }) ||
      findPlayerStats(inPlayer->getId()) != nullptr)
  {
    return false;
  }

  const auto outgoing = std::ranges::find_if(
      players, [outPlayerId](const MatchPlayer& player)
      { return active(player) && player.player->getId() == outPlayerId; });
  if (outgoing == players.end()) return false;
  if (outgoing->player->getTeamId() != inPlayer->getTeamId()) return false;
  // A change made while the ball is live stops play for it; several changes
  // made together share that stoppage (and window).
  if (state == MatchState::PLAYING && manualSubstitutionStep != stepCounter)
  {
    beginStoppage();
    manualSubstitutionStep = stepCounter;
  }
  if (!canSubstitute(outgoing->isHomeTeam)) return false;
  return performSubstitution(*outgoing, inPlayer, SubstitutionReason::MANUAL);
}

bool MatchEngine::performSubstitution(MatchPlayer& outgoing,
                                      const Player* inPlayer,
                                      SubstitutionReason reason)
{
  if (!inPlayer || !active(outgoing) || !canSubstitute(outgoing.isHomeTeam))
    return false;
  const bool homeTeam = outgoing.isHomeTeam;
  if (state != MatchState::HALF_TIME)
  {
    std::uint32_t& lastWindow =
        homeTeam ? homeLastWindowStoppage : awayLastWindowStoppage;
    if (lastWindow != stoppageSequence)
    {
      lastWindow = stoppageSequence;
      ++(homeTeam ? homeSubstitutionWindows : awaySubstitutionWindows);
    }
    ++stoppageLogs[static_cast<std::size_t>(period - 1)].substitutions;
  }
  ++(homeTeam ? stats.homeSubstitutions : stats.awaySubstitutions);
  auto& bench = homeTeam ? homeBench : awayBench;
  std::erase(bench, inPlayer);

  PlayerMatchStats& leaving = statsOf(outgoing);
  leaving.substitutedOff = true;
  leaving.condition = outgoing.stamina;

  const bool hadPossession = ball.possessedBy == outgoing.player;
  const std::string outgoingName = outgoing.player->getName();
  const PlayerID outgoingId = outgoing.player->getId();
  const bool incomingKeeper = inPlayer->getRole() == PlayerRole::GK;
  if (incomingKeeper && !outgoing.isGoalkeeper)
  {
    // A replacement keeper takes over from any emergency keeper.
    for (auto& teammate : players)
    {
      if (teammate.isHomeTeam == homeTeam && teammate.isGoalkeeper)
        teammate.isGoalkeeper = false;
    }
    outgoing.basePosition = {
        homeTeam ? MatchTuning::Pitch::LINEUP_GOALKEEPER_X
                 : 1.0f - MatchTuning::Pitch::LINEUP_GOALKEEPER_X,
        MatchTuning::Pitch::CENTRE};
    outgoing.isGoalkeeper = true;
  }
  outgoing.player = inPlayer;
  loadAttributes(outgoing, inPlayer);
  outgoing.stamina = 1.0f;
  outgoing.isInjured = false;
  outgoing.yellowCards = 0;
  outgoing.actionCooldown = MatchTuning::Player::SUBSTITUTION_SETTLE_SECONDS;
  outgoing.statsIndex = addPlayerStats(outgoing, false);
  if (hadPossession) ball.possessedBy = inPlayer;
  if (ball.lastPossessor && ball.lastPossessor->getId() == outgoingId)
    ball.lastPossessor = inPlayer;

  substitutions.push_back({matchTimeMinutes, period, homeTeam, outgoingId,
                           inPlayer->getId(), reason});
  MatchEvent& event = logEvent(
      MatchEventType::SUBSTITUTION,
      outgoingName + " is replaced by " + inPlayer->getName(), outgoing);
  event.secondaryPlayerId = outgoingId;
  return true;
}

const Player* MatchEngine::chooseReplacement(bool homeTeam, PlayerRole role,
                                             bool wantGoalkeeper) const
{
  const auto& bench = homeTeam ? homeBench : awayBench;
  const Player* best = nullptr;
  float bestScore = -std::numeric_limits<float>::infinity();
  for (const Player* candidate : bench)
  {
    if (!candidate) continue;
    const bool keeper = candidate->getRole() == PlayerRole::GK;
    if (keeper != wantGoalkeeper) continue;
    const float fit = roleGroup(candidate->getRole()) == roleGroup(role)
                          ? MatchTuning::Substitution::ROLE_FIT_BONUS
                          : 0.0f;
    const float score =
        fit + static_cast<float>(candidate->getOverall(statsConfig)) /
                  MatchTuning::Player::RATING_SCALE;
    if (score > bestScore)
    {
      bestScore = score;
      best = candidate;
    }
  }
  return best;
}

void MatchEngine::runAiSubstitutions()
{
  if (homeLastAiReviewStoppage != stoppageSequence)
  {
    homeLastAiReviewStoppage = stoppageSequence;
    runAiSubstitutionsFor(true);
  }
  if (awayLastAiReviewStoppage != stoppageSequence)
  {
    awayLastAiReviewStoppage = stoppageSequence;
    runAiSubstitutionsFor(false);
  }
}

void MatchEngine::runAiSubstitutionsFor(bool homeTeam)
{
  // Injured players leave at the first stoppage: replaced when possible, or
  // the team continues a player short.
  for (auto& player : players)
  {
    if (!active(player) || player.isHomeTeam != homeTeam || !player.isInjured)
      continue;
    const bool autoManaged =
        homeTeam ? homeAutoSubstitutions : awayAutoSubstitutions;
    if (!autoManaged) continue;
    const Player* replacement =
        canSubstitute(homeTeam)
            ? chooseReplacement(homeTeam, player.player->getRole(),
                                player.isGoalkeeper)
            : nullptr;
    if (!replacement && player.isGoalkeeper && canSubstitute(homeTeam))
      replacement =
          chooseReplacement(homeTeam, player.player->getRole(), false);
    if (!replacement ||
        !performSubstitution(player, replacement, SubstitutionReason::INJURY))
    {
      removeFromPitch(player);
    }
  }

  if (!(homeTeam ? homeAutoSubstitutions : awayAutoSubstitutions) ||
      state == MatchState::PENALTY || period != 2 ||
      matchTimeMinutes < MatchTuning::Substitution::EARLIEST_TACTICAL_MINUTE)
  {
    return;
  }

  const int goalDifference =
      homeTeam ? homeScore - awayScore : awayScore - homeScore;
  const float minute = matchTimeMinutes;
  const float fatigueThreshold =
      MatchTuning::Substitution::FATIGUE_THRESHOLD +
      (minute >= MatchTuning::Substitution::LATE_GAME_MINUTE
           ? MatchTuning::Substitution::FATIGUE_THRESHOLD_LATE_GAIN
           : 0.0f);
  struct Need
  {
    MatchPlayer* player = nullptr;
    float score = 0.0f;
    SubstitutionReason reason = SubstitutionReason::FATIGUE;
    int wantedGroup = 2;
  };
  std::array<Need, 2> picks{};
  for (auto& player : players)
  {
    if (!active(player) || player.isHomeTeam != homeTeam || player.isGoalkeeper)
      continue;
    const PlayerRole role = player.player->getRole();
    Need need{&player, 0.0f, SubstitutionReason::FATIGUE, roleGroup(role)};
    need.score =
        std::max(0.0f, fatigueThreshold - player.stamina) *
            MatchTuning::Substitution::FATIGUE_NEED_SCALE +
        (minute - MatchTuning::Substitution::EARLIEST_TACTICAL_MINUTE) *
            MatchTuning::Substitution::MINUTE_NEED_GAIN *
            (1.0f - player.stamina);
    if (player.yellowCards > 0 && !isAttackingRole(role) &&
        minute >= MatchTuning::Substitution::CARD_RISK_MINUTE)
    {
      need.score += MatchTuning::Substitution::CARD_RISK_NEED;
      need.reason = SubstitutionReason::CARD_RISK;
    }
    if (goalDifference < 0 && isDefensiveRole(role) &&
        minute >= MatchTuning::Substitution::TRAILING_CHASE_MINUTE)
    {
      need.score += MatchTuning::Substitution::TACTICAL_NEED;
      need.reason = SubstitutionReason::TACTICAL;
      need.wantedGroup = 3;
    }
    else if (goalDifference > 0 && isAttackingRole(role) &&
             minute >= MatchTuning::Substitution::LEADING_PROTECT_MINUTE)
    {
      need.score += MatchTuning::Substitution::TACTICAL_NEED;
      need.reason = SubstitutionReason::TACTICAL;
      need.wantedGroup = 1;
    }
    if (need.score < MatchTuning::Substitution::MINIMUM_NEED) continue;
    for (std::size_t slot = 0; slot < picks.size(); ++slot)
    {
      if (need.score <= picks[slot].score) continue;
      for (std::size_t shifted = picks.size() - 1; shifted > slot; --shifted)
        picks[shifted] = picks[shifted - 1];
      picks[slot] = need;
      break;
    }
  }

  // Keep one change in reserve for late injuries until the final minutes.
  const int reserve =
      minute < MatchTuning::Substitution::LATE_GAME_MINUTE ? 1 : 0;
  for (const Need& need : picks)
  {
    if (!need.player || !canSubstitute(homeTeam)) break;
    if (getSubstitutionsUsed(homeTeam) >=
        MatchTuning::Rules::MAX_SUBSTITUTIONS_PER_TEAM - reserve)
      break;
    const PlayerRole wantedRole = need.wantedGroup == 3 ? PlayerRole::ST
                                  : need.wantedGroup == 1
                                      ? PlayerRole::CB
                                      : need.player->player->getRole();
    const Player* replacement = chooseReplacement(homeTeam, wantedRole, false);
    if (!replacement) break;
    performSubstitution(*need.player, replacement, need.reason);
  }
}

void MatchEngine::checkInjuries()
{
  std::uniform_real_distribution<float> roll(0.0f, 1.0f);
  for (auto& player : players)
  {
    if (!active(player) || player.isInjured) continue;
    const float speedRatio =
        length(player.velocity) / std::max(player.maxSpeed, EPSILON);
    const float tiredness = 1.0f - player.stamina;
    const float hazard =
        MatchTuning::Injury::BASE_HAZARD_PER_SECOND *
        (1.0f + tiredness * tiredness *
                    MatchTuning::Injury::FATIGUE_HAZARD_MULTIPLIER) *
        (1.0f + speedRatio * MatchTuning::Injury::INTENSITY_HAZARD_MULTIPLIER);
    const float probability =
        1.0f - std::exp(-hazard * MatchTuning::Injury::CHECK_INTERVAL_SECONDS);
    if (roll(incidentRng) < probability) injurePlayer(player, false);
  }
}

void MatchEngine::injurePlayer(MatchPlayer& player, bool fromContact)
{
  if (!active(player) || player.isInjured) return;
  player.isInjured = true;
  statsOf(player).injured = true;
  ++(player.isHomeTeam ? stats.homeInjuries : stats.awayInjuries);
  ++stoppageLogs[static_cast<std::size_t>(period - 1)].injuries;
  MatchEvent& event = logEvent(
      MatchEventType::INJURY,
      player.player->getName() +
          (fromContact ? " is injured in the challenge" : " is injured"),
      player);
  event.position = player.position;
  // A player who cannot run no longer carries the ball forward.
  if (ball.possessedBy == player.player) player.actionCooldown = 0.0f;
}

void MatchEngine::removeFromPitch(MatchPlayer& player)
{
  if (!active(player)) return;
  const bool homeTeam = player.isHomeTeam;
  const Vector2F vacated = player.basePosition;
  const bool wasKeeper = player.isGoalkeeper;
  statsOf(player).condition = player.stamina;
  if (ball.possessedBy == player.player)
  {
    ball.possessedBy = nullptr;
    ball.velocity = {0.0f, 0.0f};
  }
  // Park beside the dugouts, clear of play and of each other.
  int parked = 0;
  for (const auto& other : players)
    if (other.player && !other.onPitch) ++parked;
  player.onPitch = false;
  player.isGoalkeeper = false;
  player.isPressing = false;
  player.isMakingRun = false;
  player.intent = PlayerIntent::HOLD_SHAPE;
  player.velocity = {0.0f, 0.0f};
  player.position = {MatchTuning::Pitch::CENTRE +
                         (homeTeam ? -1.0f : 1.0f) *
                             (MatchTuning::Rules::PARKED_PLAYER_OFFSET +
                              static_cast<float>(parked) *
                                  MatchTuning::Rules::PARKED_PLAYER_SPACING),
                     MatchTuning::Pitch::PLAYER_MIN_Y};
  player.movementTarget = player.position;
  if (restartTaker == &player)
    restartTaker = findClosestPlayer(ball.position, homeTeam, false);
  if (wasKeeper) ensureGoalkeeper(homeTeam);
  rebalanceShape(homeTeam, vacated);

  // Law 3: a match cannot continue with fewer than seven players a side.
  const auto remaining = std::ranges::count_if(
      players, [homeTeam](const MatchPlayer& other)
      { return active(other) && other.isHomeTeam == homeTeam; });
  if (remaining < MatchTuning::Rules::MINIMUM_PLAYERS && !abandoned)
  {
    abandoned = true;
    logEvent(MatchEventType::INFO, "Match abandoned: too few players");
    period = 2;
    endPeriod();
  }
}

void MatchEngine::rebalanceShape(bool homeTeam, Vector2F vacatedBase)
{
  // A side reduced to ten fills a defensive hole with the nearest midfielder
  // and drops a little deeper overall.
  const float ownDepth = homeTeam ? vacatedBase.x : 1.0f - vacatedBase.x;
  if (ownDepth < MatchTuning::Rules::DEFENSIVE_SLOT_DEPTH)
  {
    MatchPlayer* cover = nullptr;
    float best = std::numeric_limits<float>::max();
    for (auto& player : players)
    {
      if (!active(player) || player.isHomeTeam != homeTeam ||
          player.isGoalkeeper)
        continue;
      const float depth =
          homeTeam ? player.basePosition.x : 1.0f - player.basePosition.x;
      if (depth <= ownDepth + EPSILON) continue;
      const float d = distance(player.basePosition, vacatedBase);
      if (d < best)
      {
        best = d;
        cover = &player;
      }
    }
    if (cover) cover->basePosition = vacatedBase;
  }
  for (auto& player : players)
  {
    if (!active(player) || player.isHomeTeam != homeTeam || player.isGoalkeeper)
      continue;
    player.basePosition.x = std::clamp(
        player.basePosition.x +
            (homeTeam ? -1.0f : 1.0f) * MatchTuning::Rules::SHORT_HANDED_DROP,
        MatchTuning::Pitch::PLAYER_MIN_X, MatchTuning::Pitch::PLAYER_MAX_X);
  }
}

void MatchEngine::ensureGoalkeeper(bool homeTeam)
{
  if (findGoalkeeper(homeTeam)) return;
  const bool autoManaged =
      homeTeam ? homeAutoSubstitutions : awayAutoSubstitutions;
  if (autoManaged && canSubstitute(homeTeam))
  {
    if (const Player* keeper =
            chooseReplacement(homeTeam, PlayerRole::GK, true))
    {
      // Sacrifice the most advanced outfield player for the new keeper.
      MatchPlayer* sacrificed = nullptr;
      float mostAdvanced = -std::numeric_limits<float>::infinity();
      for (auto& player : players)
      {
        if (!active(player) || player.isHomeTeam != homeTeam) continue;
        const float depth =
            homeTeam ? player.basePosition.x : 1.0f - player.basePosition.x;
        if (depth > mostAdvanced)
        {
          mostAdvanced = depth;
          sacrificed = &player;
        }
      }
      if (sacrificed &&
          performSubstitution(*sacrificed, keeper,
                              SubstitutionReason::GOALKEEPER_REPLACEMENT))
      {
        return;
      }
    }
  }
  // Otherwise the outfield player best with his hands goes in goal.
  MatchPlayer* emergency = nullptr;
  for (auto& player : players)
  {
    if (!active(player) || player.isHomeTeam != homeTeam) continue;
    if (!emergency || player.goalkeeping > emergency->goalkeeping)
      emergency = &player;
  }
  if (!emergency) return;
  emergency->isGoalkeeper = true;
  emergency->basePosition = {
      homeTeam ? MatchTuning::Pitch::LINEUP_GOALKEEPER_X
               : 1.0f - MatchTuning::Pitch::LINEUP_GOALKEEPER_X,
      MatchTuning::Pitch::CENTRE};
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
  period = scenarioMatchTime > MatchTuning::Timing::HALF_TIME_MINUTE ? 2 : 1;
  keepers = {};
  pendingAdvantage = {};
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
    matchPlayer.isInjured = false;
    matchPlayer.diveTimer = 0.0f;
    matchPlayer.isDiving = false;
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

MatchEvent& MatchEngine::logEvent(MatchEventType type,
                                  const std::string& message)
{
  if (events.size() >= MatchTuning::Timing::MAX_EVENTS)
    events.erase(events.begin());
  MatchEvent& event = events.emplace_back();
  event.timeMinute = matchTimeMinutes;
  event.description = message;
  event.type = type;
  event.period = period;
  const float regulationEnd = period == 1
                                  ? MatchTuning::Timing::HALF_TIME_MINUTE
                                  : MatchTuning::Timing::FULL_TIME_MINUTE;
  event.addedMinute = std::max(0.0f, matchTimeMinutes - regulationEnd);
  event.position = ball.position;
  return event;
}

MatchEvent& MatchEngine::logEvent(MatchEventType type,
                                  const std::string& message,
                                  const MatchPlayer& actor)
{
  MatchEvent& event = logEvent(type, message);
  event.hasTeam = true;
  event.isHomeTeam = actor.isHomeTeam;
  event.primaryPlayerId = actor.player ? actor.player->getId() : 0;
  event.position = actor.position;
  return event;
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
         << ",\"home_substitutions\":" << stats.homeSubstitutions
         << ",\"away_substitutions\":" << stats.awaySubstitutions
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
         << ",\"away_offsides\":" << stats.awayOffsides
         << ",\"home_fouls\":" << stats.homeFouls
         << ",\"away_fouls\":" << stats.awayFouls
         << ",\"home_yellow_cards\":" << stats.homeYellowCards
         << ",\"away_yellow_cards\":" << stats.awayYellowCards
         << ",\"home_red_cards\":" << stats.homeRedCards
         << ",\"away_red_cards\":" << stats.awayRedCards
         << ",\"home_corners\":" << stats.homeCorners
         << ",\"away_corners\":" << stats.awayCorners
         << ",\"home_injuries\":" << stats.homeInjuries
         << ",\"away_injuries\":" << stats.awayInjuries
         << "},\"clock\":{\"period\":" << period
         << ",\"added_first_half\":" << addedMinutes[0]
         << ",\"added_second_half\":" << addedMinutes[1]
         << ",\"referee_strictness\":" << refereeStrictness
         << "},\"goalkeepers\":{\"home\":\""
         << goalkeeperStateName(keepers[0].state) << "\",\"away\":\""
         << goalkeeperStateName(keepers[1].state) << "\"},\"players\":[";
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
           << ",\"on_pitch\":" << (player.onPitch ? "true" : "false")
           << ",\"goalkeeper\":" << (player.isGoalkeeper ? "true" : "false")
           << ",\"yellow_cards\":" << player.yellowCards
           << ",\"rating\":" << playerStats[player.statsIndex].rating << '}';
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
