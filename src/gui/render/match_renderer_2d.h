// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include "gui/render/imatch_renderer.h"
#include "gui/scenes/match_scene_tuning.h"

/**
 * ImGui/SDL-agnostic 2D match renderer.
 *
 * Draws pitch markings, player markers, ball, labels, and the optional AI
 * movement-target overlay into the current ImGui draw list. Consumes only the
 * read-only snapshot and never touches the simulation.
 */
class MatchRenderer2D final : public IMatchRenderer
{
 public:
  void render(const MatchRenderSnapshot& snapshot,
              const MatchRenderOptions& options,
              const MatchViewport& viewport) override;
};

/**
 * Fits the pitch into the available screen space while preserving its aspect
 * ratio and applying the tuning scale limits.
 */
MatchViewport computeMatchViewport(float topLeftX, float topLeftY,
                                   float availableWidth, float availableHeight);
