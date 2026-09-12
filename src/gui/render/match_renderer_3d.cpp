// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "gui/render/match_renderer_3d.h"

#include <fmt/printf.h>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <numbers>
#include <span>
#include <vector>

#include "global/language_manager.h"
#include "gui/render/match_camera_3d.h"
#include "gui/render/match_kit_colors.h"
#include "gui/render/match_render_3d_tuning.h"
#include "gui/render/match_render_math.h"
#include "gui/render/match_renderer_2d.h"
#include "gui/render/match_stadium_3d.h"
#include "model/player.h"
#include "model/role_utils.h"

namespace
{
using RenderMath::ScreenPoint;
using RenderMath::Vec3;
using RenderMath::Vec4;
using Tuning = MatchRender3DTuning;

constexpr float LENGTH = MatchTuning::Pitch::LENGTH_METRES;
constexpr float WIDTH = MatchTuning::Pitch::WIDTH_METRES;
constexpr Vec3 UP{0.0f, 0.0f, 1.0f};
constexpr float TWO_PI = 2.0f * std::numbers::pi_v<float>;
constexpr std::size_t MAX_POLYGON_POINTS = 13;

enum class PrimitiveKind : std::uint8_t
{
  POLYGON,
  LINE,
  HEAD,
  BALL,
  NET,
  BOARD,
};

/** Screen-space primitive waiting for the back-to-front sort. */
struct Primitive
{
  std::uint32_t firstPoint = 0;
  std::uint16_t pointCount = 0;
  PrimitiveKind kind = PrimitiveKind::POLYGON;
  ImU32 color = 0;
  ImU32 secondaryColor = 0;
  float size = 0.0f;
  std::uint32_t reference = 0;
};

struct SortKey
{
  float objectDepth = 0.0f;
  float localDepth = 0.0f;
  std::uint32_t primitive = 0;
};

struct ClipVertex
{
  Vec4 clip;
  ImU32 color = 0;
};

/** Per-snapshot-slot animation memory. */
struct AnimationSlot
{
  const Player* player = nullptr;
  Vec3 lastPosition;
  float phase = 0.0f;
  float stride = 0.0f;
};

/** Where a player landed on screen, for labels and hover tooltips. */
struct PlayerOnScreen
{
  const MatchRenderPlayer* player = nullptr;
  ImVec2 headTop;
  ImVec2 feet;
  float depth = 0.0f;
};

constexpr std::array<ImU32, 6> SKIN_TONES{
    IM_COL32(242, 206, 178, 255), IM_COL32(226, 180, 142, 255),
    IM_COL32(198, 146, 106, 255), IM_COL32(160, 110, 74, 255),
    IM_COL32(118, 78, 52, 255),   IM_COL32(86, 58, 40, 255)};
constexpr std::array<ImU32, 5> HAIR_COLORS{
    IM_COL32(24, 18, 14, 255), IM_COL32(64, 40, 24, 255),
    IM_COL32(120, 82, 44, 255), IM_COL32(200, 160, 96, 255),
    IM_COL32(40, 40, 44, 255)};

float signedArea(std::span<const ImVec2> points)
{
  float area = 0.0f;
  for (std::size_t index = 0; index < points.size(); ++index)
  {
    const ImVec2& a = points[index];
    const ImVec2& b = points[(index + 1) % points.size()];
    area += a.x * b.y - b.x * a.y;
  }
  return area;
}

float lighting(Vec3 normal)
{
  static const Vec3 LIGHT = RenderMath::normalize({Tuning::Light::DIRECTION_X,
                                                   Tuning::Light::DIRECTION_Y,
                                                   Tuning::Light::DIRECTION_Z});
  return Tuning::Light::AMBIENT +
         Tuning::Light::DIFFUSE *
             std::max(0.0f, RenderMath::dot(normal, LIGHT)) +
         Tuning::Light::SKY * std::max(0.0f, normal.z);
}

bool outsideClipVolume(std::span<const ClipVertex> vertices)
{
  bool left = true;
  bool right = true;
  bool bottom = true;
  bool top = true;
  for (const ClipVertex& vertex : vertices)
  {
    left = left && vertex.clip.x < -vertex.clip.w;
    right = right && vertex.clip.x > vertex.clip.w;
    bottom = bottom && vertex.clip.y < -vertex.clip.w;
    top = top && vertex.clip.y > vertex.clip.w;
  }
  return left || right || bottom || top;
}

ImVec2 toImVec(const ScreenPoint& point) { return {point.x, point.y}; }

/// Interpolated ball height above the grass in metres.
float ballHeightMetres(const MatchRenderBall& ball, float alpha)
{
  return std::max(0.0f, ball.previousHeightMetres +
                            (ball.currentHeightMetres -
                             ball.previousHeightMetres) *
                                alpha);
}

/// The player's standing height in metres (engine value, else his data).
float playerHeightMetres(const MatchRenderPlayer& player)
{
  using P = Tuning::Player;
  float height = player.heightMetres;
  if (height <= 0.0f && player.player && player.player->getHeight() > 0)
    height = static_cast<float>(player.player->getHeight()) * 0.01f;
  if (height <= 0.0f) height = P::REFERENCE_HEIGHT_METRES;
  return std::clamp(height, P::MIN_HEIGHT_METRES, P::MAX_HEIGHT_METRES);
}
}  // namespace

struct MatchRenderer3D::State
{
  Stadium3D::Geometry geometry;
  bool geometryBuilt = false;
  TeamID homeTeam = 0;
  TeamID awayTeam = 0;
  MatchKits kits;
  MatchCamera3D camera;
  RenderMath::Projection projection;
  /** False until a frame was projected (input maps through it). */
  bool hasProjection = false;
  ImDrawList* drawList = nullptr;
  ImVec2 whitePixel;
  float elapsedSeconds = 0.0f;
  float attackDirection = 1.0f;
  float directionChangeSeconds = 0.0f;

  std::vector<AnimationSlot> slots;
  std::vector<ImVec2> points;
  std::vector<Primitive> primitives;
  std::vector<SortKey> keys;
  std::vector<std::pair<float, std::uint32_t>> sectionOrder;
  std::vector<PlayerOnScreen> playersOnScreen;

  State()
  {
    points.reserve(8192);
    primitives.reserve(2048);
    keys.reserve(2048);
    sectionOrder.reserve(64);
    playersOnScreen.reserve(32);
    slots.reserve(32);
  }

  // --- frame setup -------------------------------------------------------
  void prepareMatch(const MatchRenderSnapshot& snapshot);
  MatchCameraFocus computeFocus(const MatchRenderSnapshot& snapshot,
                                float deltaSeconds);
  /** Turns the view's mouse input into world-space camera control. */
  MatchCameraControl cameraControl(const MatchCameraInput& input) const;

  // --- immediate (unsorted) layers ----------------------------------------
  void drawSky();
  void emitRaw(std::span<const ImVec2> screen, std::span<const ImU32> colors);
  void drawWorldPolygon(std::span<const Vec3> world,
                        std::span<const ImU32> colors, bool antiAliased);
  void drawGroundEllipse(Vec3 centre, Vec3 axisA, Vec3 axisB, ImU32 color);
  void drawShadows(const MatchRenderSnapshot& snapshot, float alpha);
  void drawStadium();
  void emitQuad(ImVec2 bottomLeft, ImVec2 bottomRight, ImVec2 topRight,
                ImVec2 topLeft, ImU32 bottom, ImU32 top);
  void drawCrowd(const Stadium3D::Face& face);
  void drawVignette();

  // --- sorted layer -------------------------------------------------------
  void addPolygon(std::span<const ScreenPoint> screen, ImU32 color,
                  float objectDepth, float localDepth);
  void addLine(Vec3 from, Vec3 to, ImU32 color, float thickness,
               float objectDepth);
  void addBox(const std::array<Vec3, 8>& corners, ImU32 color,
              float objectDepth);
  void addPlayer(const MatchRenderPlayer& player, AnimationSlot& slot,
                 float alpha, float deltaSeconds);
  void addGoals();
  void addCornerFlags();
  void addBoards();
  void addBall(const MatchRenderSnapshot& snapshot, float alpha);
  void flushSorted();
  void drawBoard(const Primitive& primitive);
  void drawBoardLogo(const Stadium3D::AdBoard& board,
                     const Stadium3D::SponsorStyle& style, Vec3 origin,
                     float width, float height);

  // --- overlays -----------------------------------------------------------
  void drawLabelsAndHover(const MatchRenderOptions& options);
  void drawScoreBug(const MatchRenderSnapshot& snapshot,
                    const MatchRenderOptions& options);
};

