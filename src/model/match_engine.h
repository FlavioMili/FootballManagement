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
#include <limits>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include "global/stats_config.h"
#include "global/types.h"
#include "model/lineup.h"
#include "model/match_events.h"
#include "model/match_rules.h"
#include "model/match_scenario.h"
#include "model/match_tuning.h"
#include "model/strategy.h"

enum class MatchState
{
  KICK_OFF,
  PLAYING,
  THROW_IN,
  GOAL_KICK,
  CORNER_KICK,
  FREE_KICK,
  PENALTY,
  GOAL,
  HALF_TIME,
  FULL_TIME
};

/** Team-level context shared by individual player decisions. */
enum class TeamPhase
{
  STOPPAGE,
  SET_PIECE,
  DEFENSIVE_BLOCK,
  DEFENSIVE_TRANSITION,
  ATTACKING_TRANSITION,
  POSSESSION,
  FINAL_THIRD
};

/** High-level decision currently driving a player's renderer-independent AI. */
enum class PlayerIntent
{
  HOLD_SHAPE,
  CARRY_BALL,
  OFFER_SUPPORT,
  RECEIVE_PASS,
  RUN_IN_BEHIND,
  ATTACK_BOX,
  OVERLAP,
  PRESS_BALL,
  COVER_PRESS,
  BLOCK_PASSING_LANE,
  MARK_OPPONENT,
  CLAIM_LOOSE_BALL,
  RECOVER_SHAPE,
  GOALKEEP
};

/** Tactical purpose of the most recently executed pass. */
enum class PassIntent
{
  RECYCLE,
  PROGRESSIVE,
  THROUGH_BALL,
  CROSS,
  CUTBACK,
  SWITCH_PLAY,
  PRESSURE_RELEASE,
  SET_PIECE
};

struct PassDecision
{
  std::uint32_t passerId = 0;
  std::uint32_t receiverId = 0;
  Vector2F targetPoint{MatchTuning::Pitch::CENTRE, MatchTuning::Pitch::CENTRE};
  PassIntent intent = PassIntent::RECYCLE;
  float utility = 0.0f;
  float progression = 0.0f;
  float laneRisk = 0.0f;
  float completionProbability = 0.0f;
};

/** Best and runner-up pass candidates plus the final chosen reason. */
enum class ScenarioAction
{
  NONE,
  SHOT,
  PASS,
  CARRY,
  SHIELD,
  CLEAR
};

/** Semantic goalkeeper state. Animation stays out of the engine. */
enum class GoalkeeperState
{
  SET_POSITION,
  SWEEP,
  RUSH,
  CLAIM,
  DIVE,
  HOLD,
  DISTRIBUTE,
  RECOVER
};

std::string_view goalkeeperStateName(GoalkeeperState state);

struct ScenarioDecision
{
  ScenarioAction action = ScenarioAction::NONE;
  std::optional<PassDecision> best;
  std::optional<PassDecision> runnerUp;
  std::string reason;
  float passUtility = -std::numeric_limits<float>::infinity();
  float shotUtility = -std::numeric_limits<float>::infinity();
  float carryUtility = -std::numeric_limits<float>::infinity();
  float shieldUtility = -std::numeric_limits<float>::infinity();
};

struct MatchPlayer
{
  const Player* player = nullptr;
  bool isHomeTeam = false;
  Vector2F position{MatchTuning::Pitch::CENTRE, MatchTuning::Pitch::CENTRE};
  Vector2F velocity{0.0f, 0.0f};
  Vector2F basePosition{MatchTuning::Pitch::CENTRE, MatchTuning::Pitch::CENTRE};
  Vector2F movementTarget{MatchTuning::Pitch::CENTRE,
                          MatchTuning::Pitch::CENTRE};
  PlayerIntent intent = PlayerIntent::HOLD_SHAPE;

  float facingAngle = 0.0f;
  float targetAngle = 0.0f;
  float turnRate = MatchTuning::Player::TURN_RATE_RADIANS;

  bool isTrapping = false;
  float trapTimer = 0.0f;
  bool isDiving = false;
  float diveTimer = 0.0f;
  bool isPressing = false;
  bool isMakingRun = false;

