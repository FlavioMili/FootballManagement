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
#include <functional>
#include <span>
#include <string>
#include <vector>

#include "global/types.h"
#include "model/match_events.h"

class MatchEngine;
struct MatchStats;

/** @brief How a shot ended (values are persisted in match analytics). */
enum class ShotOutcome : std::uint8_t
{
  Goal = 0,
  Saved,
  Blocked,
  OffTarget,
  Woodwork
};

/**
 * @brief One shot in the shooter's attacking frame: the shooter attacks
 * towards x = 1 and y = 0 is his left touchline.
 */
struct ShotRecord
{
  float minute = 0.0f;
  std::uint8_t period = 1;
  bool home = true; /*!< Shooter's side. */
  PlayerID player = 0;
  float x = 0.5f;
  float y = 0.5f;
  float xg = 0.0f;
  ShotOutcome outcome = ShotOutcome::OffTarget;
};

/**
 * Shots of the match log with their outcome, in attacking frames. A shot is
 * a goal when a goal by the same player follows before the next shot.
 */
std::vector<ShotRecord> extractShots(std::span<const MatchEvent> events);

/** Flank of the pitch seen from the attacking side (left, centre, right). */
enum class Flank : std::uint8_t
{
  Left = 0,
  Centre,
  Right
};

/** Flank of a shot in its attacking frame. */
Flank flankOf(const ShotRecord& shot);

/** @brief One side's numbers in the analysed part of the match. */
struct SideSummary
{
  int goals = 0;
  int shots = 0;
  int shots_on_target = 0;
  float xg = 0.0f;
  /** xG and shots per flank of the side's own attack. */
  std::array<float, 3> flank_xg{};
  std::array<int, 3> flank_shots{};
  int box_shots = 0;
  int set_piece_shots = 0;
  int headed_shots = 0;
  float possession = 50.0f;
  int passes_attempted = 0;
  int passes_completed = 0;
  int tackles_attempted = 0;
  int tackles_won = 0;
  int interceptions = 0;
  int aerials_won = 0;
  int aerials_lost = 0;

  float passCompletion() const;
  /** Opponent passes per defensive action (tackle attempt or
   * interception) of this side; lower means a more intense press. */
  float passesAllowedPerAction(int opponent_passes) const;
};

/** @brief A player worth mentioning. */
struct PlayerNote
{
  enum class Kind : std::uint8_t
  {
    Standout,   /*!< Rating well above the baseline. */
    Struggling, /*!< Rating well below the baseline. */
    Tired       /*!< Low live condition. */
  };
  Kind kind = Kind::Standout;
  PlayerID player = 0;
  float value = 0.0f; /*!< Rating, or condition in [0, 1]. */
};

/** @brief A localised line: key plus arguments ({0}, {1}...). */
struct AnalysisLine
{
  std::string key;
  std::vector<std::string> args;
};

/** @brief What the assistant proposes, with the evidence behind it. */
struct AnalysisSuggestion
{
  enum class Kind : std::uint8_t
  {
    SubstituteTired,
    SubstituteStruggling,
    ProtectFlank,
    AttackFlank,
    PressHigher,
    CreateMore,
    KeepBallOnGround,
    TightenUp,
    KeepGoing
  };
  Kind kind = Kind::KeepGoing;
  AnalysisLine action;
  AnalysisLine reason;
  PlayerID player = 0; /*!< Player the suggestion is about, if any. */
  int priority = 0;
};

/** @brief Facts of a live match for the assistant's analysis. */
struct AnalysisInput
{
  std::span<const MatchEvent> events;
  const MatchStats* stats = nullptr;
  std::span<const PlayerMatchStats> players;
  /** Live condition (0-1) of the managed players on the pitch. */
  std::span<const std::pair<PlayerID, float>> conditions;
  bool managed_home = true;
  float minute = 45.0f; /*!< Clock minutes played. */
  bool full_time = false;
  int substitutions_left = 0;
  /** Display name of a player (used in the lines' arguments). */
  std::function<std::string(PlayerID)> name_of;
};

/** @brief The half-time / full-time analysis. */
struct MatchAnalysis
{
  bool full_time = false;
  /** Enough shots to read patterns at all (otherwise only fatigue). */
  bool enough_data = false;
  /** Patterns rest on a small sample: present them as early signs. */
  bool tentative = true;
  /** How much evidence the analysis rests on (shown with it). */
  AnalysisLine sample;
  SideSummary own;
  SideSummary opponent;
  std::vector<AnalysisLine> observations;
  std::vector<PlayerNote> notes;
  /** Two or three suggestions, most important first. */
  std::vector<AnalysisSuggestion> suggestions;
};

namespace MatchAnalysisRules
{
/** Shots (both sides) before any pattern is read; clock time alone never
 * makes a sample. */
inline constexpr int MIN_SHOTS = 8;
/** Below this many shots patterns are only "early signs". */
inline constexpr int CONFIDENT_SHOTS = 16;
/** Share of a side's xG on one flank that counts as a pattern, from at
 * least FLANK_MIN_SHOTS shots of that side. */
inline constexpr float FLANK_SHARE = 0.55f;
inline constexpr float FLANK_MIN_XG = 0.35f;
inline constexpr int FLANK_MIN_SHOTS = 5;
/** Passes and defensive actions behind a pressing or possession claim. */
inline constexpr int PRESS_MIN_PASSES = 60;
inline constexpr int PRESS_MIN_ACTIONS = 5;
inline constexpr int CREATE_MIN_PASSES = 100;
/** Opponent shots behind a "they are getting on top" claim. */
inline constexpr int TIGHTEN_MIN_SHOTS = 6;
/** Ratings that single a player out (minimum minutes on the pitch). */
inline constexpr float STANDOUT_RATING = 7.5f;
inline constexpr float STRUGGLING_RATING = 5.6f;
inline constexpr float RATING_MIN_MINUTES = 20.0f;
/** Condition below which a player is flagged as tired. */
inline constexpr float TIRED_CONDITION = 0.55f;
/** Aerial duels needed before their share is mentioned. */
inline constexpr int MIN_AERIALS = 6;
inline constexpr float AERIAL_LOSS_SHARE = 0.65f;
/** Opponent passes per defensive action above which the press is loose. */
inline constexpr float LOOSE_PRESS = 14.0f;
inline constexpr int MAX_SUGGESTIONS = 3;
}  // namespace MatchAnalysisRules

/** Builds the analysis from the facts (pure, deterministic). */
MatchAnalysis analyseMatch(const AnalysisInput& input);

/** Collects the facts from a live engine and analyses them. */
MatchAnalysis analyseLiveMatch(
    const MatchEngine& engine, bool managed_home,
    std::function<std::string(PlayerID)> name_of);