void MatchRenderer3D::State::prepareMatch(const MatchRenderSnapshot& snapshot)
{
  TeamID home = 0;
  TeamID away = 0;
  for (const MatchRenderPlayer& player : snapshot.players)
  {
    if (!player.player) continue;
    if (player.isHomeTeam && home == 0) home = player.player->getTeamId();
    if (!player.isHomeTeam && away == 0) away = player.player->getTeamId();
  }
  if (geometryBuilt && home == homeTeam && away == awayTeam) return;
  homeTeam = home;
  awayTeam = away;
  kits = chooseMatchKits(home, away);
  geometry.build(kits);
  geometryBuilt = true;
}

MatchCameraFocus MatchRenderer3D::State::computeFocus(
    const MatchRenderSnapshot& snapshot, float deltaSeconds)
{
  const float alpha = snapshot.interpolationAlpha;
  MatchCameraFocus focus;
  focus.ball = RenderMath::worldFromPitch(lerpRenderPosition(
      snapshot.ball.previousPosition, snapshot.ball.currentPosition, alpha));
  focus.ballVelocity =
      (RenderMath::worldFromPitch(snapshot.ball.currentPosition) -
       RenderMath::worldFromPitch(snapshot.ball.previousPosition)) *
      (1.0f / MatchTuning::Timing::FIXED_STEP_SECONDS);

  const MatchRenderPlayer* carrier = nullptr;
  for (const MatchRenderPlayer& player : snapshot.players)
    if (player.possessesBall) carrier = &player;
  if (carrier)
  {
    focus.hasCarrier = true;
    focus.carrier = RenderMath::worldFromPitch(lerpRenderPosition(
        carrier->previousPosition, carrier->currentPosition, alpha));
    focus.carrierYaw = RenderMath::worldYawFromFacing(RenderMath::lerpAngle(
        carrier->previousFacingAngle, carrier->currentFacingAngle, alpha));
    // Attack direction comes from where the carrier's own keeper stands, so
    // it stays right even if the engine ever swaps ends.
    float direction = carrier->isHomeTeam ? 1.0f : -1.0f;
    for (const MatchRenderPlayer& player : snapshot.players)
    {
      if (player.isHomeTeam == carrier->isHomeTeam && player.isGoalkeeper &&
          player.onPitch)
      {
        direction = player.currentPosition.x < MatchTuning::Pitch::CENTRE
                        ? 1.0f
                        : -1.0f;
      }
    }
    // Brief turnovers must not swing the end camera around the pitch.
    directionChangeSeconds = direction != attackDirection
                                 ? directionChangeSeconds + deltaSeconds
                                 : 0.0f;
    if (directionChangeSeconds >= Tuning::Camera::ATTACK_SWITCH_SECONDS)
    {
      attackDirection = direction;
      directionChangeSeconds = 0.0f;
    }
  }
  focus.attackDirection = attackDirection;
  return focus;
}

MatchCameraControl MatchRenderer3D::State::cameraControl(
    const MatchCameraInput& input) const
{
  MatchCameraControl control;
  control.zoomSteps = input.zoomSteps;
  control.orbitYaw = -input.orbitX * Tuning::Free::ORBIT_RADIANS_PER_PIXEL;
  control.orbitPitch = input.orbitY * Tuning::Free::ORBIT_RADIANS_PER_PIXEL;
  control.followBall = input.followBall;
  control.reset = input.reset;
  if (!hasProjection) return control;
  if (input.pan)
  {
    // Grab the ground: the point under the cursor follows the cursor. Near
    // the horizon (or over the sky) fall back to a distance-scaled pan.
    Vec3 from;
    Vec3 to;
    const float limit = camera.distance() * 0.5f;
    if (projection.groundPointAt(input.panFromX, input.panFromY, from) &&
        projection.groundPointAt(input.panToX, input.panToY, to) &&
        RenderMath::length(from - to) <= limit)
    {
      control.pan = from - to;
    }
    else
    {
      const float metresPerPixel = camera.distance() / projection.focalPixels;
      const Vec3 flatSide =
          RenderMath::normalize({projection.side.x, projection.side.y, 0.0f});
      const Vec3 flatForward = RenderMath::normalize(
          {projection.forward.x, projection.forward.y, 0.0f});
      control.pan = flatSide * ((input.panFromX - input.panToX) *
                                metresPerPixel) +
                    flatForward *
                        ((input.panToY - input.panFromY) * metresPerPixel);
    }
    control.pan.z = 0.0f;
  }
  if (input.retarget)
  {
    control.retarget = projection.groundPointAt(
        input.retargetX, input.retargetY, control.retargetPoint);
  }
  return control;
}

void MatchRenderer3D::State::emitRaw(std::span<const ImVec2> screen,
                                     std::span<const ImU32> colors)
{
  const auto count = static_cast<int>(screen.size());
  drawList->PrimReserve((count - 2) * 3, count);
  const auto base = static_cast<ImDrawIdx>(drawList->_VtxCurrentIdx);
  for (int index = 0; index < count; ++index)
    drawList->PrimWriteVtx(screen[static_cast<std::size_t>(index)], whitePixel,
                           colors[static_cast<std::size_t>(index)]);
  for (int index = 2; index < count; ++index)
  {
    drawList->PrimWriteIdx(base);
    drawList->PrimWriteIdx(static_cast<ImDrawIdx>(base + index - 1));
    drawList->PrimWriteIdx(static_cast<ImDrawIdx>(base + index));
  }
}

void MatchRenderer3D::State::drawWorldPolygon(std::span<const Vec3> world,
                                              std::span<const ImU32> colors,
                                              bool antiAliased)
{
  std::array<ClipVertex, MAX_POLYGON_POINTS> input{};
  std::array<ClipVertex, MAX_POLYGON_POINTS> clipped{};
  const std::size_t count = std::min(world.size(), MAX_POLYGON_POINTS - 1);
  bool allInFront = true;
  bool anyInFront = false;
  for (std::size_t index = 0; index < count; ++index)
  {
    input[index] = {projection.toClip(world[index]), colors[index]};
    const bool inFront = input[index].clip.w >= projection.nearPlane;
    allInFront = allInFront && inFront;
    anyInFront = anyInFront || inFront;
  }
  if (!anyInFront) return;
  std::span<const ClipVertex> vertices(input.data(), count);
  if (!allInFront)
  {
    const std::size_t clippedCount = RenderMath::clipConvexToNearPlane(
        std::span<const ClipVertex>(input.data(), count),
        std::span<ClipVertex>(clipped), projection.nearPlane,
        [](const ClipVertex& vertex) { return vertex.clip.w; },
        [](const ClipVertex& a, const ClipVertex& b, float t)
        {
          return ClipVertex{RenderMath::lerp(a.clip, b.clip, t),
                            mixColor(a.color, b.color, t)};
        });
    if (clippedCount < 3) return;
    vertices = std::span<const ClipVertex>(clipped.data(), clippedCount);
  }
  if (outsideClipVolume(vertices)) return;

  std::array<ImVec2, MAX_POLYGON_POINTS> screen{};
  std::array<ImU32, MAX_POLYGON_POINTS> screenColors{};
  for (std::size_t index = 0; index < vertices.size(); ++index)
  {
    screen[index] = toImVec(projection.clipToScreen(vertices[index].clip));
    screenColors[index] = vertices[index].color;
  }
  const std::span<ImVec2> screenSpan(screen.data(), vertices.size());
  if (antiAliased)
  {
    if (signedArea(screenSpan) < 0.0f)
      std::reverse(screenSpan.begin(), screenSpan.end());
    drawList->AddConvexPolyFilled(
        screen.data(), static_cast<int>(vertices.size()), screenColors[0]);
    return;
  }
  emitRaw(screenSpan,
          std::span<const ImU32>(screenColors.data(), vertices.size()));
}

void MatchRenderer3D::State::drawSky()
{
  const ImVec2 minimum{projection.rect.x, projection.rect.y};
  const ImVec2 maximum{projection.rect.x + projection.rect.width,
                       projection.rect.y + projection.rect.height};
  const Vec3 flatForward =
      RenderMath::normalize({projection.forward.x, projection.forward.y, 0.0f});
  float horizon = minimum.y;
  ScreenPoint horizonPoint;
  if (projection.project(
          projection.eye + flatForward * Tuning::Sky::HORIZON_DISTANCE,
          horizonPoint))
  {
    horizon = std::clamp(horizonPoint.y, minimum.y, maximum.y);
  }
  drawList->AddRectFilled({minimum.x, horizon}, maximum,
                          Tuning::Sky::GROUND_COLOR);
  if (horizon <= minimum.y) return;
  const float middle = minimum.y + (horizon - minimum.y) * 0.55f;
  const std::array<ImU32, 4> upper{
      Tuning::Sky::TOP_COLOR, Tuning::Sky::TOP_COLOR, Tuning::Sky::MIDDLE_COLOR,
      Tuning::Sky::MIDDLE_COLOR};
  const std::array<ImVec2, 4> upperQuad{
      ImVec2{minimum.x, minimum.y}, ImVec2{maximum.x, minimum.y},
      ImVec2{maximum.x, middle}, ImVec2{minimum.x, middle}};
  emitRaw(upperQuad, upper);
  const std::array<ImU32, 4> lower{
      Tuning::Sky::MIDDLE_COLOR, Tuning::Sky::MIDDLE_COLOR,
      Tuning::Sky::HORIZON_COLOR, Tuning::Sky::HORIZON_COLOR};
  const std::array<ImVec2, 4> lowerQuad{
      ImVec2{minimum.x, middle}, ImVec2{maximum.x, middle},
      ImVec2{maximum.x, horizon}, ImVec2{minimum.x, horizon}};
  emitRaw(lowerQuad, lower);
}

