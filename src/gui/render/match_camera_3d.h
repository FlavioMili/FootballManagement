// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include "gui/render/imatch_renderer.h"
#include "gui/render/match_render_math.h"

/** What the camera should look at this frame, in metric world space. */
struct MatchCameraFocus
{
  RenderMath::Vec3 ball;
  /** Ball ground velocity in metres per rendered second. */
  RenderMath::Vec3 ballVelocity;
  RenderMath::Vec3 carrier;
  float carrierYaw = 0.0f;
  bool hasCarrier = false;
  /** +1 when the attacking side plays towards +x, -1 otherwise. */
  float attackDirection = 1.0f;
};

/**
 * Smoothly damped orbit camera with broadcast, tactical, end and chase
 * presets.
 *
 * The camera is described by a look-at target plus yaw, pitch, distance and
 * vertical field of view. Each preset produces a desired rig from the focus
 * and every parameter is damped exponentially with real frame time, so the
 * motion is identical at any frame rate and mode switches glide instead of
 * cutting.
 */
class MatchCamera3D
{
 public:
  /** Advances the camera towards the preset's desired framing. */
  void update(const MatchCameraFocus& focus, MatchCameraMode mode,
              float zoomSteps, float deltaSeconds);

  /** Jumps straight to the desired framing (first frame, tests). */
  void snap(const MatchCameraFocus& focus, MatchCameraMode mode);

  RenderMath::Vec3 eye() const;
  RenderMath::Vec3 target() const { return current.target; }
  float verticalFov() const { return current.fov; }
  float zoom() const { return zoomFactor; }

 private:
  struct Rig
  {
    RenderMath::Vec3 target;
    float yaw = 0.0f;
    float pitch = 0.0f;
    float distance = 1.0f;
    float fov = 0.5f;
  };

  Rig desiredRig(const MatchCameraFocus& focus, MatchCameraMode mode) const;

  Rig current;
  float zoomFactor = 1.0f;
  float chaseYaw = 0.0f;
  bool initialized = false;
};
