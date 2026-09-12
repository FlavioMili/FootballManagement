// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "global/types.h"
#include "model/match_tuning.h"

/** Machine-readable category of a match event. */
enum class MatchEventType : std::uint8_t
{
  INFO,
  KICK_OFF,
  GOAL,
  OWN_GOAL,
  SHOT,
  SAVE,
  SHOT_BLOCKED,
  SHOT_OFF_TARGET,
  WOODWORK,
  FOUL,
  ADVANTAGE,
  YELLOW_CARD,
  SECOND_YELLOW,
  RED_CARD,
  INJURY,
  SUBSTITUTION,
  OFFSIDE,
  CORNER,
  FREE_KICK,
  PENALTY,
  PENALTY_MISSED,
  THROW_IN,
  GOAL_KICK,
  ADDED_TIME,
  HALF_TIME,
  SECOND_HALF,
  FULL_TIME
};

std::string_view matchEventTypeName(MatchEventType type);

/**
 * One entry of the match log. `description` stays human-readable for the live
 * view; the remaining fields make the log usable by the result pipeline.
 *
 * `timeMinute` is the match clock: the first half runs from 0 to 45 plus its
 * added time, the second half restarts at 45. `addedMinute` is the part of
 * the clock beyond the regulation end of the period (0 when not in added
 * time), so "45+2'" is timeMinute 47 in period 1 with addedMinute 2.
 * `hasTeam`/`isHomeTeam` describe the side of the primary player (for an
 * OWN_GOAL that is the defender's side; the goal counts for the other side).
 * For goals the secondary player is the assist provider, for fouls the
 * fouled player, for substitutions the outgoing player and for saves the
 * shooter.
 */
struct MatchEvent
{
  float timeMinute = 0.0f;
  std::string description;
  MatchEventType type = MatchEventType::INFO;
  int period = 1;
  float addedMinute = 0.0f;
  bool hasTeam = false;
  bool isHomeTeam = false;
  PlayerID primaryPlayerId = 0;
  PlayerID secondaryPlayerId = 0;
  Vector2F position{MatchTuning::Pitch::CENTRE, MatchTuning::Pitch::CENTRE};
  float xg = 0.0f;
};

/**
 * Per-player match statistics. One entry exists for every player who took
 * part, including substitutes and players who left the pitch; the list is
 * never truncated, so it is the authoritative source for post-match data.
 */
struct PlayerMatchStats
{
  PlayerID playerId = 0;
  bool isHomeTeam = false;
  PlayerRole role = PlayerRole::UNKNOWN;
  bool started = false;
  bool substitutedOn = false;
  bool substitutedOff = false;
  bool sentOff = false;
  bool injured = false;
  float minutesPlayed = 0.0f;
  int goals = 0;
  int ownGoals = 0;
  int assists = 0;
  int shots = 0;
  int shotsOnTarget = 0;
  float expectedGoals = 0.0f;
  int passesAttempted = 0;
  int passesCompleted = 0;
  int keyPasses = 0;
  int tacklesAttempted = 0;
  int tacklesWon = 0;
  int interceptions = 0;
  int clearances = 0;
  int aerialDuelsWon = 0;
  int aerialDuelsLost = 0;
  int saves = 0;
  int goalsConceded = 0;
  int foulsCommitted = 0;
  int foulsSuffered = 0;
  int yellowCards = 0;
  int redCards = 0;
  /** Estimated real-match distance (see MatchTuning::Units). */
  float distanceMetres = 0.0f;
  float secondHalfDistanceMetres = 0.0f;
  /** Physical condition in [0, 1] when the player last left or at the end. */
  float condition = 1.0f;
  /** Event-driven rating, 6.0 baseline, bounded to [3, 10]. */
  float rating = MatchTuning::Rating::BASELINE;
};

enum class SubstitutionReason : std::uint8_t
{
  MANUAL,
  FATIGUE,
  INJURY,
  TACTICAL,
  CARD_RISK,
  GOALKEEPER_REPLACEMENT
};

struct MatchSubstitution
{
  float timeMinute = 0.0f;
  int period = 1;
  bool isHomeTeam = false;
  PlayerID outgoingPlayerId = 0;
  PlayerID incomingPlayerId = 0;
  SubstitutionReason reason = SubstitutionReason::MANUAL;
};