void MatchRenderer3D::State::drawGroundEllipse(Vec3 centre, Vec3 axisA,
                                               Vec3 axisB, ImU32 color)
{
  std::array<Vec3, Tuning::Shadow::SEGMENTS> world{};
  std::array<ImU32, Tuning::Shadow::SEGMENTS> colors{};
  for (std::size_t index = 0; index < world.size(); ++index)
  {
    const float angle =
        TWO_PI * static_cast<float>(index) / static_cast<float>(world.size());
    world[index] = centre + axisA * std::cos(angle) + axisB * std::sin(angle);
    colors[index] = color;
  }
  drawWorldPolygon(world, colors, true);
}

void MatchRenderer3D::State::drawShadows(const MatchRenderSnapshot& snapshot,
                                         float alpha)
{
  // Four floodlight towers cast four faint blades per player plus a darker
  // contact shadow: the classic night-match look.
  using F = Tuning::Floodlight;
  const std::array<Vec3, 4> lights{
      Vec3{-F::CORNER_OFFSET_X, -F::CORNER_OFFSET_Y, 0.0f},
      Vec3{LENGTH + F::CORNER_OFFSET_X, -F::CORNER_OFFSET_Y, 0.0f},
      Vec3{-F::CORNER_OFFSET_X, WIDTH + F::CORNER_OFFSET_Y, 0.0f},
      Vec3{LENGTH + F::CORNER_OFFSET_X, WIDTH + F::CORNER_OFFSET_Y, 0.0f}};
  for (const MatchRenderPlayer& player : snapshot.players)
  {
    const Vec3 root = RenderMath::worldFromPitch(lerpRenderPosition(
        player.previousPosition, player.currentPosition, alpha));
    ScreenPoint screen;
    if (!projection.project(root, screen)) continue;
    // Far away, a soft disc in the shirt colour keeps the teams readable.
    const float pixelHeight = playerHeightMetres(player) *
                              projection.focalPixels / screen.depth;
    if (pixelHeight < Tuning::Player::MARKER_MAX_PIXELS)
    {
      const KitColors& kit =
          player.isHomeTeam
              ? (player.isGoalkeeper ? kits.homeGoalkeeper : kits.home)
              : (player.isGoalkeeper ? kits.awayGoalkeeper : kits.away);
      const float fade = std::clamp(
          (Tuning::Player::MARKER_MAX_PIXELS - pixelHeight) /
              (Tuning::Player::MARKER_MAX_PIXELS * 0.5f),
          0.0f, 1.0f);
      const float radius = Tuning::Player::MARKER_RADIUS;
      // A shirt close to the grass colour would vanish: use the trim.
      const ImU32 marker =
          kitColorDistance(kit.shirt, Tuning::Grass::PITCH_COLOR) <
                  KIT_CLASH_DISTANCE
              ? kit.trim
              : kit.shirt;
      drawGroundEllipse(
          root, {radius, 0.0f, 0.0f}, {0.0f, radius, 0.0f},
          withAlpha(marker,
                    static_cast<std::uint8_t>(
                        static_cast<float>(Tuning::Player::MARKER_ALPHA) *
                        fade)));
    }
    for (const Vec3& light : lights)
    {
      const Vec3 away = RenderMath::normalize(root - light);
      const Vec3 across{-away.y, away.x, 0.0f};
      drawGroundEllipse(root + away * (Tuning::Shadow::BLADE_LENGTH * 0.5f),
                        away * (Tuning::Shadow::BLADE_LENGTH * 0.5f),
                        across * Tuning::Shadow::BLADE_HALF_WIDTH,
                        Tuning::Shadow::BLADE_COLOR);
    }
    drawGroundEllipse(root, {Tuning::Shadow::CONTACT_RADIUS, 0.0f, 0.0f},
                      {0.0f, Tuning::Shadow::CONTACT_RADIUS, 0.0f},
                      Tuning::Shadow::CONTACT_COLOR);
    if (player.possessesBall)
    {
      const float radius =
          Tuning::Shadow::RING_RADIUS *
          (1.0f +
           Tuning::Shadow::RING_PULSE *
               std::sin(elapsedSeconds * Tuning::Shadow::RING_PULSE_SPEED));
      std::array<ImVec2, Tuning::Shadow::RING_SEGMENTS> ring{};
      bool visible = true;
      for (std::size_t index = 0; index < ring.size() && visible; ++index)
      {
        const float angle = TWO_PI * static_cast<float>(index) /
                            static_cast<float>(ring.size());
        ScreenPoint point;
        visible =
            projection.project(root + Vec3{std::cos(angle) * radius,
                                           std::sin(angle) * radius, 0.0f},
                               point);
        ring[index] = toImVec(point);
      }
      if (visible)
      {
        drawList->AddPolyline(ring.data(), static_cast<int>(ring.size()),
                              Tuning::Shadow::RING_COLOR, ImDrawFlags_Closed,
                              Tuning::Shadow::RING_THICKNESS);
      }
    }
  }

  const Vector2F ball = lerpRenderPosition(
      snapshot.ball.previousPosition, snapshot.ball.currentPosition, alpha);
  const float height = ballHeightMetres(snapshot.ball, alpha);
  const float radius = Tuning::Ball::SHADOW_RADIUS *
                       (1.0f + height * Tuning::Ball::SHADOW_GROWTH);
  const auto shadowAlpha = static_cast<std::uint8_t>(
      110.0f / (1.0f + height * Tuning::Ball::SHADOW_FADE));
  drawGroundEllipse(RenderMath::worldFromPitch(ball), {radius, 0.0f, 0.0f},
                    {0.0f, radius, 0.0f}, IM_COL32(0, 0, 0, shadowAlpha));
}

void MatchRenderer3D::State::emitQuad(ImVec2 bottomLeft, ImVec2 bottomRight,
                                      ImVec2 topRight, ImVec2 topLeft,
                                      ImU32 bottom, ImU32 top)
{
  const auto first = static_cast<ImDrawIdx>(drawList->_VtxCurrentIdx);
  drawList->PrimWriteVtx(bottomLeft, whitePixel, bottom);
  drawList->PrimWriteVtx(bottomRight, whitePixel, bottom);
  drawList->PrimWriteVtx(topRight, whitePixel, top);
  drawList->PrimWriteVtx(topLeft, whitePixel, top);
  drawList->PrimWriteIdx(first);
  drawList->PrimWriteIdx(static_cast<ImDrawIdx>(first + 1));
  drawList->PrimWriteIdx(static_cast<ImDrawIdx>(first + 2));
  drawList->PrimWriteIdx(first);
  drawList->PrimWriteIdx(static_cast<ImDrawIdx>(first + 2));
  drawList->PrimWriteIdx(static_cast<ImDrawIdx>(first + 3));
}