  float stamina = 1.0f;
  float maxSpeed = MatchTuning::Player::BASE_MAX_SPEED;
  float acceleration = MatchTuning::Player::BASE_ACCELERATION;
  float tackleCooldown = 0.0f;
  float actionCooldown = 0.0f;

  float pace = MatchTuning::Player::DEFAULT_ATTRIBUTE;
  float shooting = MatchTuning::Player::DEFAULT_ATTRIBUTE;
  float passing = MatchTuning::Player::DEFAULT_ATTRIBUTE;
  float dribbling = MatchTuning::Player::DEFAULT_ATTRIBUTE;
  float defending = MatchTuning::Player::DEFAULT_ATTRIBUTE;
  float goalkeeping = MatchTuning::Player::DEFAULT_ATTRIBUTE;
  float physicality = MatchTuning::Player::DEFAULT_ATTRIBUTE;
  /** Stamina attribute (endurance); `stamina` above is the live condition. */
  float endurance = MatchTuning::Player::DEFAULT_ATTRIBUTE;
  float vision = MatchTuning::Player::DEFAULT_ATTRIBUTE;
  float heightMetres = MatchTuning::Units::DEFAULT_PLAYER_HEIGHT_METRES;

  /** False once sent off or forced off injured with no substitute left. */
  bool onPitch = true;
  /** Goalkeeper duties (the natural keeper, or an emergency replacement). */
  bool isGoalkeeper = false;
  bool isInjured = false;
  int yellowCards = 0;
  /** Index into MatchEngine::getPlayerStats() for the current occupant. */
  std::size_t statsIndex = 0;
};

struct MatchBall
{
  Vector2F position{MatchTuning::Pitch::CENTRE, MatchTuning::Pitch::CENTRE};
  float z = 0.0f;
  Vector2F velocity{0.0f, 0.0f};
  float velocityZ = 0.0f;
  float curve = 0.0f;
  float friction = MatchTuning::Passing::GROUND_FRICTION;

  const Player* possessedBy = nullptr;
  const Player* lastPossessor = nullptr;
  const Player* intendedReceiver = nullptr;
  float passCooldown = 0.0f;

  bool isPass = false;
  bool passByHome = false;
  bool passWasOffside = false;
  bool isShot = false;
  bool shotByHome = false;
  /** Whether the shot's flight crosses the goal line inside the frame. */
  bool shotOnTarget = false;
  float shotXG = 0.0f;
  /** Predicted goal-line crossing of the current shot (y and metres high). */
  float shotTargetY = MatchTuning::Pitch::CENTRE;
  float shotTargetHeightMetres = 0.0f;
  float shotElapsedSeconds = 0.0f;
  bool shotIsHeader = false;
  bool shotIsPenalty = false;
  bool shotFromSetPiece = false;
  bool shotSaveResolved = false;
  /** Lofted delivery (cross, corner, long ball) that can be contested. */
  bool isAerialDelivery = false;
  /** Thrown in and untouched since: it cannot score directly. */
  bool fromThrowIn = false;
};

struct MatchStats
{
  int homeShots = 0;
  int awayShots = 0;
  int homeOnTarget = 0;
  int awayOnTarget = 0;
  int homeCorners = 0;
  int awayCorners = 0;
  int homeFouls = 0;
  int awayFouls = 0;
  int homeSaves = 0;
  int awaySaves = 0;
  int homePassesAttempted = 0;
  int awayPassesAttempted = 0;
  int homePassesCompleted = 0;
  int awayPassesCompleted = 0;
  int homeProgressivePasses = 0;
  int awayProgressivePasses = 0;
  int homeThroughBalls = 0;
  int awayThroughBalls = 0;
  int homeCrosses = 0;
  int awayCrosses = 0;
  int homeCutbacks = 0;
  int awayCutbacks = 0;
  int homeSwitchesOfPlay = 0;
  int awaySwitchesOfPlay = 0;
  int homeTackles = 0;
  int awayTackles = 0;
  int homeOffsides = 0;
  int awayOffsides = 0;
  int homeYellowCards = 0;
  int awayYellowCards = 0;
  int homeRedCards = 0;
  int awayRedCards = 0;
  int homeTackleAttempts = 0;
  int awayTackleAttempts = 0;
  int homeInjuries = 0;
  int awayInjuries = 0;
  int homePenalties = 0;
  int awayPenalties = 0;
  int homeSubstitutions = 0;
  int awaySubstitutions = 0;
  int homeAerialDuelsWon = 0;
  int awayAerialDuelsWon = 0;
  int homeHeadedShots = 0;
  int awayHeadedShots = 0;
  int homeSetPieceShots = 0;
  int awaySetPieceShots = 0;
  int homeAdvantagesPlayed = 0;
  int awayAdvantagesPlayed = 0;
  int homeShotsInsideBox = 0;
  int awayShotsInsideBox = 0;
  int homeHeadedGoals = 0;
  int awayHeadedGoals = 0;
  int homeSetPieceGoals = 0;
  int awaySetPieceGoals = 0;
  int homePenaltyGoals = 0;
  int awayPenaltyGoals = 0;
  float homePossession = MatchTuning::Statistics::EVEN_POSSESSION_PERCENT;
  float awayPossession = MatchTuning::Statistics::EVEN_POSSESSION_PERCENT;
  float homeShotXG = 0.0f;
  float awayShotXG = 0.0f;
  /** Clock minutes with the ball in play (restarts excluded). */
  float ballInPlayMinutes = 0.0f;
};

