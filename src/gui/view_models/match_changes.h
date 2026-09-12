// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "global/stats_config.h"
#include "global/types.h"
#include "gui/view_models/formation.h"
#include "model/match_engine.h"
#include "model/strategy.h"

class Player;

/**
 * @brief Touchline decisions of the live match, independent of the widgets:
 * planned substitutions made together at a stoppage, the assistant's
 * suggestions, tactical presets, formation changes and the shouts on offer.
 *
 * Every change goes through the engine, which owns the rules; the plan only
 * explains in advance why a change would be refused.
 */
namespace MatchChanges
{

/** @brief Why a change cannot be made (NONE when it can). */
enum class Refusal : std::uint8_t
{
  NONE,
  MATCH_OVER,
  /** Every substitution is used (counting the planned ones). */
  LIMIT,
  /** Every substitution window is used (half-time changes are free). */
  WINDOWS,
  /** The engine does not allow a change at this moment (shootout). */
  NOT_NOW,
  /** The outgoing player is no longer on the pitch. */
  NOT_ON_PITCH,
  /** The incoming player is not among the substitutes. */
  NOT_ON_BENCH,
  /** The substitute has already played (no re-entry). */
  ALREADY_PLAYED,
  /** One of the two players is already part of a planned change. */
  ALREADY_PLANNED
};

/** @brief Language key explaining a refusal (LIMIT and WINDOWS take %d). */
const char* refusalKey(Refusal refusal);

/** @brief Substitutions a side may make in this match (one more in ET). */
int maxSubstitutions(const MatchEngine& engine);

/** @brief Substitution windows of a side in this match (one more in ET). */
int maxWindows(const MatchEngine& engine);

/**
 * @brief Refusal from the match rules alone (limit, windows, full time) for
 * one more change when @p planned changes are already waiting.
 */
Refusal rulesRefusal(const MatchEngine& engine, bool home, int planned);

/** @brief Play is stopped: planned changes can be made without waiting. */
bool atStoppage(const MatchEngine& engine);

/** @brief One planned substitution. */
struct PlannedSubstitution
{
  PlayerID out = 0;
  PlayerID in = 0;
};

/** @brief Result of one planned change once it was attempted. */
struct SubstitutionOutcome
{
  PlannedSubstitution change;
  Refusal refusal = Refusal::NONE;
};

/**
 * @brief Substitutions queued by the manager and made together, so they
 * share one stoppage and one window.
 */
class SubstitutionPlan
{
 public:
  /**
   * @brief Whether @p out can be replaced by @p in given the rules and the
   * changes already planned (nothing is added).
   * @param bench The side's substitutes (the matchday squad's reserves).
   */
  [[nodiscard]] Refusal check(const MatchEngine& engine, bool home,
                              std::span<const Player* const> bench,
                              PlayerID out, PlayerID in) const;

  /** @brief check() and, when allowed, appends the change. */
  Refusal add(const MatchEngine& engine, bool home,
              std::span<const Player* const> bench, PlayerID out, PlayerID in);

  /** @brief Drops one planned change (false for a bad index). */
  bool remove(std::size_t index);

  /** @brief Drops every planned change and the confirmation. */
  void clear();

  [[nodiscard]] std::span<const PlannedSubstitution> entries() const
  {
    return planned;
  }
  [[nodiscard]] bool empty() const { return planned.empty(); }
  /** @brief The player is leaving or coming on in a planned change. */
  [[nodiscard]] bool involves(PlayerID player) const;
  /** @brief Planned replacement of an outgoing player (0 when none). */
  [[nodiscard]] PlayerID replacementFor(PlayerID out) const;

  /** @brief Hands the plan to the referee: made at the next stoppage. */
  void confirm() { confirmed = !planned.empty(); }
  [[nodiscard]] bool isConfirmed() const { return confirmed; }

  /**
   * @brief Makes every planned change now, in order, without advancing the
   * engine in between (so they count as one window), then clears the plan.
   * Changes the engine refuses carry the reason.
   */
  std::vector<SubstitutionOutcome> apply(MatchEngine& engine, bool home,
                                         std::span<const Player* const> bench);

  /**
   * @brief apply() once the plan is confirmed and play has stopped;
   * nothing (an empty result) otherwise.
   */
  std::vector<SubstitutionOutcome> applyIfDue(
      MatchEngine& engine, bool home, std::span<const Player* const> bench);

