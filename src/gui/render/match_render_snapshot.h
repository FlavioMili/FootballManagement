// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cmath>
#include <vector>

#include "model/match_engine.h"

/**
 * Read-only normalized world state consumed by renderers.
 *
 * The snapshot is rebuilt every frame from a `MatchEngine` and carries the
 * previous and current fixed-step positions plus the interpolation fraction,
 * so 2D and future 3D renderers can interpolate without touching the
 * simulation. Renderers must never mutate this data or the engine.
 *
 * Units: positions are normalised pitch coordinates (x along the 105 m
 * length, home attacking +x; y across the 68 m width). Ball `currentZ` and
 * `previousZ` are in length units (metres = z * 105); the `*HeightMetres`
 * fields carry the same value already converted to metres.
 */
struct MatchRenderPlayer
{
  const Player* player = nullptr;
  bool isHomeTeam = false;
  Vector2F previousPosition{MatchTuning::Pitch::CENTRE,
                            MatchTuning::Pitch::CENTRE};
  Vector2F currentPosition{MatchTuning::Pitch::CENTRE,
                           MatchTuning::Pitch::CENTRE};
  Vector2F movementTarget{MatchTuning::Pitch::CENTRE,
                          MatchTuning::Pitch::CENTRE};
  PlayerIntent intent = PlayerIntent::HOLD_SHAPE;
  float previousFacingAngle = 0.0f;
  float currentFacingAngle = 0.0f;
  float stamina = 1.0f;
  bool possessesBall = false;
  /** False once sent off or forced off; such players wait by the bench. */
  bool onPitch = true;
  bool isGoalkeeper = false;
  bool isDiving = false;
  bool isInjured = false;
  int yellowCards = 0;
  float heightMetres = MatchTuning::Units::DEFAULT_PLAYER_HEIGHT_METRES;
  /** Running speed in metres per simulated second (drives the stride). */
  float speedMetresPerSecond = 0.0f;
  /** Seconds before this player may challenge again; it jumps up when he
   * goes into a tackle (renderers start the tackle pose on the rise). */
  float tackleCooldown = 0.0f;
};

struct MatchRenderBall
{
  Vector2F previousPosition{MatchTuning::Pitch::CENTRE,
                            MatchTuning::Pitch::CENTRE};
  Vector2F currentPosition{MatchTuning::Pitch::CENTRE,
                           MatchTuning::Pitch::CENTRE};
  float previousZ = 0.0f;
  float currentZ = 0.0f;
  const Player* possessedBy = nullptr;
  float previousHeightMetres = 0.0f;
  float currentHeightMetres = 0.0f;
  bool isShot = false;
  bool isAerialDelivery = false;
  /** Thrown in from the touchline (until the next touch). */
  bool fromThrowIn = false;
  /**
   * Who last struck the ball and the seconds left in his lockout. The
   * lockout is reset on every kick and only counts down otherwise, so a rise
   * between two snapshots marks the step the ball was struck.
   */
  const Player* kicker = nullptr;
  float kickerLockout = 0.0f;
};

struct MatchRenderSnapshot
{
  std::vector<MatchRenderPlayer> players;
  /** Sides whose strips the views draw (the match screen's own ids, so its
   * colour swatches match); 0: taken from the players' clubs. */
  TeamID homeTeam = 0;
  TeamID awayTeam = 0;
  MatchRenderBall ball;
  MatchState state = MatchState::KICK_OFF;
  TeamPhase homePhase = TeamPhase::SET_PIECE;
  TeamPhase awayPhase = TeamPhase::SET_PIECE;
  float transitionSecondsRemaining = 0.0f;
  int homeScore = 0;
  int awayScore = 0;
  bool goalScoredByHome = false;
  float goalCelebrationRemaining = 0.0f;
  float goalCelebrationDuration = MatchTuning::Timing::GOAL_CELEBRATION_SECONDS;
  float matchTimeMinutes = 0.0f;
  const std::vector<MatchEvent>* events = nullptr;
  const MatchStats* stats = nullptr;
  float interpolationAlpha = 0.0f;
  /** 1 or 2; added minutes are 0 until announced. */
  int period = 1;
  int addedMinutesFirstHalf = 0;
  int addedMinutesSecondHalf = 0;
  GoalkeeperState homeGoalkeeperState = GoalkeeperState::SET_POSITION;
  GoalkeeperState awayGoalkeeperState = GoalkeeperState::SET_POSITION;
  const std::vector<PlayerMatchStats>* playerStats = nullptr;
};

/**
 * Refills `snapshot` from a live engine, reusing its storage so a view that
 * keeps one snapshot allocates nothing per frame.
 */
