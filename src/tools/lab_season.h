// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>

#include "tools/lab_report.h"

/** Whole-world season runs of the balance lab. */
namespace Lab
{
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
