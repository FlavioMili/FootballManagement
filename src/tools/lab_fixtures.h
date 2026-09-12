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
#include <memory>
#include <string_view>
#include <vector>

#include "global/stats_config.h"
#include "global/types.h"
#include "model/lineup.h"
#include "model/strategy.h"

class Player;

/** Synthetic squads and tactic presets for isolated lab matches. */
namespace Lab
{
/**
 * The game's stats configuration (assets/config/stats_config.json), so lab
 * players rate exactly like generated ones; the calibration-test constants
 * when the file cannot be read.
 */
StatsConfig loadStatsConfig();

/**
 * 4-4-2 matchday squad (eleven starters, seven reserves) whose attributes
 * are role-shaped offsets around `rating` with a small deterministic
 * per-player spread. Same construction as test/test_match_calibration.cpp
 * (createCalibrationTeam), duplicated so the lab does not depend on tests,
 * except that the spread depends on the squad slot only: two squads of the
 * same rating are identical (the test's ID-based spread makes team 1 about
 * 2.3 attribute points stronger than team 2).
 * Players are owned by `pool` and get IDs team_id * 100 + index.
 */
Lineup buildLabLineup(TeamID team_id, float rating,
                      std::vector<std::unique_ptr<Player>>& pool);

/** Mean overall of the eleven starters (the contract's team rating). */
float lineupRating(const Lineup& lineup, const StatsConfig& config);

struct TacticPreset
{
  std::string_view name;
  StrategySliders sliders;
};

/**
 * The four presets of the tactics screen (gui/scenes/strategy_scene.cpp,
 * PRESETS). Duplicated because the GUI table lives in an anonymous
 * namespace of fm_ui; keep both in sync.
 */
inline constexpr std::array<TacticPreset, 4> TACTIC_PRESETS{{
    {"Balanced", {0.50f, 0.50f, 0.50f, 0.50f, 0.50f}},
    {"Front foot", {0.82f, 0.72f, 0.76f, 0.68f, 0.64f}},
    {"Counter", {0.38f, 0.68f, 0.62f, 0.58f, 0.42f}},
    {"Control", {0.62f, 0.34f, 0.44f, 0.72f, 0.72f}},
}};
}  // namespace Lab