void MatchRenderer3D::State::drawCrowd(const Stadium3D::Face& face)
{
  using C = Tuning::Crowd;
  if (face.clumpEnd <= face.clumpBegin) return;
  const Vec3 centre = (face.corners[0] + face.corners[2]) * 0.5f;
  ScreenPoint centreScreen;
  ScreenPoint aboveScreen;
  if (!projection.project(centre, centreScreen) ||
      !projection.project(centre + UP, aboveScreen))
    return;
  // Screen-space "up" per metre at the face centre, rescaled per depth.
  const float upX = (aboveScreen.x - centreScreen.x) * centreScreen.depth;
  const float upY = (aboveScreen.y - centreScreen.y) * centreScreen.depth;
  const float left = projection.rect.x;
  const float right = projection.rect.x + projection.rect.width;
  const float top = projection.rect.y;
  const float bottom = projection.rect.y + projection.rect.height;
  const float seatFocal = C::SEAT_SPACING * projection.focalPixels;
  const float halfDotFocal = C::DOT_WIDTH * 0.5f * projection.focalPixels;

  // Worst case: every spectator with a separate head (two quads); what is
  // not used is handed back at the end.
  const std::uint32_t firstDot = geometry.clumps[face.clumpBegin].dotBegin;
  const std::uint32_t endDot = geometry.clumps[face.clumpEnd - 1].dotEnd;
  const auto worstQuads = static_cast<int>(endDot - firstDot) * 2;
  drawList->PrimReserve(worstQuads * 6, worstQuads * 4);
  int quads = 0;
  for (std::uint32_t clumpIndex = face.clumpBegin; clumpIndex < face.clumpEnd;
       ++clumpIndex)
  {
    const Stadium3D::CrowdClump& clump = geometry.clumps[clumpIndex];
    const Vec4 clumpClip = projection.toClip(clump.base);
    if (clumpClip.w < projection.nearPlane) continue;
    const float clumpInverse = 1.0f / clumpClip.w;
    const float seatPixels = seatFocal * clumpInverse;
    if (seatPixels < C::CLUMP_PIXELS)
    {
      // Far: one quad up the tier in the members' average colour.
      const float halfWidth =
          clump.halfWidth * projection.focalPixels * clumpInverse;
      if (halfWidth * 2.0f < C::MIN_CLUMP_PIXELS) continue;
      const Vec4 topClip = projection.toClip(clump.top);
      if (topClip.w < projection.nearPlane) continue;
      const ScreenPoint base = projection.clipToScreen(clumpClip);
      const ScreenPoint upper = projection.clipToScreen(topClip);
      const float topHalf =
          clump.halfWidth * projection.focalPixels / topClip.w;
      if (std::max(base.x, upper.x) + halfWidth < left ||
          std::min(base.x, upper.x) - halfWidth > right ||
          std::min(base.y, upper.y) > bottom ||
          std::max(base.y, upper.y) < top)
        continue;
      emitQuad({base.x - halfWidth, base.y}, {base.x + halfWidth, base.y},
               {upper.x + topHalf, upper.y}, {upper.x - topHalf, upper.y},
               shadeColor(clump.color, 0.85f), clump.color);
      ++quads;
      continue;
    }
    // Near: individual spectators whose colours fade towards the clump
    // average while they are still small (cheap mip-mapping).
    const float detail =
        std::clamp((seatPixels - C::CLUMP_PIXELS) /
                       (C::FULL_DETAIL_PIXELS - C::CLUMP_PIXELS),
                   0.0f, 1.0f);
    for (std::uint32_t index = clump.dotBegin; index < clump.dotEnd; ++index)
    {
      const Stadium3D::CrowdDot& dot = geometry.crowd[index];
      const Vec4 clip = projection.toClip(dot.base);
      if (clip.w < projection.nearPlane) continue;
      const ScreenPoint base = projection.clipToScreen(clip);
      const float inverseDepth = 1.0f / clip.w;
      const float halfWidth = halfDotFocal * inverseDepth;
      const float dx = upX * C::DOT_HEIGHT * inverseDepth;
      const float dy = upY * C::DOT_HEIGHT * inverseDepth;
      if (base.x + halfWidth < left || base.x - halfWidth > right ||
          base.y + dy > bottom || base.y < top)
        continue;
      const ImU32 body =
          detail < 1.0f ? mixColor(clump.color, dot.body, detail) : dot.body;
      const ImU32 head =
          detail < 1.0f ? mixColor(clump.color, dot.head, detail) : dot.head;
      if (halfWidth * 2.0f < C::HEAD_DETAIL_PIXELS)
      {
        emitQuad({base.x - halfWidth, base.y}, {base.x + halfWidth, base.y},
                 {base.x + halfWidth + dx, base.y + dy},
                 {base.x - halfWidth + dx, base.y + dy},
                 shadeColor(body, 0.75f), mixColor(body, head, 0.5f));
        ++quads;
        continue;
      }
      const float shoulderX = base.x + dx * C::BODY_SHARE;
      const float shoulderY = base.y + dy * C::BODY_SHARE;
      emitQuad({base.x - halfWidth, base.y}, {base.x + halfWidth, base.y},
               {shoulderX + halfWidth, shoulderY},
               {shoulderX - halfWidth, shoulderY}, shadeColor(body, 0.75f),
               body);
      const float headHalf = halfWidth * C::HEAD_WIDTH_SHARE;
      const float neckX = base.x + dx * (C::BODY_SHARE + C::NECK_GAP);
      const float neckY = base.y + dy * (C::BODY_SHARE + C::NECK_GAP);
      emitQuad({neckX - headHalf, neckY}, {neckX + headHalf, neckY},
               {base.x + dx + headHalf, base.y + dy},
               {base.x + dx - headHalf, base.y + dy}, shadeColor(head, 0.8f),
               head);
      quads += 2;
    }
  }
  const int unused = worstQuads - quads;
  drawList->PrimUnreserve(unused * 6, unused * 4);
}

void MatchRenderer3D::State::drawStadium()
{
  using S = Tuning::Stands;
  const Vec3 eye = projection.eye;
  const bool belowRoofs = eye.z < Tuning::Camera::STAND_CLEAR_HEIGHT;
  sectionOrder.clear();
  for (std::uint32_t index = 0;
       index < static_cast<std::uint32_t>(geometry.sections.size()); ++index)
  {
    const Stadium3D::Section& section = geometry.sections[index];
    // A stand the eye sits in (or behind, below roof level) is never drawn:
    // real broadcasts do not show the gantry's own stand either. High
    // cameras look over the roofs and draw every stand.
    if (belowRoofs &&
        ((section.side == Stadium3D::Side::SOUTH && eye.y < -S::SIDE_FRONT) ||
         (section.side == Stadium3D::Side::NORTH &&
          eye.y > WIDTH + S::SIDE_FRONT) ||
         (section.side == Stadium3D::Side::WEST && eye.x < -S::END_FRONT) ||
         (section.side == Stadium3D::Side::EAST &&
          eye.x > LENGTH + S::END_FRONT)))
      continue;
    sectionOrder.emplace_back(projection.depth(section.centre), index);
  }
  std::sort(sectionOrder.begin(), sectionOrder.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });

  for (const auto& [depth, sectionIndex] : sectionOrder)
  {
    const Stadium3D::Section& section = geometry.sections[sectionIndex];
    for (std::uint32_t faceIndex = section.faceBegin;
         faceIndex < section.faceEnd; ++faceIndex)
    {
      const Stadium3D::Face& face = geometry.faces[faceIndex];
      if (RenderMath::dot(face.normal, eye - face.corners[0]) <= 0.0f) continue;
      drawWorldPolygon(face.corners, face.colors, false);
      drawCrowd(face);
      if (face.glow != 0)
      {
        const Vec3 centre = (face.corners[0] + face.corners[2]) * 0.5f;
        ScreenPoint glow;
        if (projection.project(centre, glow))
        {
          const float radius = Tuning::Floodlight::GLOW_RADIUS *
                               projection.focalPixels / glow.depth;
          drawList->AddCircleFilled(toImVec(glow), radius,
                                    withAlpha(face.glow, 34));
          drawList->AddCircleFilled(toImVec(glow), radius * 0.55f,
                                    withAlpha(face.glow, 70));
          drawList->AddCircleFilled(toImVec(glow), radius * 0.25f,
                                    withAlpha(face.glow, 200));
        }
      }
    }
  }
}

void MatchRenderer3D::State::addPolygon(std::span<const ScreenPoint> screen,
                                        ImU32 color, float objectDepth,
                                        float localDepth)
{
  Primitive primitive;
  primitive.firstPoint = static_cast<std::uint32_t>(points.size());
  primitive.pointCount = static_cast<std::uint16_t>(screen.size());
  primitive.color = color;
  for (const ScreenPoint& point : screen) points.push_back(toImVec(point));
  const std::span<ImVec2> written(points.data() + primitive.firstPoint,
                                  screen.size());
  if (signedArea(written) < 0.0f) std::reverse(written.begin(), written.end());
  keys.push_back(
      {objectDepth, localDepth, static_cast<std::uint32_t>(primitives.size())});
  primitives.push_back(primitive);
}

void MatchRenderer3D::State::addLine(Vec3 from, Vec3 to, ImU32 color,
                                     float thickness, float objectDepth)
{
  ScreenPoint a;
  ScreenPoint b;
  if (!projection.project(from, a) || !projection.project(to, b)) return;
  Primitive primitive;
  primitive.firstPoint = static_cast<std::uint32_t>(points.size());
  primitive.pointCount = 2;
  primitive.kind = PrimitiveKind::LINE;
  primitive.color = color;
  primitive.size = std::max(
      1.0f, thickness * projection.focalPixels * 2.0f / (a.depth + b.depth));
  points.push_back(toImVec(a));
  points.push_back(toImVec(b));
  keys.push_back({objectDepth, (a.depth + b.depth) * 0.5f,
                  static_cast<std::uint32_t>(primitives.size())});
  primitives.push_back(primitive);
}