/**
 * Stateful, deterministic-when-seeded live match simulation.
 *
 * update() uses a fixed internal timestep, so the same seed produces the same
 * match at different render frame rates. Home attacks toward x=1 and away
 * attacks toward x=0.
 */
class MatchEngine
{
 public:
  MatchEngine(const Lineup& home_lineup, const Lineup& away_lineup,
              const Strategy& home_strat, const Strategy& away_strat,
              const StatsConfig& config);
  MatchEngine(const Lineup& home_lineup, const Lineup& away_lineup,
              const Strategy& home_strat, const Strategy& away_strat,
              const StatsConfig& config, uint32_t seed);

  void update(float deltaTime);

  const std::vector<MatchPlayer>& getPlayers() const { return players; }
  const MatchBall& getBall() const { return ball; }
  const std::vector<MatchEvent>& getEvents() const { return events; }
  MatchState getState() const { return state; }
  const MatchStats& getStats() const { return stats; }
  TeamPhase getHomePhase() const { return homePhase; }
  TeamPhase getAwayPhase() const { return awayPhase; }
  float getTransitionSecondsRemaining() const
  {
    return transitionSecondsRemaining;
  }
  const PassDecision& getLastPassDecision() const { return lastPassDecision; }

  /**
   * Replaces an on-pitch player with a bench player of the same team.
   * Enforces the substitution limit (5) and windows (3, half-time excluded);
   * returns false when the change is not allowed.
   */
  bool substitutePlayer(uint32_t outPlayerId, const Player* inPlayer);
  /** Whether the side can still make a substitution right now. */
  bool canSubstitute(bool homeTeam) const;
  int getSubstitutionsUsed(bool homeTeam) const;
  int getSubstitutionWindowsUsed(bool homeTeam) const;
  /**
   * AI substitutions (fatigue, injuries, cards, game state) are enabled for
   * both sides by default so headless matches manage themselves. A side
   * managed by the human should disable its own.
   */
  void setAutoSubstitutions(bool home, bool away);
  const std::vector<MatchSubstitution>& getSubstitutions() const
  {
    return substitutions;
  }

  /** Per-player statistics for everyone who took part (never truncated). */
  const std::vector<PlayerMatchStats>& getPlayerStats() const
  {
    return playerStats;
  }
  const PlayerMatchStats* findPlayerStats(PlayerID playerId) const;
  /**
   * Live physical condition in [0, 1] (end-of-match condition after full
   * time) so a career simulation can carry fatigue between matches.
   */
  std::optional<float> getPlayerCondition(PlayerID playerId) const;
  /** Sets a starter's condition before kick-off; false if not applicable. */
  bool setPlayerCondition(PlayerID playerId, float condition);

  /** Current half: 1 or 2. */
  int getPeriod() const { return period; }
  /** Announced added minutes of a half (0 until announced). */
  int getAddedMinutes(int half) const
  {
    return half == 1 ? addedMinutes[0] : half == 2 ? addedMinutes[1] : 0;
  }
  bool isInAddedTime() const;
  /** True when a side fell below seven players and the match was stopped. */
  bool isAbandoned() const { return abandoned; }
  /** Per-match referee strictness (1 is average). */
  float getRefereeStrictness() const { return refereeStrictness; }