inline void fillMatchRenderSnapshot(const MatchEngine& engine,
                                    MatchRenderSnapshot& snapshot)
{
  const auto& players = engine.getPlayers();
  const auto& previousPositions = engine.getPreviousPlayerPositions();
  const auto& previousFacingAngles = engine.getPreviousPlayerFacingAngles();
  snapshot.players.clear();
  snapshot.players.reserve(players.size());
  for (std::size_t index = 0; index < players.size(); ++index)
  {
    const MatchPlayer& source = players[index];
    MatchRenderPlayer& renderPlayer = snapshot.players.emplace_back();
    renderPlayer.player = source.player;
    renderPlayer.isHomeTeam = source.isHomeTeam;
    renderPlayer.currentPosition = source.position;
    renderPlayer.movementTarget = source.movementTarget;
    renderPlayer.intent = source.intent;
    renderPlayer.currentFacingAngle = source.facingAngle;
    renderPlayer.stamina = source.stamina;
    renderPlayer.possessesBall = source.player != nullptr &&
                                 engine.getBall().possessedBy == source.player;
    renderPlayer.onPitch = source.onPitch;
    renderPlayer.isGoalkeeper = source.isGoalkeeper;
    renderPlayer.isDiving = source.isDiving;
    renderPlayer.isInjured = source.isInjured;
    renderPlayer.yellowCards = source.yellowCards;
    renderPlayer.heightMetres = source.heightMetres;
    renderPlayer.speedMetresPerSecond =
        std::sqrt(source.velocity.x * source.velocity.x +
                  source.velocity.y * source.velocity.y);
    renderPlayer.tackleCooldown = source.tackleCooldown;
    renderPlayer.previousPosition = index < previousPositions.size()
                                        ? previousPositions[index]
                                        : renderPlayer.currentPosition;
    renderPlayer.previousFacingAngle = index < previousFacingAngles.size()
                                           ? previousFacingAngles[index]
                                           : renderPlayer.currentFacingAngle;
  }

  const MatchBall& ball = engine.getBall();
  snapshot.ball.currentPosition = ball.position;
  snapshot.ball.previousPosition = engine.getPreviousBallPosition();
  snapshot.ball.currentZ = ball.z;
  snapshot.ball.previousZ = engine.getPreviousBallZ();
  snapshot.ball.possessedBy = ball.possessedBy;
  snapshot.ball.currentHeightMetres =
      ball.z * MatchTuning::Units::BALL_Z_METRES;
  snapshot.ball.previousHeightMetres =
      engine.getPreviousBallZ() * MatchTuning::Units::BALL_Z_METRES;
  snapshot.ball.isShot = ball.isShot;
  snapshot.ball.isAerialDelivery = ball.isAerialDelivery;
  snapshot.ball.fromThrowIn = ball.fromThrowIn;
  snapshot.ball.kicker = ball.kicker;
  snapshot.ball.kickerLockout = ball.kickerLockout;

  snapshot.state = engine.getState();
  snapshot.homePhase = engine.getHomePhase();
  snapshot.awayPhase = engine.getAwayPhase();
  snapshot.transitionSecondsRemaining = engine.getTransitionSecondsRemaining();
  snapshot.homeScore = engine.getHomeScore();
  snapshot.awayScore = engine.getAwayScore();
  snapshot.goalScoredByHome = engine.getGoalScoredByHome();
  snapshot.goalCelebrationRemaining = engine.getGoalCelebrationRemaining();
  snapshot.goalCelebrationDuration = engine.getGoalCelebrationDuration();
  snapshot.matchTimeMinutes = engine.getMatchTimeMinutes();
  snapshot.events = &engine.getEvents();
  snapshot.stats = &engine.getStats();
  snapshot.interpolationAlpha = engine.getInterpolationAlpha();
  snapshot.period = engine.getPeriod();
  snapshot.addedMinutesFirstHalf = engine.getAddedMinutes(1);
  snapshot.addedMinutesSecondHalf = engine.getAddedMinutes(2);
  snapshot.homeGoalkeeperState = engine.getHomeGoalkeeperState();
  snapshot.awayGoalkeeperState = engine.getAwayGoalkeeperState();
  snapshot.playerStats = &engine.getPlayerStats();
}

/** Builds a read-only render snapshot from a live engine. */
inline MatchRenderSnapshot buildMatchRenderSnapshot(const MatchEngine& engine)
{
  MatchRenderSnapshot snapshot;
  fillMatchRenderSnapshot(engine, snapshot);
  return snapshot;
}

/** Linear interpolation of a 2D point used by renderers. */
inline Vector2F lerpRenderPosition(Vector2F first, Vector2F second, float alpha)
{
  return {first.x + (second.x - first.x) * alpha,
          first.y + (second.y - first.y) * alpha};
}
