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
 * User camera control for one frame, already converted to world units by the
 * renderer (it owns the projection the user was looking at).
 */
struct MatchCameraControl
{
  /** Mouse-wheel steps (positive zooms in). */
  float zoomSteps = 0.0f;
  /** Orbit around the target, radians (positive yaw turns the view left). */
  float orbitYaw = 0.0f;
  /** Positive values raise the camera towards a top-down view. */
  float orbitPitch = 0.0f;
  /** Metres the free camera's target moves across the ground. */
  RenderMath::Vec3 pan;
  bool retarget = false;
  RenderMath::Vec3 retargetPoint;
  /** Free camera keeps orbiting the moving ball. */
  bool followBall = false;
  bool reset = false;
};

/**
 * Smoothly damped orbit camera with broadcast, tactical, end and chase
 * presets plus a user-driven free orbit camera.
 *
 * The camera is described by a look-at target plus yaw, pitch, distance and
 * vertical field of view. Each preset produces a desired rig from the focus
 * and every parameter is damped exponentially with real frame time, so the
 * motion is identical at any frame rate and mode switches glide instead of
 * cutting. The free camera starts from whatever pose was on screen when it
 * was selected, so a drag in any preset takes over seamlessly; its pitch,
 * distance and target are clamped so it never dips under the pitch, leaves
 * the stadium bowl or loses the pitch.
 */
class MatchCamera3D
{
 public:
  /** Advances the camera towards the preset's desired framing. */
  void update(const MatchCameraFocus& focus, MatchCameraMode mode,
              const MatchCameraControl& control, float deltaSeconds);

  /** Jumps straight to the desired framing (first frame, tests). */
  void snap(const MatchCameraFocus& focus, MatchCameraMode mode);

  RenderMath::Vec3 eye() const;
  RenderMath::Vec3 target() const { return current.target; }
  float verticalFov() const { return current.fov; }
  float zoom() const { return zoomFactor; }
  /** Current orbit parameters (radians, metres). */
  float yaw() const { return current.yaw; }
  float pitch() const { return current.pitch; }
  float distance() const { return current.distance; }
  /** Where the free camera is heading (after clamps), for tests. */
  RenderMath::Vec3 freeTarget() const { return freeRig.target; }
  float freePitch() const { return freeRig.pitch; }
  float freeDistance() const { return freeRig.distance; }

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
  /** Applies one frame of user control to the free rig and clamps it. */
  void steerFree(const MatchCameraFocus& focus,
                 const MatchCameraControl& control);

  Rig current;
  /** Desired pose of the free camera (the user's, before bowl clamping). */
  Rig freeRig;
  float zoomFactor = 1.0f;
  float chaseYaw = 0.0f;
  bool initialized = false;
  bool freeActive = false;
};