  int getHomeScore() const { return homeScore; }
  int getAwayScore() const { return awayScore; }
  float getMatchTimeMinutes() const { return matchTimeMinutes; }
  /** Total clock minutes played so far, including both added times. */
  float getElapsedMatchMinutes() const { return elapsedMatchMinutes; }
  int getLastUpdateStepCount() const { return lastUpdateStepCount; }
  std::uint64_t getDroppedSimulationSteps() const
  {
    return droppedSimulationSteps;
  }

  /** Whether the last scored goal was conceded by the away team's keeper. */
  bool getGoalScoredByHome() const { return goalScoredByHome; }

  /** Seconds left in the goal celebration before the kick-off restart. */
  float getGoalCelebrationRemaining() const { return goalCelebrationRemaining; }

  /**
   * Interpolation fraction between the previous and current fixed-step state,
   * derived from the engine accumulator. In [0, 1).
   */
  float getInterpolationAlpha() const;
  const std::vector<Vector2F>& getPreviousPlayerPositions() const
  {
    return previousPlayerPositions;
  }
  const std::vector<float>& getPreviousPlayerFacingAngles() const
  {
    return previousPlayerFacingAngles;
  }
  Vector2F getPreviousBallPosition() const { return previousBallPosition; }
  float getPreviousBallZ() const { return previousBallZ; }

  /** Machine-readable state for headless tests and external debug tooling. */
  std::string getDebugSnapshotJson() const;
  bool writeDebugSnapshot(std::string_view path) const;

  /** Semantic goalkeeper state for each side, updated every fixed step. */
  GoalkeeperState getHomeGoalkeeperState() const { return keepers[0].state; }
  GoalkeeperState getAwayGoalkeeperState() const { return keepers[1].state; }

  /**
   * Loads a fully-specified deterministic scenario and evaluates the carrier's
   * decision exactly as the live engine would (no physics step is advanced).
   * The renderer never calls this; it is only an explicit headless evaluation
   * hook for the scenario suite. The optional parameters let scenario tests
   * reproduce late-game and score-state behaviour deterministically.
   */
  bool applyScenario(const MatchScenario& scenario,
                     MatchState scenarioState = MatchState::PLAYING,
                     float scenarioMatchTime = 0.0f, int scenarioHomeScore = 0,
                     int scenarioAwayScore = 0);
  const ScenarioDecision& getLastScenarioDecision() const
  {
    return lastScenarioDecision;
  }

 private:
  std::vector<MatchPlayer> players;
  MatchBall ball;
  const StatsConfig& statsConfig;
  Strategy homeStrategy;
  Strategy awayStrategy;
  std::mt19937 rng;
  // Referee decisions and injuries draw from their own stream so tuning them
  // does not reshuffle player decisions and ball physics.
  std::mt19937 incidentRng;
  std::uint32_t matchSeed = 0;
  std::uint64_t stepCounter = 0;
  float refereeStrictness = 1.0f;

  std::vector<PlayerMatchStats> playerStats;
  std::vector<MatchSubstitution> substitutions;
  std::vector<const Player*> homeBench;
  std::vector<const Player*> awayBench;
  bool homeAutoSubstitutions = true;
  bool awayAutoSubstitutions = true;
  int homeSubstitutionWindows = 0;
  int awaySubstitutionWindows = 0;
  std::uint32_t stoppageSequence = 0;
  std::uint64_t lastStoppageStep = 0;
  std::uint64_t manualSubstitutionStep =
      std::numeric_limits<std::uint64_t>::max();
  std::uint32_t homeLastWindowStoppage = 0;
  std::uint32_t awayLastWindowStoppage = 0;
  std::uint32_t homeLastAiReviewStoppage = 0;
  std::uint32_t awayLastAiReviewStoppage = 0;

  int period = 1;
  bool abandoned = false;
  std::array<int, 2> addedMinutes{0, 0};
  std::array<MatchRules::StoppageLog, 2> stoppageLogs{};
  float elapsedMatchMinutes = 0.0f;
  float injuryCheckTimer = 0.0f;

