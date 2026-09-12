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
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "global/types.h"
#include "model/match_analysis.h"
#include "model/match_tracking.h"

class MatchEngine;
struct MatchReport;

/** @brief Tracked numbers of one player in a match (attacking frame). */
struct DetailPlayer
{
  PlayerID player = 0;
  bool home = true;
  std::uint16_t touches = 0;
  /** Mean touch position; meaningless without touches. */
  float avg_x = 0.5f;
  float avg_y = 0.5f;
  std::uint16_t progressive_passes = 0;
  std::uint16_t pressures = 0;
  float expected_assists = 0.0f;
  /** Touches per grid cell (MatchTracking::cellOf), capped at 255. */
  std::array<std::uint8_t, MatchTracking::CELLS> cells{};
};

/** @brief Completed passes from one player to a team-mate. */
struct PassLink
{
  std::uint8_t from = 0; /*!< Index into MatchDetail::players. */
  std::uint8_t to = 0;
  std::uint16_t count = 0;
};

/** @brief A substitution of the match. */
struct DetailSubstitution
{
  std::uint8_t minute = 0; /*!< Clock minute (added time counts on). */
  bool home = true;
  PlayerID off = 0;
  PlayerID on = 0;
};

/**
 * @brief Per-match aggregates behind the match report's analysis: touch
 * maps, the pass network, progressive passes, pressures, expected assists,
 * final-third pressure over time and the substitutions, for both sides.
 * Stored as a compact binary blob next to the managed match's snapshot.
 */
struct MatchDetail
{
  std::vector<DetailPlayer> players;
  std::vector<PassLink> links;
  /** Final-third touches per five-minute bucket, index 0 home. */
  std::array<std::array<std::uint8_t, MatchTracking::BUCKETS>, 2>
      final_third{};
  std::vector<DetailSubstitution> substitutions;

  [[nodiscard]] bool empty() const { return players.empty(); }
  /** Index of a player in players, or nullopt. */
  [[nodiscard]] std::optional<std::size_t> indexOf(PlayerID player) const;

  /** Versioned binary encoding (varints, sparse cells). */
  [[nodiscard]] std::vector<std::uint8_t> encode() const;
  /** nullopt for malformed or unknown data. */
  static std::optional<MatchDetail> decode(std::span<const std::uint8_t> data);
};

/** Builds the detail of a finished (or running) match from its tracker. */
MatchDetail captureDetail(const MatchEngine& engine);

/** @brief One entry of the key-moments list. */
struct KeyMoment
{
  enum class Kind : std::uint8_t
  {
    Goal,
    OwnGoal,
    BigChance, /*!< A missed shot of at least BIG_CHANCE_XG. */
    Woodwork,
    Yellow,
    SecondYellow,
    Red,
    Substitution
  };
  Kind kind = Kind::Goal;
  std::uint8_t minute = 0; /*!< Clock minute. */
  std::uint8_t added = 0;  /*!< Minutes into added time (0 = none). */
  /** Side the moment counts for (an own goal counts for the other side). */
  bool home = true;
  PlayerID player = 0;
  PlayerID other = 0; /*!< Assist, or the player substituted off. */
  float xg = 0.0f;
  /** Index into the snapshot's shots, or -1. */
  int shot = -1;
};

namespace MatchInsights
{
inline constexpr float BIG_CHANCE_XG = 0.3f;
/** Gap between the sides' xG that makes one side the better one. */
inline constexpr float CLEAR_XG_GAP = 0.6f;
/** Goals minus xG that counts as clinical or wasteful finishing. */
inline constexpr float FINISHING_GAP = 1.0f;
/** Final-third touches behind any flank or spell claim. */
inline constexpr int MIN_FLANK_TOUCHES = 24;
inline constexpr float FLANK_SHARE = 0.45f;
inline constexpr int MIN_SPELL_TOUCHES = 12;
/** Average xG per shot below which shots count as speculative. */
inline constexpr float LOW_SHOT_QUALITY = 0.075f;
inline constexpr int MIN_SHOTS_FOR_QUALITY = 10;
inline constexpr int MAX_SUMMARY_LINES = 5;
}  // namespace MatchInsights

/** @brief What the summary and the key moments are built from. */
struct InsightInput
{
  const MatchReport* report = nullptr;
  std::span<const ShotRecord> shots;
  const MatchDetail* detail = nullptr; /*!< May be empty (not tracked). */
  bool managed_home = true;
  /** Display name of a player (used in the lines' arguments). */
  std::function<std::string(PlayerID)> name_of;
};

/** Goals, cards, big chances, woodwork and substitutions, by minute. */
std::vector<KeyMoment> keyMoments(const InsightInput& input);

/**
 * A few lines explaining the result from the managed side's point of view:
 * the xG balance, finishing at both ends, shot quality, the flanks where
 * the attacks came from, the strongest spell, set pieces and the main
 * passing connection. Pure and deterministic; sentences hedge on what a
 * single match can show.
 */
std::vector<AnalysisLine> summariseMatch(const InsightInput& input);

/** @brief Final-third touches of one side split by flank (attacking frame). */
std::array<int, 3> finalThirdByFlank(const MatchDetail& detail, bool home);
