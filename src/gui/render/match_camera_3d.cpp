// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "gui/render/match_camera_3d.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "gui/render/match_render_3d_tuning.h"

namespace
{
using RenderMath::Vec3;
using Tuning = MatchRender3DTuning;

constexpr float PITCH_LENGTH = MatchTuning::Pitch::LENGTH_METRES;
constexpr float PITCH_WIDTH = MatchTuning::Pitch::WIDTH_METRES;
constexpr float HALF_LENGTH = PITCH_LENGTH * 0.5f;
constexpr float HALF_WIDTH = PITCH_WIDTH * 0.5f;

Vec3 viewDirection(float yaw, float pitch)
{
  return {std::cos(pitch) * std::cos(yaw), std::cos(pitch) * std::sin(yaw),
          -std::sin(pitch)};
}

Vec3 lookAhead(const MatchCameraFocus& focus)
{
  Vec3 ahead = focus.ballVelocity * Tuning::Camera::LOOKAHEAD_SECONDS;
  ahead.z = 0.0f;
  const float aheadLength = RenderMath::length(ahead);
  if (aheadLength > Tuning::Camera::MAX_LOOKAHEAD)
    ahead = ahead * (Tuning::Camera::MAX_LOOKAHEAD / aheadLength);
  return ahead;
}

/// The chase camera looks where the carrier runs, or along the attack when
/// the ball is loose.
float chaseHeading(const MatchCameraFocus& focus)
{
  if (focus.hasCarrier) return focus.carrierYaw;
  return focus.attackDirection >= 0.0f ? 0.0f : std::numbers::pi_v<float>;
}

/// Keeps a low eye inside the stadium bowl so no preset ends up inside a
/// stand: the camera first tilts down (keeping its distance), and only moves
/// closer once it is as steep as allowed. Eyes above the roofs are free.
void clampToBowl(Vec3 target, float yaw, float& pitch, float& distance)
{
  const Vec3 flat{std::cos(yaw), std::sin(yaw), 0.0f};
  const auto horizontalReach = [&]()
  {
    float reach = distance * std::cos(pitch);
    const auto limit =
        [&](float origin, float step, float minimum, float maximum)
    {
      if (step > 1e-4f) reach = std::min(reach, (origin - minimum) / step);
      if (step < -1e-4f) reach = std::min(reach, (origin - maximum) / step);
    };
    limit(target.x, flat.x, Tuning::Camera::EYE_MIN_X,
          Tuning::Camera::EYE_MAX_X);
    limit(target.y, flat.y, Tuning::Camera::EYE_MIN_Y,
          Tuning::Camera::EYE_MAX_Y);
    return std::max(reach, 0.0f);
  };
  if (target.z + std::sin(pitch) * distance >=
      Tuning::Camera::STAND_CLEAR_HEIGHT)
    return;
  const float reach = horizontalReach();
  if (reach >= distance * std::cos(pitch) - 1e-4f) return;
  pitch = std::min(std::acos(std::clamp(reach / distance, 0.0f, 1.0f)),
                   Tuning::Camera::MAX_CLAMP_PITCH);
  distance = std::max(std::min(distance, reach / std::cos(pitch)), 1.0f);
}
}  // namespace

