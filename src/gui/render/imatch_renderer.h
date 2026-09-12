// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>

#include "gui/render/match_render_snapshot.h"

/** Screen-space rectangle the renderer draws the pitch into. */
struct MatchViewport
{
  float x = 0.0f;
  float y = 0.0f;
  float width = 800.0f;
  float height = 500.0f;
};

/** Camera presets offered by the 3D broadcast renderer. */
enum class MatchCameraMode : std::uint8_t
{
  BROADCAST,     /**< Elevated side-on TV camera tracking the ball. */
  TACTICAL,      /**< High, near top-down view of the whole pitch. */
  END,           /**< Behind the play, looking along the attack. */
  PLAYER_FOLLOW, /**< Low chase camera behind the ball carrier. */
};

/** Presentation-only switches passed to a renderer. */
struct MatchRenderOptions
{
  bool showAiDebug = false;
  /** Always label every player instead of only the ball carrier. */
  bool showPlayerNames = false;
  /** Real (unscaled) seconds since the previous rendered frame. */
  float frameSeconds = 0.0f;
  /** Mouse-wheel steps over the viewport since the previous frame. */
  float zoomSteps = 0.0f;
  MatchCameraMode cameraMode = MatchCameraMode::BROADCAST;
  /** Short team labels for the in-view score bug; may be null. */
  const char* homeLabel = nullptr;
  const char* awayLabel = nullptr;
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