 private:
  std::vector<PlannedSubstitution> planned;
  bool confirmed = false;
};

/** @brief Reasons behind a suggested substitution (bit flags). */
enum SuggestionReason : std::uint8_t
{
  SUGGEST_INJURED = 1U << 0U,
  SUGGEST_TIRED = 1U << 1U,
  SUGGEST_BOOKED = 1U << 2U
};

/** @brief The assistant's proposal: replace @p out with @p in. */
struct Suggestion
{
  PlayerID out = 0;
  PlayerID in = 0;
  std::uint8_t reasons = 0;
  /** Condition of the outgoing player in [0, 1]. */
  float condition = 1.0f;
};

/**
 * @brief Up to @p maximum substitutions the assistant would make now:
 * injured players first, then tired ones and booked defenders or
 * midfielders late in the game, each with the best free substitute for the
 * position (goalkeepers only for goalkeepers). Players already in the plan
 * are left out; nothing is suggested when no change is allowed.
 */
std::vector<Suggestion> suggestSubstitutions(
    const MatchEngine& engine, bool home, std::span<const Player* const> bench,
    const SubstitutionPlan& plan, const StatsConfig& config,
    std::size_t maximum);

/**
 * @brief Suitability of a substitute for the outgoing player's position in
 * [0, 1] (the natural role fit, see Formation::roleFit).
 */
float positionFit(PlayerRole substitute, PlayerRole position);

/** @brief A named set of the five strategy sliders. */
struct TacticPreset
{
  const char* nameKey;
  const char* descriptionKey;
  StrategySliders sliders;
};

/** @brief Balanced, front foot, counter and control. */
inline constexpr std::array<TacticPreset, 4> TACTIC_PRESETS{{
    {"TACTIC_BALANCED",
     "TACTIC_BALANCED_HELP",
     {0.50f, 0.50f, 0.50f, 0.50f, 0.50f}},
    {"TACTIC_FRONT_FOOT",
     "TACTIC_FRONT_FOOT_HELP",
     {0.82f, 0.72f, 0.76f, 0.68f, 0.64f}},
    {"TACTIC_COUNTER",
     "TACTIC_COUNTER_HELP",
     {0.38f, 0.68f, 0.62f, 0.58f, 0.42f}},
    {"TACTIC_CONTROL",
     "TACTIC_CONTROL_HELP",
     {0.62f, 0.34f, 0.44f, 0.72f, 0.72f}},
}};

/** @brief One strategy slider: label, help and the member it edits. */
struct SliderInfo
{
  const char* key;
  const char* id;
  float StrategySliders::* value;
  const char* helpKey;
};

inline constexpr std::array<SliderInfo, 5> TACTIC_SLIDERS{{
    {"STRATEGY_PRESSING", "pressing", &StrategySliders::pressing,
     "TACTIC_PRESSING_HELP"},
    {"STRATEGY_RISK_TAKING", "risk", &StrategySliders::riskTaking,
     "TACTIC_RISK_HELP"},
    {"STRATEGY_OFFENSIVE_BIAS", "offensive", &StrategySliders::offensiveBias,
     "TACTIC_OFFENSIVE_HELP"},
    {"STRATEGY_WIDTH_USAGE", "width", &StrategySliders::widthUsage,
     "TACTIC_WIDTH_HELP"},
    {"STRATEGY_COMPACTNESS", "compactness", &StrategySliders::compactness,
     "TACTIC_COMPACTNESS_HELP"},
}};

/** @brief Every slider within a small tolerance. */
bool sameSliders(const StrategySliders& left, const StrategySliders& right);

/** @brief Index of the preset with these sliders, or -1 (custom). */
int detectTacticPreset(const StrategySliders& sliders);

/** @brief Index of the formation preset with this shape, or -1 (custom). */
int detectFormation(std::span<const Vector2F> shape);

/**
 * @brief The side's current shape rearranged as @p preset: each occupied
 * slot gets the preset position that best suits its player (natural role
 * first, then the nearest spot), empty slots take what is left. Returns the
 * current shape unchanged when it does not have ten slots.
 */
std::vector<Vector2F> formationFor(const MatchEngine& engine, bool home,
                                   const Formation::Preset& preset);

/**
 * @brief Tactical familiarity the side gives up for a while when its shape
 * changes from @p from to @p to, in [0, 1] (the engine's reshape rule).
 */
float reshapeCost(std::span<const Vector2F> from, std::span<const Vector2F> to);

/** @brief One touchline shout: its labels and keyboard slot. */
struct ShoutInfo
{
  MatchShout shout;
  const char* labelKey;
  const char* hintKey;
};

/** @brief Every shout, in bar order (Shift+1 .. Shift+0, Shift+-). */
inline constexpr std::array<ShoutInfo, 11> SHOUTS{{
    {MatchShout::ENCOURAGE, "SHOUT_ENCOURAGE", "SHOUT_ENCOURAGE_HINT"},
    {MatchShout::DEMAND_MORE, "SHOUT_DEMAND_MORE", "SHOUT_DEMAND_MORE_HINT"},
    {MatchShout::CALM_DOWN, "SHOUT_CALM_DOWN", "SHOUT_CALM_DOWN_HINT"},
    {MatchShout::PUSH_HIGHER, "SHOUT_PUSH_HIGHER", "SHOUT_PUSH_HIGHER_HINT"},
    {MatchShout::DROP_DEEPER, "SHOUT_DROP_DEEPER", "SHOUT_DROP_DEEPER_HINT"},
    {MatchShout::PRESS_MORE, "SHOUT_PRESS_MORE", "SHOUT_PRESS_MORE_HINT"},
    {MatchShout::STAND_OFF, "SHOUT_STAND_OFF", "SHOUT_STAND_OFF_HINT"},
    {MatchShout::KEEP_POSSESSION, "SHOUT_KEEP_POSSESSION",
     "SHOUT_KEEP_POSSESSION_HINT"},
    {MatchShout::HIT_ON_COUNTER, "SHOUT_HIT_ON_COUNTER",
     "SHOUT_HIT_ON_COUNTER_HINT"},
    {MatchShout::WORK_BALL_INTO_BOX, "SHOUT_WORK_BALL_INTO_BOX",
     "SHOUT_WORK_BALL_INTO_BOX_HINT"},
    {MatchShout::SHOOT_ON_SIGHT, "SHOUT_SHOOT_ON_SIGHT",
     "SHOUT_SHOOT_ON_SIGHT_HINT"},
}};

}  // namespace MatchChanges