void MatchRenderer3D::State::addBox(const std::array<Vec3, 8>& corners,
                                    ImU32 color, float objectDepth)
{
  std::array<ScreenPoint, 8> screen{};
  for (std::size_t index = 0; index < corners.size(); ++index)
    if (!projection.project(corners[index], screen[index])) return;
  for (const auto& face : Stadium3D::BOX_FACES)
  {
    const Vec3& v0 = corners[face[0]];
    const Vec3 normal = RenderMath::cross(corners[face[2]] - v0,
                                          corners[face[3]] - corners[face[1]]);
    if (RenderMath::dot(normal, projection.eye - v0) <= 0.0f) continue;
    const std::array<ScreenPoint, 4> quad{screen[face[0]], screen[face[1]],
                                          screen[face[2]], screen[face[3]]};
    const float localDepth =
        (quad[0].depth + quad[1].depth + quad[2].depth + quad[3].depth) * 0.25f;
    addPolygon(quad, shadeColor(color, lighting(RenderMath::normalize(normal))),
               objectDepth, localDepth);
  }
}

void MatchRenderer3D::State::addPlayer(const MatchRenderPlayer& player,
                                       AnimationSlot& slot, float alpha,
                                       float deltaSeconds)
{
  using P = Tuning::Player;
  const Vec3 root = RenderMath::worldFromPitch(lerpRenderPosition(
      player.previousPosition, player.currentPosition, alpha));
  if (slot.player != player.player)
  {
    slot = AnimationSlot{};
    slot.player = player.player;
    slot.lastPosition = root;
    slot.phase =
        player.player
            ? static_cast<float>(cosmeticHash(player.player->getId()) % 628U) /
                  100.0f
            : 0.0f;
  }
  // The gait (stride length, swing amplitude, idling) comes from the
  // simulated ground speed; the leg phase advances with the distance the
  // player actually covers on screen, so boots never slide over the grass
  // at any playback speed.
  Vec3 moved = root - slot.lastPosition;
  moved.z = 0.0f;
  float distance = RenderMath::length(moved);
  if (distance > P::TELEPORT_METRES) distance = 0.0f;
  slot.lastPosition = root;
  const float groundSpeed = player.speedMetresPerSecond;
  const float targetStride =
      groundSpeed < P::IDLE_SPEED
          ? 0.0f
          : std::clamp(groundSpeed / P::FULL_STRIDE_SPEED, 0.0f, 1.0f);
  slot.stride += (targetStride - slot.stride) *
                 RenderMath::dampingFactor(P::STRIDE_RATE, deltaSeconds);
  const float cycleMetres =
      P::STRIDE_BASE_METRES + P::STRIDE_PER_SPEED * groundSpeed;
  slot.phase = std::fmod(
      slot.phase + TWO_PI * std::min(distance / cycleMetres,
                                     P::MAX_CYCLES_PER_FRAME),
      TWO_PI);

  ScreenPoint anchor;
  if (!projection.project(root + UP * P::HIP_HEIGHT, anchor)) return;
  const RenderMath::ScreenRect& rect = projection.rect;
  if (anchor.x < rect.x - P::CULL_MARGIN_PIXELS ||
      anchor.x > rect.x + rect.width + P::CULL_MARGIN_PIXELS ||
      anchor.y < rect.y - P::CULL_MARGIN_PIXELS ||
      anchor.y > rect.y + rect.height + P::CULL_MARGIN_PIXELS)
    return;

  const bool keeper = player.isGoalkeeper;
  const KitColors& kit = player.isHomeTeam
                             ? (keeper ? kits.homeGoalkeeper : kits.home)
                             : (keeper ? kits.awayGoalkeeper : kits.away);
  const std::uint32_t looks =
      cosmeticHash(player.player ? player.player->getId() + 17U : 17U);
  const ImU32 skin = SKIN_TONES[looks % SKIN_TONES.size()];
  const ImU32 hair = HAIR_COLORS[(looks >> 8) % HAIR_COLORS.size()];
  const float scale =
      P::SCALE * playerHeightMetres(player) / P::REFERENCE_HEIGHT_METRES;

  const float yaw = RenderMath::worldYawFromFacing(RenderMath::lerpAngle(
      player.previousFacingAngle, player.currentFacingAngle, alpha));
  const Vec3 forward{std::cos(yaw), std::sin(yaw), 0.0f};
  const Vec3 side{-forward.y, forward.x, 0.0f};
  const float stride = slot.stride;
  const float bob = stride * P::RUN_BOB * std::abs(std::sin(slot.phase));
  const Vec3 base = root + UP * (bob * scale);
  const float objectDepth = anchor.depth;

  const auto limb = [&](Vec3 joint, float angle, float limbLength,
                        float halfWidth, ImU32 color)
  {
    const Vec3 down = UP * -std::cos(angle) + forward * std::sin(angle);
    const Vec3 along = down * -1.0f;
    const Vec3 across = RenderMath::cross(side, along);
    const Vec3 centre = joint + down * (limbLength * 0.5f * scale);
    std::array<Vec3, 8> corners{};
    for (std::size_t corner = 0; corner < 8; ++corner)
    {
      corners[corner] =
          centre +
          across * (((corner & 1U) != 0U ? 1.0f : -1.0f) * halfWidth * scale) +
          side * (((corner & 2U) != 0U ? 1.0f : -1.0f) * halfWidth * scale) +
          along * (((corner & 4U) != 0U ? 1.0f : -1.0f) * limbLength * 0.5f *
                   scale);
    }
    addBox(corners, color, objectDepth);
    return joint + down * (limbLength * scale);
  };
  const auto orientedBox =
      [&](Vec3 centre, Vec3 axisA, Vec3 axisB, Vec3 axisC, ImU32 color)
  {
    std::array<Vec3, 8> corners{};
    for (std::size_t corner = 0; corner < 8; ++corner)
    {
      corners[corner] = centre + axisA * ((corner & 1U) != 0U ? 1.0f : -1.0f) +
                        axisB * ((corner & 2U) != 0U ? 1.0f : -1.0f) +
                        axisC * ((corner & 4U) != 0U ? 1.0f : -1.0f);
    }
    addBox(corners, color, objectDepth);
  };

  for (const float sideSign : {1.0f, -1.0f})
  {
    const float legPhase =
        slot.phase + (sideSign > 0.0f ? 0.0f : std::numbers::pi_v<float>);
    const float thigh = stride * P::THIGH_SWING * std::sin(legPhase);
    const float knee =
        -stride * P::KNEE_FLEX * (0.5f - 0.5f * std::cos(legPhase));
    const Vec3 hip = base + UP * (P::HIP_HEIGHT * scale) +
                     side * (sideSign * P::HIP_SPREAD * scale);
    const Vec3 kneeJoint =
        limb(hip, thigh, P::THIGH_LENGTH, P::THIGH_HALF_WIDTH, skin);
    const Vec3 ankle = limb(kneeJoint, thigh + knee, P::SHIN_LENGTH,
                            P::SHIN_HALF_WIDTH, kit.socks);
    orientedBox(ankle + forward * (P::BOOT_FORWARD * scale),
                forward * (P::BOOT_HALF_LENGTH * scale),
                side * (P::BOOT_HALF_WIDTH * scale),
                UP * (P::BOOT_HALF_HEIGHT * scale), P::BOOT_COLOR);
  }
  orientedBox(base + UP * (P::SHORTS_HEIGHT * scale),
              forward * (P::SHORTS_HALF_DEPTH * scale),
              side * (P::SHORTS_HALF_WIDTH * scale),
              UP * (P::SHORTS_HALF_HEIGHT * scale), kit.shorts);

  // Tapered torso leaning into the run.
  const float lean = stride * P::RUN_LEAN;
  const Vec3 torsoUp = UP * std::cos(lean) + forward * std::sin(lean);
  const Vec3 torsoForward = forward * std::cos(lean) - UP * std::sin(lean);
  const Vec3 waist = base + UP * (P::TORSO_BASE * scale);
  const Vec3 chest = waist + torsoUp * (P::TORSO_LENGTH * scale);
  std::array<Vec3, 8> torso{};
  for (std::size_t corner = 0; corner < 8; ++corner)
  {
    const bool upper = (corner & 4U) != 0U;
    const float depth =
        (upper ? P::CHEST_HALF_DEPTH : P::WAIST_HALF_DEPTH) * scale;
    const float width =
        (upper ? P::CHEST_HALF_WIDTH : P::WAIST_HALF_WIDTH) * scale;
    torso[corner] = (upper ? chest : waist) +
                    torsoForward * ((corner & 1U) != 0U ? depth : -depth) +
                    side * ((corner & 2U) != 0U ? width : -width);
  }
  addBox(torso, kit.shirt, objectDepth);
  orientedBox(chest, torsoForward * (P::COLLAR_HALF_SIZE * scale),
              side * (P::COLLAR_HALF_SIZE * scale),
              torsoUp * (P::COLLAR_HALF_HEIGHT * scale), kit.trim);

  for (const float sideSign : {1.0f, -1.0f})
  {
    const float legPhase =
        slot.phase + (sideSign > 0.0f ? 0.0f : std::numbers::pi_v<float>);
    const float upperArm = -stride * P::ARM_SWING * std::sin(legPhase);
    const Vec3 shoulder = chest - torsoUp * (P::SHOULDER_DROP * scale) +
                          side * (sideSign * P::SHOULDER_SPREAD * scale);
    const Vec3 elbow = limb(shoulder, upperArm, P::UPPER_ARM_LENGTH,
                            P::UPPER_ARM_HALF_WIDTH, kit.shirt);
    limb(elbow, upperArm + P::ELBOW_BEND + stride * P::ELBOW_RUN_BEND,
         P::FOREARM_LENGTH, P::FOREARM_HALF_WIDTH, skin);
  }

  const Vec3 headCentre = chest + torsoUp * (P::NECK_LENGTH * scale);
  ScreenPoint head;
  if (projection.project(headCentre, head))
  {
    Primitive primitive;
    primitive.firstPoint = static_cast<std::uint32_t>(points.size());
    primitive.pointCount = 1;
    primitive.kind = PrimitiveKind::HEAD;
    primitive.color = skin;
    primitive.secondaryColor = hair;
    primitive.size =
        P::HEAD_RADIUS * scale * projection.focalPixels / head.depth;
    points.push_back(toImVec(head));
    keys.push_back({objectDepth, head.depth,
                    static_cast<std::uint32_t>(primitives.size())});
    primitives.push_back(primitive);

    ScreenPoint feet;
    projection.project(root, feet);
    playersOnScreen.push_back({&player,
                               {head.x, head.y - primitive.size},
                               toImVec(feet),
                               head.depth});
  }
}

