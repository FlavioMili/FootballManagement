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

/** What the play-mode overlay shows this frame (presentation only). */
struct MatchPlayOverlayState
{
  /** The human's active footballer and who a switch would pick next. */
  PlayerID activePlayer = 0;
  PlayerID nextSwitch = 0;
  /** Filled share of the power bar while a button is held. */
  float power = 0.0f;
  bool charging = false;
  /** Radar dot colours of the two sides. */
  ImU32 homeColor = IM_COL32(220, 60, 60, 255);
  ImU32 awayColor = IM_COL32(60, 110, 220, 255);
};

/**
 * Play-mode overlay over a rendered match view: a ring under the active
 * footballer with his name and condition above him, the power bar while a
 * button is held, a lighter ring on the next switch candidate, an arrow at
 * the view's edge when the active footballer is out of the picture, and a
 * radar of the whole pitch at the bottom of the view. Draws through the
 * renderer's own projection, so it fits the 2D and the 3D view alike, and
 * allocates nothing.
 */
void drawMatchPlayOverlay(ImDrawList& drawList, const IMatchRenderer& renderer,
                          const MatchRenderSnapshot& snapshot,
                          const MatchPlayOverlayState& state, ImVec2 viewMin,
                          ImVec2 viewMax);