  struct PendingAdvantage
  {
    bool active = false;
    bool fouledTeamHome = false;
    Vector2F position{MatchTuning::Pitch::CENTRE, MatchTuning::Pitch::CENTRE};
    float secondsRemaining = 0.0f;
  };
  PendingAdvantage pendingAdvantage;

  /** Chain used for assists and key passes. */
  const Player* lastCompletedPasser = nullptr;
  const Player* lastCompletedReceiver = nullptr;
  const Player* shotAssistCandidate = nullptr;
  const Player* lastShooter = nullptr;
  bool restartIsSetPiece = false;
  /** Shots shortly after a corner or attacking free kick count as set-piece
   * shots. */
  float setPiecePhaseRemaining = 0.0f;

  std::vector<Vector2F> previousPlayerPositions;
  std::vector<float> previousPlayerFacingAngles;
  Vector2F previousBallPosition{MatchTuning::Pitch::CENTRE,
                                MatchTuning::Pitch::CENTRE};
  float previousBallZ = 0.0f;

  std::vector<MatchEvent> events;
  MatchStats stats;
  PassDecision lastPassDecision;
  ScenarioDecision lastScenarioDecision;
  struct GoalkeeperControl
  {
    GoalkeeperState state = GoalkeeperState::SET_POSITION;
    float timer = 0.0f;
    float reactionRemaining = 0.0f;
    float diveTargetY = MatchTuning::Pitch::CENTRE;
    float lateralVelocity = 0.0f;
  };
  /** Index 0 is the home keeper, 1 the away keeper. */
  std::array<GoalkeeperControl, 2> keepers{};
  MatchState state = MatchState::KICK_OFF;
  TeamPhase homePhase = TeamPhase::SET_PIECE;
  TeamPhase awayPhase = TeamPhase::SET_PIECE;
  std::optional<bool> lastControlledTeamHome;
  float transitionSecondsRemaining = 0.0f;
  MatchPlayer* restartTaker = nullptr;
  float setPieceTimer = 0.0f;
  bool goalScoredByHome = false;
  float goalCelebrationRemaining = 0.0f;
  float accumulator = 0.0f;
  float homePossessionMinutes = 0.0f;
  float awayPossessionMinutes = 0.0f;
  float matchTimeMinutes = 0.0f;
  int homeScore = 0;
  int awayScore = 0;
  int lastUpdateStepCount = 0;
  std::uint64_t droppedSimulationSteps = 0;

  struct PassOption
  {
    MatchPlayer* receiver = nullptr;
    Vector2F targetPoint{MatchTuning::Pitch::CENTRE,
                         MatchTuning::Pitch::CENTRE};
    PassIntent intent = PassIntent::RECYCLE;
    float utility = 0.0f;
    float progression = 0.0f;
    float laneRisk = 0.0f;
    float completionProbability = 0.0f;
    float passDistance = 0.0f;
    bool lofted = false;
  };