void MatchRenderer3D::State::addGoals()
{
  for (const Stadium3D::Goal& goal : geometry.goals)
  {
    for (const Stadium3D::NetPanel& net : goal.nets)
    {
      std::array<ScreenPoint, 4> corners{};
      bool visible = true;
      for (std::size_t index = 0; index < 4 && visible; ++index)
        visible = projection.project(net.corners[index], corners[index]);
      if (!visible) continue;
      Primitive primitive;
      primitive.firstPoint = static_cast<std::uint32_t>(points.size());
      primitive.kind = PrimitiveKind::NET;
      primitive.color = Tuning::Goal::NET_FILL_COLOR;
      primitive.secondaryColor = Tuning::Goal::NET_LINE_COLOR;
      for (const ScreenPoint& corner : corners)
        points.push_back(toImVec(corner));
      const std::span<ImVec2> quad(points.data() + primitive.firstPoint, 4);
      if (signedArea(quad) < 0.0f) std::reverse(quad.begin(), quad.end());
      // Mesh strands: bilinear lines across the panel in both directions.
      const auto bilinear = [&net](float u, float v)
      {
        const Vec3 bottom = RenderMath::lerp(net.corners[0], net.corners[1], u);
        const Vec3 top = RenderMath::lerp(net.corners[3], net.corners[2], u);
        return RenderMath::lerp(bottom, top, v);
      };
      const auto strand = [&](Vec3 from, Vec3 to)
      {
        ScreenPoint a;
        ScreenPoint b;
        if (!projection.project(from, a) || !projection.project(to, b)) return;
        points.push_back(toImVec(a));
        points.push_back(toImVec(b));
      };
      for (int column = 1; column < net.columns; ++column)
      {
        const float u =
            static_cast<float>(column) / static_cast<float>(net.columns);
        strand(bilinear(u, 0.0f), bilinear(u, 1.0f));
      }
      for (int row = 1; row < net.rows; ++row)
      {
        const float v = static_cast<float>(row) / static_cast<float>(net.rows);
        strand(bilinear(0.0f, v), bilinear(1.0f, v));
      }
      primitive.pointCount =
          static_cast<std::uint16_t>(points.size() - primitive.firstPoint);
      const float depth = (corners[0].depth + corners[1].depth +
                           corners[2].depth + corners[3].depth) *
                          0.25f;
      keys.push_back(
          {depth, depth, static_cast<std::uint32_t>(primitives.size())});
      primitives.push_back(primitive);
    }
    for (const auto& support : goal.supports)
    {
      const float depth = projection.depth((support[0] + support[1]) * 0.5f);
      addLine(support[0], support[1], Tuning::Goal::SUPPORT_COLOR,
              Tuning::Goal::POST_SIZE * 0.5f, depth);
    }
    for (const Stadium3D::Box& part : goal.frame)
    {
      const float depth =
          projection.depth((part.corners[0] + part.corners[7]) * 0.5f);
      addBox(part.corners, part.color, depth);
    }
  }
}

void MatchRenderer3D::State::addCornerFlags()
{
  using C = Tuning::CornerFlag;
  for (const Vec3& base : geometry.cornerFlags)
  {
    const Vec3 top = base + UP * C::POLE_HEIGHT;
    const float depth = projection.depth(top);
    addLine(base, top, C::POLE_COLOR, C::POLE_WIDTH, depth);
    // The flag streams away from the pitch and ripples gently.
    const Vec3 outward =
        RenderMath::normalize(Vec3{base.x < LENGTH * 0.5f ? -1.0f : 1.0f,
                                   base.y < WIDTH * 0.5f ? -1.0f : 1.0f, 0.0f});
    const float wave =
        std::sin(elapsedSeconds * C::WAVE_SPEED + base.x + base.y) *
        C::WAVE_AMPLITUDE;
    const std::array<Vec3, 3> flag{
        top, top - UP * C::FLAG_HEIGHT,
        top + outward * C::FLAG_LENGTH - UP * (C::FLAG_HEIGHT * 0.5f + wave)};
    std::array<ScreenPoint, 3> screen{};
    bool visible = true;
    for (std::size_t index = 0; index < flag.size() && visible; ++index)
      visible = projection.project(flag[index], screen[index]);
    if (visible) addPolygon(screen, C::FLAG_COLOR, depth, depth - 0.01f);
  }
}

void MatchRenderer3D::State::addBoards()
{
  using B = Tuning::Boards;
  for (std::uint32_t index = 0;
       index < static_cast<std::uint32_t>(geometry.boards.size()); ++index)
  {
    const Stadium3D::AdBoard& board = geometry.boards[index];
    const std::array<Vec3, 4> corners{
        board.origin, board.origin + board.right * B::LENGTH,
        board.origin + board.right * B::LENGTH + UP * B::HEIGHT,
        board.origin + UP * B::HEIGHT};
    std::array<ScreenPoint, 4> screen{};
    bool visible = true;
    for (std::size_t corner = 0; corner < 4 && visible; ++corner)
      visible = projection.project(corners[corner], screen[corner]);
    if (!visible) continue;
    const float depth = (screen[0].depth + screen[1].depth + screen[2].depth +
                         screen[3].depth) *
                        0.25f;
    const bool front =
        RenderMath::dot(board.normal, projection.eye - board.origin) > 0.0f;
    if (!front)
    {
      addPolygon(screen, B::BACK_COLOR, depth, depth);
      continue;
    }
    Primitive primitive;
    primitive.firstPoint = static_cast<std::uint32_t>(points.size());
    primitive.pointCount = 4;
    primitive.kind = PrimitiveKind::BOARD;
    primitive.reference = index;
    for (const ScreenPoint& point : screen) points.push_back(toImVec(point));
    keys.push_back(
        {depth, depth, static_cast<std::uint32_t>(primitives.size())});
    primitives.push_back(primitive);
  }
}

void MatchRenderer3D::State::addBall(const MatchRenderSnapshot& snapshot,
                                     float alpha)
{
  using B = Tuning::Ball;
  const Vector2F ball = lerpRenderPosition(
      snapshot.ball.previousPosition, snapshot.ball.currentPosition, alpha);
  const float height = ballHeightMetres(snapshot.ball, alpha);
  ScreenPoint screen;
  if (!projection.project(
          RenderMath::worldFromPitch(ball, height + B::RADIUS), screen))
    return;
  // True size up close; a gentle boost with distance keeps it readable on
  // wide shots without ever looking oversized.
  const float boost =
      1.0f + (B::MAX_BOOST - 1.0f) *
                 std::clamp((screen.depth - B::BOOST_START_DEPTH) /
                                (B::BOOST_FULL_DEPTH - B::BOOST_START_DEPTH),
                            0.0f, 1.0f);
  Primitive primitive;
  primitive.firstPoint = static_cast<std::uint32_t>(points.size());
  primitive.pointCount = 1;
  primitive.kind = PrimitiveKind::BALL;
  primitive.size =
      std::max(B::MIN_PIXELS,
               B::RADIUS * boost * projection.focalPixels / screen.depth);
  points.push_back(toImVec(screen));
  keys.push_back({screen.depth, screen.depth,
                  static_cast<std::uint32_t>(primitives.size())});
  primitives.push_back(primitive);
}