MatchCamera3D::Rig MatchCamera3D::desiredRig(const MatchCameraFocus& focus,
                                             MatchCameraMode mode) const
{
  const Vec3 ahead = lookAhead(focus);
  Rig rig;
  switch (mode)
  {
    case MatchCameraMode::BROADCAST:
    {
      rig.target = {
          std::clamp(focus.ball.x + ahead.x, Tuning::Broadcast::TARGET_MIN_X,
                     Tuning::Broadcast::TARGET_MAX_X),
          std::clamp(HALF_WIDTH + (focus.ball.y + ahead.y - HALF_WIDTH) *
                                      Tuning::Broadcast::TARGET_WIDTH_FOLLOW,
                     Tuning::Broadcast::TARGET_MIN_Y,
                     Tuning::Broadcast::TARGET_MAX_Y),
          0.0f};
      // The gantry camera slides only part of the way along its rail and
      // pans for the rest, like a real main camera.
      const Vec3 eye{HALF_LENGTH + (rig.target.x - HALF_LENGTH) *
                                       Tuning::Broadcast::RAIL_FOLLOW,
                     Tuning::Broadcast::EYE_Y, Tuning::Broadcast::EYE_HEIGHT};
      const Vec3 offset = rig.target - eye;
      rig.distance = RenderMath::length(offset);
      rig.yaw = std::atan2(offset.y, offset.x);
      rig.pitch = std::asin(-offset.z / rig.distance);
      rig.fov = Tuning::Broadcast::FOV * zoomFactor;
      return rig;
    }
    case MatchCameraMode::TACTICAL:
      rig.target = {HALF_LENGTH + (focus.ball.x - HALF_LENGTH) *
                                      Tuning::Tactical::LENGTH_FOLLOW,
                    HALF_WIDTH + Tuning::Tactical::TARGET_Y_OFFSET, 0.0f};
      rig.yaw = std::numbers::pi_v<float> * 0.5f;
      rig.pitch = Tuning::Tactical::PITCH;
      rig.distance = Tuning::Tactical::DISTANCE * zoomFactor;
      rig.fov = Tuning::Tactical::FOV;
      break;
    case MatchCameraMode::END:
    {
      const float direction = focus.attackDirection >= 0.0f ? 1.0f : -1.0f;
      rig.target = {
          std::clamp(focus.ball.x + ahead.x + direction * Tuning::End::LEAD,
                     Tuning::End::TARGET_MIN_X, Tuning::End::TARGET_MAX_X),
          HALF_WIDTH + (focus.ball.y - HALF_WIDTH) * Tuning::End::WIDTH_FOLLOW,
          0.0f};
      rig.yaw = direction > 0.0f ? 0.0f : std::numbers::pi_v<float>;
      rig.pitch = Tuning::End::PITCH;
      rig.distance = Tuning::End::DISTANCE * zoomFactor;
      rig.fov = Tuning::End::FOV;
      break;
    }
    case MatchCameraMode::PLAYER_FOLLOW:
      rig.target = focus.hasCarrier ? focus.carrier : focus.ball;
      rig.target.z = Tuning::Follow::TARGET_HEIGHT;
      rig.yaw = chaseYaw;
      rig.pitch = Tuning::Follow::PITCH;
      rig.distance = Tuning::Follow::DISTANCE * zoomFactor;
      rig.fov = Tuning::Follow::FOV;
      break;
  }
  clampToBowl(rig.target, rig.yaw, rig.pitch, rig.distance);
  return rig;
}

void MatchCamera3D::snap(const MatchCameraFocus& focus, MatchCameraMode mode)
{
  chaseYaw = chaseHeading(focus);
  current = desiredRig(focus, mode);
  initialized = true;
}

void MatchCamera3D::update(const MatchCameraFocus& focus, MatchCameraMode mode,
                           float zoomSteps, float deltaSeconds)
{
  if (zoomSteps != 0.0f)
  {
    zoomFactor =
        std::clamp(zoomFactor * std::pow(Tuning::Camera::ZOOM_STEP, zoomSteps),
                   Tuning::Camera::MIN_ZOOM, Tuning::Camera::MAX_ZOOM);
  }
  if (!initialized)
  {
    snap(focus, mode);
    return;
  }

  const float dt =
      std::clamp(deltaSeconds, 0.0f, Tuning::Camera::MAX_FRAME_SECONDS);
  chaseYaw = RenderMath::lerpAngle(
      chaseYaw, chaseHeading(focus),
      RenderMath::dampingFactor(Tuning::Follow::YAW_RATE, dt));

  const Rig desired = desiredRig(focus, mode);
  const float targetBlend =
      RenderMath::dampingFactor(Tuning::Camera::TARGET_RATE, dt);
  const float angleBlend =
      RenderMath::dampingFactor(Tuning::Camera::ANGLE_RATE, dt);
  current.target =
      RenderMath::lerp(current.target, desired.target, targetBlend);
  current.yaw = RenderMath::wrapAngle(
      RenderMath::lerpAngle(current.yaw, desired.yaw, angleBlend));
  current.pitch += (desired.pitch - current.pitch) * angleBlend;
  current.distance +=
      (desired.distance - current.distance) *
      RenderMath::dampingFactor(Tuning::Camera::DISTANCE_RATE, dt);
  current.fov += (desired.fov - current.fov) *
                 RenderMath::dampingFactor(Tuning::Camera::FOV_RATE, dt);
}

Vec3 MatchCamera3D::eye() const
{
  Vec3 position = current.target -
                  viewDirection(current.yaw, current.pitch) * current.distance;
  position.z = std::max(position.z, Tuning::Camera::MIN_EYE_HEIGHT);
  return position;
}
