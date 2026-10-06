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
#include <iosfwd>
#include <string>
#include <vector>

#include "tools/lab_report.h"

/** Whole-world season runs of the balance lab. */
namespace Lab
{
/** Health of one division tier at the end of a season (all its clubs). */
struct TierVitals
{
  std::size_t clubs = 0;
  double median_cash = 0.0;    /*!< EUR on 30 June. */
  double negative_share = 0.0; /*!< Clubs with negative cash, 30 June. */
  /** Highest share with negative cash on the first day of any month. */
  double peak_negative_share = 0.0;
  /** Cash below -25% of the season's revenue on 30 June. */
  double insolvent_share = 0.0;
  double wage_revenue = 0.0;     /*!< Pooled player wages / revenue. */
  double owner_investment = 0.0; /*!< Owner money injected (EUR). */
  double mean_revenue = 0.0;     /*!< EUR per club. */
  double mean_reputation = 0.0;
  double reputation_sd = 0.0;
  double mean_overall = 0.0; /*!< Senior club players. */
};

/** World state after one simulated season (the soak trend). */
struct SeasonVitals
{
  int season = 0;
  // Population on 30 June.
  std::size_t players = 0;
  std::size_t club_seniors = 0;
  std::size_t academy = 0;
  std::size_t free_agents = 0;
  double free_agent_mean_age = 0.0;
  /** Club players (seniors and academy) aged <= 18, 19-23, 24-29, 30-33, 34+.
   */
  std::array<std::size_t, 5> age_bands{};
  /** Players created / removed between 1 July and the next rollover. */
  std::size_t intake = 0;
  std::size_t removed = 0;
  // Ability of senior club players.
  double mean_overall = 0.0;
  double p90_overall = 0.0;
  double top100_overall = 0.0;
  double mean_potential = 0.0;
  std::array<TierVitals, 2> tiers{};
  double wage_position_r2 = 0.0;
  double wage_position_r2_top = 0.0; /*!< Top divisions only (CT-W28). */
  // Fee-paying transfers.
  std::size_t fee_moves = 0;
  double median_fee = 0.0;
  double mean_fee = 0.0;
  double max_fee = 0.0;
  std::size_t moves = 0; /*!< All moves except releases and loan returns. */
  // Competitions decided this season.
  std::size_t leagues_completed = 0;
  std::size_t promoted = 0;
  std::size_t relegated = 0;
  std::size_t cups_completed = 0;
  std::size_t continental_completed = 0;
  std::size_t continental_competitions = 0;
  std::size_t national_honours = 0;
  // Cost of the career.
  double save_mb = 0.0;
  double save_ms = 0.0;
  double load_ms = 0.0; /*!< 0 unless SeasonOptions::reload. */
  double day_ms_mean = 0.0;
  double day_ms_p95 = 0.0;
  double day_ms_max = 0.0;
  double season_seconds = 0.0;
};

struct SeasonOptions
{
  std::uint64_t seed = 1;
  int seasons = 1;
  unsigned threads = 4;
  /** "all", "top" (tier 1 only) or a comma-separated list of league IDs. */
  std::string leagues = "all";
  /** Safety cap: a season taking more days than this is a failure. */
  int max_days_per_season = 400;
  /** Progress lines (every 30 days) go here when set. */
  std::ostream* progress = nullptr;
  /** Save and reload the career after every season (soak runs). */
  bool reload = false;
  /** Receives one SeasonVitals per season when set. */
  std::vector<SeasonVitals>* vitals = nullptr;
};

/**
 * Creates a new AI-only career (no managed club) in the current runtime
 * root, which the caller must point at a scratch directory
 * (FM_RUNTIME_ROOT), and simulates `seasons` full seasons through
 * GameController::advanceDay(). Throws std::invalid_argument for a bad
 * league filter or season count and std::runtime_error when a season does
 * not complete.
 */
LabReport runSeasons(const SeasonOptions& options);
}  // namespace Lab