void MatchRenderer3D::State::drawBoard(const Primitive& primitive)
{
  using B = Tuning::Boards;
  const Stadium3D::AdBoard& board = geometry.boards[primitive.reference];
  const auto rotation =
      static_cast<std::size_t>(elapsedSeconds / B::ROTATE_SECONDS);
  const Stadium3D::SponsorStyle& style =
      Stadium3D::SPONSORS[(board.sponsor + rotation) %
                          Stadium3D::SPONSORS.size()];
  const ImVec2* quad = points.data() + primitive.firstPoint;
  const std::array<ImVec2, 4> corners{quad[0], quad[1], quad[2], quad[3]};
  const std::array<ImU32, 4> colors{
      shadeColor(style.background, 0.8f), shadeColor(style.background, 0.8f),
      shadeColor(style.background, 1.15f), shadeColor(style.background, 1.15f)};
  emitRaw(corners, colors);

  // Fit the lettering box on the board, then decide how it can be shown
  // from its projected height at the board centre.
  ImFont* font = ImGui::GetFont();
  const ImVec2 unitSize =
      font->CalcTextSizeA(B::MIN_FONT_SIZE, FLT_MAX, 0.0f, style.text);
  if (unitSize.x <= 0.0f || unitSize.y <= 0.0f) return;
  float textHeight = B::HEIGHT * B::TEXT_HEIGHT_RATIO;
  float textWidth = textHeight * unitSize.x / unitSize.y;
  const float maximumWidth = B::LENGTH * B::TEXT_WIDTH_RATIO;
  if (textWidth > maximumWidth)
  {
    textHeight *= maximumWidth / textWidth;
    textWidth = maximumWidth;
  }
  const Vec3 textOrigin = board.origin +
                          board.right * ((B::LENGTH - textWidth) * 0.5f) +
                          UP * ((B::HEIGHT - textHeight) * 0.5f);
  const Vec3 textCentre =
      textOrigin + board.right * (textWidth * 0.5f) + UP * (textHeight * 0.5f);
  ScreenPoint centre;
  if (!projection.project(textCentre, centre)) return;
  const float textPixels = textHeight * projection.focalPixels / centre.depth;
  if (textPixels < B::MIN_TEXT_PIXELS)
  {
    drawBoardLogo(board, style, textOrigin, textWidth, textHeight);
    return;
  }

  // Bake the glyphs close to their on-screen size (quantised so only a few
  // sizes are ever cached) and map each vertex onto the board plane, so the
  // lettering stays crisp and follows the perspective.
  const float step = std::round(std::log(textPixels / B::MIN_FONT_SIZE) /
                                std::log(B::FONT_STEP));
  const float fontSize = std::min(
      B::MIN_FONT_SIZE * std::pow(B::FONT_STEP, step), B::MAX_FONT_SIZE);
  const ImVec2 textSize =
      font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, style.text);
  if (textSize.x <= 0.0f || textSize.y <= 0.0f) return;
  const ImVec2 canonical{projection.rect.x, projection.rect.y};
  const ImVec4 unclipped{-1e6f, -1e6f, 1e6f, 1e6f};
  const int firstVertex = drawList->VtxBuffer.Size;
  drawList->AddText(font, fontSize, canonical, style.foreground, style.text,
                    nullptr, 0.0f, &unclipped);
  for (int index = firstVertex; index < drawList->VtxBuffer.Size; ++index)
  {
    ImDrawVert& vertex = drawList->VtxBuffer[index];
    const float u = (vertex.pos.x - canonical.x) / textSize.x;
    const float v = (vertex.pos.y - canonical.y) / textSize.y;
    const Vec4 clip =
        projection.toClip(textOrigin + board.right * (u * textWidth) +
                          UP * ((1.0f - v) * textHeight));
    const ScreenPoint mapped = projection.clipToScreen(
        {clip.x, clip.y, clip.z, std::max(clip.w, projection.nearPlane)});
    vertex.pos = {mapped.x, mapped.y};
  }
}

void MatchRenderer3D::State::drawBoardLogo(const Stadium3D::AdBoard& board,
                                           const Stadium3D::SponsorStyle& style,
                                           Vec3 origin, float width,
                                           float height)
{
  // Too small to read: a clean two-tone word mark in the sponsor's colours.
  using B = Tuning::Boards;
  const float barHeight = height * B::LOGO_BAR_HEIGHT;
  const Vec3 bottom = origin + UP * ((height - barHeight) * 0.5f);
  const float markWidth = width * B::LOGO_MARK_SHARE;
  const auto bar = [&](float from, float to, ImU32 color)
  {
    const std::array<Vec3, 4> corners{
        bottom + board.right * from, bottom + board.right * to,
        bottom + board.right * to + UP * barHeight,
        bottom + board.right * from + UP * barHeight};
    const std::array<ImU32, 4> colors{color, color, color, color};
    drawWorldPolygon(corners, colors, false);
  };
  bar(0.0f, markWidth * 0.8f, mixColor(style.foreground, style.background,
                                       0.35f));
  bar(markWidth, width, style.foreground);
}

void MatchRenderer3D::State::flushSorted()
{
  std::sort(keys.begin(), keys.end(),
            [](const SortKey& a, const SortKey& b)
            {
              if (a.objectDepth != b.objectDepth)
                return a.objectDepth > b.objectDepth;
              return a.localDepth > b.localDepth;
            });
  for (const SortKey& key : keys)
  {
    const Primitive& primitive = primitives[key.primitive];
    const ImVec2* first = points.data() + primitive.firstPoint;
    switch (primitive.kind)
    {
      case PrimitiveKind::POLYGON:
        drawList->AddConvexPolyFilled(first, primitive.pointCount,
                                      primitive.color);
        break;
      case PrimitiveKind::LINE:
        drawList->AddLine(first[0], first[1], primitive.color, primitive.size);
        break;
      case PrimitiveKind::HEAD:
      {
        const float radius = std::max(primitive.size, 1.0f);
        drawList->AddCircleFilled(first[0], radius,
                                  shadeColor(primitive.color, 0.78f));
        drawList->AddCircleFilled(
            {first[0].x - radius * 0.18f, first[0].y - radius * 0.12f},
            radius * 0.74f, primitive.color);
        drawList->PathArcTo(first[0], radius * 1.04f,
                            std::numbers::pi_v<float> + 0.2f, TWO_PI - 0.2f);
        drawList->PathFillConvex(primitive.secondaryColor);
        break;
      }
      case PrimitiveKind::BALL:
      {
        const float radius = primitive.size;
        drawList->AddCircleFilled(first[0], radius, Tuning::Ball::SHADE_COLOR);
        drawList->AddCircleFilled(
            {first[0].x - radius * 0.14f, first[0].y - radius * 0.14f},
            radius * 0.82f, Tuning::Ball::COLOR);
        drawList->AddCircleFilled(
            {first[0].x + radius * 0.2f, first[0].y + radius * 0.1f},
            radius * 0.28f, Tuning::Ball::PATCH_COLOR);
        drawList->AddCircleFilled(
            {first[0].x - radius * 0.38f, first[0].y - radius * 0.4f},
            radius * 0.22f, IM_COL32_WHITE);
        break;
      }
      case PrimitiveKind::NET:
      {
        drawList->AddConvexPolyFilled(first, 4, primitive.color);
        for (std::uint16_t index = 4; index + 1 < primitive.pointCount;
             index = static_cast<std::uint16_t>(index + 2))
        {
          drawList->AddLine(first[index], first[index + 1],
                            primitive.secondaryColor, 1.0f);
        }
        break;
      }
      case PrimitiveKind::BOARD:
        drawBoard(primitive);
        break;
    }
  }
}

void MatchRenderer3D::State::drawVignette()
{
  const float x0 = projection.rect.x;
  const float y0 = projection.rect.y;
  const float x1 = x0 + projection.rect.width;
  const float y1 = y0 + projection.rect.height;
  const float edgeX = projection.rect.width * Tuning::Sky::VIGNETTE_EDGE;
  const float edgeY = projection.rect.height * Tuning::Sky::VIGNETTE_EDGE;
  const ImU32 dark = Tuning::Sky::VIGNETTE_COLOR;
  const ImU32 clear = withAlpha(dark, 0);
  drawList->AddRectFilledMultiColor({x0, y0}, {x1, y0 + edgeY}, dark, dark,
                                    clear, clear);
  drawList->AddRectFilledMultiColor({x0, y1 - edgeY}, {x1, y1}, clear, clear,
                                    dark, dark);
  drawList->AddRectFilledMultiColor({x0, y0}, {x0 + edgeX, y1}, dark, clear,
                                    clear, dark);
  drawList->AddRectFilledMultiColor({x1 - edgeX, y0}, {x1, y1}, clear, dark,
                                    dark, clear);
}

