// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include "model/match_tuning.h"

/**
 * Per-match balance settings, independent of the engine's mutable state.
 * Defaults preserve the calibrated engine. MatchSimulation::leagueContext()
 * derives these from a league; headless callers may supply an explicit
 * MatchSimulationInput::context_override for an experiment or custom ruleset.
 *
 * MatchEngine::setMatchContext() accepts settings only before the first step,
 * clamps finite values to MatchTuning::Context bounds and replaces non-finite
 * values with defaults. Keep this value in captured match inputs, not in a
 * mutable global: concurrent fixtures and highlight copies must stay isolated.
 * See docs/development/extending-and-modding.md for examples and limitations.
 */
struct MatchContext
{
  /** Finishing precision: above 1 sharpens shots, below 1 makes them less
   * accurate. Changes execution, not the score or the xG formula directly. */
  float goalRateScale = 1.0F;
  /** Mean and spread of the referee strictness drawn at kick-off. */
  float refereeStrictnessMean = 1.0F;
  float refereeStrictnessSd = MatchTuning::Discipline::STRICTNESS_SD;
  /** Scales the home edge on attributes, execution and refereeing;
   * 0 represents a neutral venue. */
  float homeAdvantageScale = 1.0F;
};
