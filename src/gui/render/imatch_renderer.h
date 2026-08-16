// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include "gui/render/match_render_snapshot.h"

/** Screen-space rectangle the renderer draws the pitch into. */
struct MatchViewport
{
  float x = 0.0f;
  float y = 0.0f;
  float width = 800.0f;
  float height = 500.0f;
};

/** Presentation-only switches passed to a renderer. */
struct MatchRenderOptions
{
  bool showAiDebug = false;
};

/**
 * Renderer-independent match presentation boundary.
 *
 * Implementations consume a read-only `MatchRenderSnapshot`, must never call
 * RNG or mutate the engine, and are free to interpolate between the previous
 * and current fixed-step positions using `snapshot.interpolationAlpha`.
 */
class IMatchRenderer
{
 public:
  virtual ~IMatchRenderer() = default;

  virtual void render(const MatchRenderSnapshot& snapshot,
                      const MatchRenderOptions& options,
                      const MatchViewport& viewport) = 0;
};