void MatchRenderer3D::State::drawLabelsAndHover(
    const MatchRenderOptions& options)
{
  const ImVec2 mouse = ImGui::GetMousePos();
  const bool mouseInView =
      mouse.x >= projection.rect.x &&
      mouse.x <= projection.rect.x + projection.rect.width &&
      mouse.y >= projection.rect.y &&
      mouse.y <= projection.rect.y + projection.rect.height;
  const PlayerOnScreen* hovered = nullptr;
  for (const PlayerOnScreen& entry : playersOnScreen)
  {
    const float halfWidth = std::max((entry.feet.y - entry.headTop.y) * 0.3f,
                                     Tuning::Hud::HOVER_PADDING);
    if (mouseInView && mouse.x >= entry.feet.x - halfWidth &&
        mouse.x <= entry.feet.x + halfWidth &&
        mouse.y >= entry.headTop.y - Tuning::Hud::HOVER_PADDING &&
        mouse.y <= entry.feet.y + Tuning::Hud::HOVER_PADDING &&
        (!hovered || entry.depth < hovered->depth))
      hovered = &entry;
  }

  for (const PlayerOnScreen& entry : playersOnScreen)
  {
    const Player* source = entry.player->player;
    if (!source) continue;
    if (!options.showPlayerNames && !entry.player->possessesBall &&
        &entry != hovered)
      continue;
    const std::string& name = source->getLastName();
    const ImVec2 size = ImGui::CalcTextSize(name.c_str());
    const ImVec2 position{entry.headTop.x - size.x * 0.5f,
                          entry.headTop.y - Tuning::Hud::LABEL_OFFSET - size.y};
    const float padding = Tuning::Hud::LABEL_PADDING;
    drawList->AddRectFilled(
        {position.x - padding, position.y - padding * 0.5f},
        {position.x + size.x + padding, position.y + size.y + padding * 0.5f},
        Tuning::Hud::LABEL_BACK_COLOR, Tuning::Hud::ROUNDING);
    drawList->AddText(position, Tuning::Hud::TEXT_COLOR, name.c_str());
  }

  if (hovered && hovered->player->player)
  {
    const MatchRenderPlayer& player = *hovered->player;
    ImGui::BeginTooltip();
    ImGui::TextUnformatted(player.player->getName().c_str());
    ImGui::TextUnformatted(
        fmt::sprintf(LOC("MATCH_PLAYER_TOOLTIP_SHORT"),
                     RoleUtils::shortName(player.player->getRole()),
                     playerIntentLabel(player.intent),
                     static_cast<double>(player.stamina * 100.0f))
            .c_str());
    ImGui::EndTooltip();
  }
}

void MatchRenderer3D::State::drawScoreBug(const MatchRenderSnapshot& snapshot,
                                          const MatchRenderOptions& options)
{
  if (!options.homeLabel || !options.awayLabel) return;
  using H = Tuning::Hud;
  char score[16];
  std::snprintf(score, sizeof(score), "%d - %d", snapshot.homeScore,
                snapshot.awayScore);
  const int minute = static_cast<int>(snapshot.matchTimeMinutes);
  const int second = static_cast<int>(
      (snapshot.matchTimeMinutes - static_cast<float>(minute)) * 60.0f);
  char clock[16];
  std::snprintf(clock, sizeof(clock), "%02d:%02d", minute, second);

  const ImVec2 homeSize = ImGui::CalcTextSize(options.homeLabel);
  const ImVec2 awaySize = ImGui::CalcTextSize(options.awayLabel);
  const ImVec2 scoreSize = ImGui::CalcTextSize(score);
  const ImVec2 clockSize = ImGui::CalcTextSize(clock);
  const float height = homeSize.y + H::PADDING * 2.0f;
  float x = projection.rect.x + H::MARGIN;
  const float y = projection.rect.y + H::MARGIN;

  const auto panel = [&](float width, ImU32 color)
  {
    drawList->AddRectFilled({x, y}, {x + width, y + height}, color,
                            H::ROUNDING);
  };
  const auto team = [&](const char* label, ImVec2 size, ImU32 kitColor)
  {
    const float width = H::CHIP_WIDTH + size.x + H::PADDING * 3.0f;
    panel(width, H::PANEL_COLOR);
    drawList->AddRectFilled(
        {x + H::PADDING, y + H::PADDING},
        {x + H::PADDING + H::CHIP_WIDTH, y + height - H::PADDING}, kitColor);
    drawList->AddText({x + H::CHIP_WIDTH + H::PADDING * 2.0f, y + H::PADDING},
                      H::TEXT_COLOR, label);
    x += width;
  };
  team(options.homeLabel, homeSize, kits.home.shirt);
  const float scoreWidth = scoreSize.x + H::PADDING * 2.0f;
  panel(scoreWidth, H::SCORE_PANEL_COLOR);
  drawList->AddText({x + H::PADDING, y + H::PADDING}, H::SCORE_TEXT_COLOR,
                    score);
  x += scoreWidth;
  team(options.awayLabel, awaySize, kits.away.shirt);
  x += H::PADDING;
  const float clockWidth = clockSize.x + H::PADDING * 2.0f;
  panel(clockWidth, H::PANEL_COLOR);
  drawList->AddText({x + H::PADDING, y + H::PADDING}, H::TEXT_COLOR, clock);
}

MatchRenderer3D::MatchRenderer3D() : state(std::make_unique<State>()) {}

MatchRenderer3D::~MatchRenderer3D() = default;

void MatchRenderer3D::render(const MatchRenderSnapshot& snapshot,
                             const MatchRenderOptions& options,
                             const MatchViewport& viewport)
{
  ImDrawList* drawList = ImGui::GetWindowDrawList();
  if (!drawList || viewport.width < 2.0f || viewport.height < 2.0f) return;
  State& s = *state;
  s.drawList = drawList;
  s.whitePixel = ImGui::GetFontTexUvWhitePixel();
  const float deltaSeconds =
      std::clamp(options.frameSeconds, 0.0f, Tuning::Camera::MAX_FRAME_SECONDS);
  s.elapsedSeconds += deltaSeconds;

  s.prepareMatch(snapshot);
  if (s.slots.size() != snapshot.players.size())
    s.slots.resize(snapshot.players.size());
  const MatchCameraFocus focus = s.computeFocus(snapshot, deltaSeconds);
  // Mouse input maps through the frame the user was looking at.
  s.camera.update(focus, options.cameraMode,
                  s.cameraControl(options.cameraInput), deltaSeconds);
  s.projection = RenderMath::Projection::make(
      s.camera.eye(), s.camera.target(), s.camera.verticalFov(),
      Tuning::Camera::NEAR_PLANE, Tuning::Camera::FAR_PLANE,
      {viewport.x, viewport.y, viewport.width, viewport.height});
  s.hasProjection = true;

  drawList->PushClipRect(
      {viewport.x, viewport.y},
      {viewport.x + viewport.width, viewport.y + viewport.height}, true);
  s.drawSky();
  for (const Stadium3D::GroundPolygon& polygon : s.geometry.ground)
  {
    s.drawWorldPolygon(
        std::span<const Vec3>(polygon.points.data(), polygon.count),
        std::span<const ImU32>(polygon.colors.data(), polygon.count), false);
  }
  for (const Stadium3D::GroundPolygon& polygon : s.geometry.markings)
  {
    s.drawWorldPolygon(
        std::span<const Vec3>(polygon.points.data(), polygon.count),
        std::span<const ImU32>(polygon.colors.data(), polygon.count), true);
  }
  const float alpha = snapshot.interpolationAlpha;
  s.drawShadows(snapshot, alpha);
#ifdef DEBUG
  if (options.showAiDebug)
  {
    for (const MatchRenderPlayer& player : snapshot.players)
    {
      ScreenPoint from;
      ScreenPoint to;
      if (s.projection.project(
              RenderMath::worldFromPitch(lerpRenderPosition(
                  player.previousPosition, player.currentPosition, alpha)),
              from) &&
          s.projection.project(
              RenderMath::worldFromPitch(player.movementTarget), to))
      {
        drawList->AddLine(toImVec(from), toImVec(to),
                          Tuning::Hud::DEBUG_TARGET_COLOR, 1.5f);
      }
    }
  }
#endif
  s.drawStadium();

  s.points.clear();
  s.primitives.clear();
  s.keys.clear();
  s.playersOnScreen.clear();
  for (std::size_t index = 0; index < snapshot.players.size(); ++index)
    s.addPlayer(snapshot.players[index], s.slots[index], alpha, deltaSeconds);
  s.addGoals();
  s.addCornerFlags();
  s.addBoards();
  s.addBall(snapshot, alpha);
  s.flushSorted();

  s.drawVignette();
  s.drawLabelsAndHover(options);
  s.drawScoreBug(snapshot, options);
  if (snapshot.state == MatchState::GOAL)
  {
    drawGoalCelebration(*drawList, viewport, snapshot.homeScore,
                        snapshot.awayScore, snapshot.goalCelebrationRemaining);
  }
  drawList->PopClipRect();
}
