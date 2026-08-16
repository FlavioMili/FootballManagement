// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <vector>

#include "model/match_engine.h"

/**
 * Read-only normalized world state consumed by renderers.
 *
 * The snapshot is rebuilt every frame from a `MatchEngine` and carries the
 * previous and current fixed-step positions plus the interpolation fraction,
 * so 2D and future 3D renderers can interpolate without touching the
 * simulation. Renderers must never mutate this data or the engine.
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
};

struct MatchRenderSnapshot
{
  std::vector<MatchRenderPlayer> players;
  MatchRenderBall ball;
  MatchState state = MatchState::KICK_OFF;
  TeamPhase homePhase = TeamPhase::SET_PIECE;
  TeamPhase awayPhase = TeamPhase::SET_PIECE;
  float transitionSecondsRemaining = 0.0f;
  int homeScore = 0;
  int awayScore = 0;
  bool goalScoredByHome = false;
  float goalCelebrationRemaining = 0.0f;
  float matchTimeMinutes = 0.0f;
  const std::vector<MatchEvent>* events = nullptr;
  const MatchStats* stats = nullptr;
  float interpolationAlpha = 0.0f;
};

/** Builds a read-only render snapshot from a live engine. */
inline MatchRenderSnapshot buildMatchRenderSnapshot(const MatchEngine& engine)
{
  MatchRenderSnapshot snapshot;
  const auto& players = engine.getPlayers();
  const auto& previousPositions = engine.getPreviousPlayerPositions();
  const auto& previousFacingAngles = engine.getPreviousPlayerFacingAngles();
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

  snapshot.state = engine.getState();
  snapshot.homePhase = engine.getHomePhase();
  snapshot.awayPhase = engine.getAwayPhase();
  snapshot.transitionSecondsRemaining = engine.getTransitionSecondsRemaining();
  snapshot.homeScore = engine.getHomeScore();
  snapshot.awayScore = engine.getAwayScore();
  snapshot.goalScoredByHome = engine.getGoalScoredByHome();
  snapshot.goalCelebrationRemaining = engine.getGoalCelebrationRemaining();
  snapshot.matchTimeMinutes = engine.getMatchTimeMinutes();
  snapshot.events = &engine.getEvents();
  snapshot.stats = &engine.getStats();
  snapshot.interpolationAlpha = engine.getInterpolationAlpha();
  return snapshot;
}

/** Linear interpolation of a 2D point used by renderers. */
inline Vector2F lerpRenderPosition(Vector2F first, Vector2F second, float alpha)
{
  return {first.x + (second.x - first.x) * alpha,
          first.y + (second.y - first.y) * alpha};
}
