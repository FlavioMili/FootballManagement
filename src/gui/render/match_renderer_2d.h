// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <imgui.h>

#include "gui/render/imatch_renderer.h"
#include "gui/render/match_kit_colors.h"
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

 private:
  MatchKits kits;
  TeamID kitHomeTeam = 0;
  TeamID kitAwayTeam = 0;
  bool kitsChosen = false;
};

/**
 * Fits the pitch into the available screen space at its real 105 x 68 m
 * proportions (never stretched); the caller centres the result.
 */
MatchViewport computeMatchViewport(float topLeftX, float topLeftY,
                                   float availableWidth, float availableHeight);

/** Short English description of a player's current intent (tooltips). */
const char* playerIntentLabel(PlayerIntent intent);

/** Goal flash and banner centred on the viewport; shared by both views. */
void drawGoalCelebration(ImDrawList& drawList, const MatchViewport& viewport,
                         int homeScore, int awayScore,
                         float celebrationRemaining);