  void initializePlayers(const Lineup& lineup, bool isHomeTeam);
  void loadAttributes(MatchPlayer& matchPlayer, const Player* player) const;
  std::size_t addPlayerStats(const MatchPlayer& matchPlayer, bool started);
  PlayerMatchStats& statsOf(const MatchPlayer& matchPlayer);
  void refreshRatings();
  void accumulatePlayerLoad(float dt);
  void updateMatchClock();
  void announceAddedTime();
  void endPeriod();
  void checkInjuries();
  void injurePlayer(MatchPlayer& player, bool fromContact);
  void removeFromPitch(MatchPlayer& player);
  void rebalanceShape(bool homeTeam, Vector2F vacatedBase);
  void ensureGoalkeeper(bool homeTeam);
  void beginStoppage();
  void runAiSubstitutions();
  void runAiSubstitutionsFor(bool homeTeam);
  bool performSubstitution(MatchPlayer& outgoing, const Player* inPlayer,
                           SubstitutionReason reason);
  const Player* chooseReplacement(bool homeTeam, PlayerRole role,
                                  bool wantGoalkeeper) const;
  void commitFoul(MatchPlayer& offender, MatchPlayer& victim, bool ballWon,
                  bool reckless);
  void applySanction(MatchPlayer& offender, MatchRules::FoulSanction sanction);
  bool deniesGoalChance(const MatchPlayer& victim,
                        const MatchPlayer& offender) const;
  void updatePendingAdvantage(float dt);
  void integrateMovement(MatchPlayer& player, Vector2F target, float dt,
                         bool urgent);
  void updateRestartMovement(float dt);
  void separatePlayers();
  Vector2F goalkeeperTarget(MatchPlayer& keeper, const MatchPlayer* carrier);
  void diveGoalkeeper(MatchPlayer& keeper, float dt);
  void planGoalkeeperDive(bool defendingHome, bool penalty);
  void resolveShotAtGoalkeeper(MatchPlayer& goalkeeper);
  bool resolveAerialContest();
  void headBall(MatchPlayer& header);
  void clearBehind(MatchPlayer& defender);
  void clearBall(MatchPlayer& defender);
  void parryShot(MatchPlayer& goalkeeper, bool overTheBar);
  float verticalSpeedFor(float horizontalDistance, float speed, float friction,
                         float startZ, float arrivalZ) const;
  void takeSetPiece(MatchPlayer& taker, MatchState restartState);
  void takeCorner(MatchPlayer& taker);
  void takeDirectFreeKick(MatchPlayer& taker);
  void arrangeSetPiece(bool attackingHome, Vector2F ballPosition);
  MatchPlayer* bestSetPieceTaker(bool homeTeam, bool shooting);
  void placeTaker(MatchPlayer& taker, Vector2F spot);
  float hashNoise(std::uint32_t salt, std::uint32_t key) const;
  /** Like hashNoise, but constant over windows of `epochSteps` steps. */
  float epochNoise(std::uint32_t salt, std::uint32_t key,
                   std::uint32_t epochSteps) const;
  float executionErrorScale(const MatchPlayer& player) const;
  void recordPassCompletion(MatchPlayer& receiver);
  void captureInterpolationFrame();
  void simulateStep(float dt);
  void updateTeamPhases();
  void updateMovement(float dt);
  void updateBall(float dt);
  void updateBallInNet(float dt);
  void resolvePossessionAndActions(float dt);
  void resolveLooseBall();
  void attemptTackle(MatchPlayer& carrier, MatchPlayer& defender);
  void decideAction(MatchPlayer& carrier);
  void passBall(MatchPlayer& passer, const PassOption& option,
                bool forceLofted = false);
  void takeShot(MatchPlayer& shooter, float forcedXG = -1.0f,
                bool header = false);
  void setPossession(MatchPlayer& player);
  void clearFlightState();

  std::optional<PassOption> choosePassTarget(MatchPlayer& passer);
  PassOption evaluatePassOption(MatchPlayer& passer,
                                MatchPlayer& receiver) const;
  MatchPlayer* findClosestPlayer(Vector2F position, bool homeTeam,
                                 bool includeGoalkeeper = true);
  MatchPlayer* findGoalkeeper(bool homeTeam);
  MatchPlayer* findMatchPlayer(const Player* player);
  float nearestOpponentDistance(const MatchPlayer& player) const;
  float openSpaceAhead(const MatchPlayer& carrier) const;
  float passingLaneRisk(const MatchPlayer& passer,
                        const MatchPlayer& receiver) const;
  float estimateShotXG(const MatchPlayer& shooter) const;
  bool isOffside(const MatchPlayer& receiver, bool attackingHome,
                 float lineTolerance = 0.0f) const;
  float offsideLine(bool attackingHome) const;

  void checkOutOfBounds();
  void scoreGoal(bool homeTeam);
  void makeSave(MatchPlayer& goalkeeper);
  void completeRestart();
  void setupKickOff(bool homeKickingOff);
  void setupThrowIn(bool homeTeam);
  void setupGoalKick(bool homeTeam);
  void setupCorner(bool homeTeam, bool topCorner);
  void setupFreeKick(bool homeTeam, Vector2F foulPos);
  void setupPenalty(bool homeTeam);
  void resetPositions();

  float attribute(const Player* player, std::string_view name) const;
  float randomFloat(float minimum, float maximum);
  bool isHomePlayer(const Player* player) const;
  MatchEvent& logEvent(MatchEventType type, const std::string& message);
  MatchEvent& logEvent(MatchEventType type, const std::string& message,
                       const MatchPlayer& actor);
};
