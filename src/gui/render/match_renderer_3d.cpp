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
#include <string>
#include <vector>

#include "global/language_manager.h"
#include "gui/render/match_camera_3d.h"
#include "gui/render/match_kit_colors.h"
#include "gui/render/match_render_3d_tuning.h"
#include "gui/render/match_render_math.h"
#include "gui/render/match_renderer_2d.h"
#include "gui/render/match_shirt_numbers.h"
#include "gui/render/match_stadium_3d.h"
#include "gui/view_models/match_clock.h"
#include "gui/widgets/theme.h"
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
constexpr int BATCH_CHUNK_VERTICES = 2048;
/** Spectator heads: a hexagon (across, up) on the unit circle. */
constexpr std::size_t HEAD_SIDES = 6;
constexpr std::array<std::array<float, 2>, HEAD_SIDES> HEAD_SHAPE{{
    {1.0f, 0.0f},
    {0.5f, 0.866f},
    {-0.5f, 0.866f},
    {-1.0f, 0.0f},
    {-0.5f, -0.866f},
    {0.5f, -0.866f},
}};

enum class PrimitiveKind : std::uint8_t
{
  POLYGON,
  LINE,
  HEAD,
  BALL,
  NET,
  BOARD,
  TEXT,
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
  /** Ground speed seen last frame and the smoothed acceleration (m/s^2). */
  float lastSpeed = 0.0f;
  float acceleration = 0.0f;
  /** Smoothed ground speed choosing the walk / jog / sprint blend. */
  float gaitSpeed = 0.0f;
  float lastYaw = 0.0f;
  bool hasYaw = false;
  /** Smoothed turn rate (rad per simulated second, positive = left). */
  float turnRate = 0.0f;
  /** Simulated seconds left in a kick or header, and its shape. */
  float kickSeconds = 0.0f;
  bool header = false;
  bool leftFooted = false;
  /** Keeper pose blends (0..1) and the side of the current dive. */
  float setBlend = 0.0f;
  float holdBlend = 0.0f;
  float diveBlend = 0.0f;
  float diveSide = 1.0f;
  /** Printed on the back: shirt number and upper-case surname. */
  int number = 0;
  std::string backName;
};

/**
 * Joint angles of one frame, blended from the run cycle and the overlays
 * (kick, keeper stances, celebrations). Index 0 is the player's left side.
 */
struct Pose
{
  float lean = 0.0f;
  float turnRoll = 0.0f;
  float bodyRoll = 0.0f;
  float hipDrop = 0.0f;
  float lift = 0.0f;
  std::array<float, 2> thigh{};
  std::array<float, 2> knee{};
  std::array<float, 2> legSpread{};
  std::array<float, 2> armSwing{};
  std::array<float, 2> armSpread{};
  std::array<float, 2> elbow{};
};

Pose mixPose(const Pose& from, const Pose& to, float t)
{
  if (t <= 0.0f) return from;
  if (t >= 1.0f) return to;
  const auto mix = [t](float a, float b) { return a + (b - a) * t; };
  Pose result;
  result.lean = mix(from.lean, to.lean);
  result.turnRoll = mix(from.turnRoll, to.turnRoll);
  result.bodyRoll = mix(from.bodyRoll, to.bodyRoll);
  result.hipDrop = mix(from.hipDrop, to.hipDrop);
  result.lift = mix(from.lift, to.lift);
  for (std::size_t index = 0; index < 2; ++index)
  {
    result.thigh[index] = mix(from.thigh[index], to.thigh[index]);
    result.knee[index] = mix(from.knee[index], to.knee[index]);
    result.legSpread[index] = mix(from.legSpread[index], to.legSpread[index]);
    result.armSwing[index] = mix(from.armSwing[index], to.armSwing[index]);
    result.armSpread[index] = mix(from.armSpread[index], to.armSpread[index]);
    result.elbow[index] = mix(from.elbow[index], to.elbow[index]);
  }
  return result;
}

/** Orthonormal body axes a limb swings in. */
struct BodyFrame
{
  Vec3 up;
  Vec3 forward;
  Vec3 side;
};

/** How the team of a player reacts to the goal being celebrated. */
enum class GoalMood : std::uint8_t
{
  NONE,
  CELEBRATING,
  SCORER,
  DEJECTED,
};

/** The goal currently being celebrated. */
struct GoalMomentState
{
  bool active = false;
  bool byHome = false;
  /** Real seconds since the goal was first shown. */
  float shownSeconds = 0.0f;
  /** Simulated seconds of celebration so far. */
  float celebrationSeconds = 0.0f;
  PlayerID scorer = 0;
  bool ownGoal = false;
  std::string scorerName;
  std::string minute;
  /** Which goal the ball went in and where it hit the net (world). */
  std::size_t goalIndex = 0;
  Vec3 impact;
};

/** "0" to "99" for the shirts. */
constexpr auto NUMBER_TEXT = []
{
  std::array<std::array<char, 3>, 100> table{};
  for (int number = 0; number < 100; ++number)
  {
    const auto digit = [](int value)
    { return static_cast<char>('0' + value); };
    table[static_cast<std::size_t>(number)] =
        number < 10 ? std::array<char, 3>{digit(number), '\0', '\0'}
                    : std::array<char, 3>{digit(number / 10),
                                          digit(number % 10), '\0'};
  }
  return table;
}();


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

/** Key light of the scene: floodlights at night, the sun by day. */
struct LightRig
{
  Vec3 direction;
  float ambient = 0.0f;
  float diffuse = 0.0f;
  float sky = 0.0f;
};

LightRig nightLight()
{
  using L = Tuning::Light;
  return {RenderMath::normalize(
              {L::DIRECTION_X, L::DIRECTION_Y, L::DIRECTION_Z}),
          L::AMBIENT, L::DIFFUSE, L::SKY};
}

LightRig dayLight()
{
  using D = Tuning::Day;
  return {RenderMath::normalize({D::SUN_X, D::SUN_Y, D::SUN_Z}), D::AMBIENT,
          D::DIFFUSE, D::SKY};
}

float lighting(Vec3 normal, const LightRig& light)
{
  return light.ambient +
         light.diffuse *
             std::max(0.0f, RenderMath::dot(normal, light.direction)) +
         light.sky * std::max(0.0f, normal.z);
}

/** Boot colours: mostly black, some white and the usual bright ones. */
constexpr std::array<ImU32, 9> BOOT_COLORS{
    IM_COL32(26, 26, 30, 255),   IM_COL32(26, 26, 30, 255),
    IM_COL32(26, 26, 30, 255),   IM_COL32(236, 236, 232, 255),
    IM_COL32(236, 222, 40, 255), IM_COL32(246, 112, 42, 255),
    IM_COL32(44, 124, 232, 255), IM_COL32(228, 62, 124, 255),
    IM_COL32(60, 200, 150, 255)};

/** Hair styles drawn on the head disc. */
enum class HairStyle : std::uint8_t
{
  SHORT,
  BUZZ,
  VOLUME,
  LONG,
  SHAVED,
};
constexpr std::array<HairStyle, 8> HAIR_STYLES{
    HairStyle::SHORT,  HairStyle::SHORT, HairStyle::BUZZ,
    HairStyle::VOLUME, HairStyle::LONG,  HairStyle::SHORT,
    HairStyle::SHAVED, HairStyle::BUZZ};

/** Blends the walk, jog and sprint shapes for a ground speed. */
Tuning::Gait::Shape gaitShape(float speed)
{
  using G = Tuning::Gait;
  const auto mix = [](const G::Shape& a, const G::Shape& b, float t)
  {
    const auto lerp = [t](float x, float y) { return x + (y - x) * t; };
    return G::Shape{lerp(a.speed, b.speed), lerp(a.thigh, b.thigh),
                    lerp(a.knee, b.knee),   lerp(a.arm, b.arm),
                    lerp(a.elbow, b.elbow), lerp(a.lean, b.lean),
                    lerp(a.bob, b.bob)};
  };
  if (speed <= G::WALK.speed) return G::WALK;
  if (speed <= G::JOG.speed)
    return mix(G::WALK, G::JOG,
               (speed - G::WALK.speed) / (G::JOG.speed - G::WALK.speed));
  return mix(G::JOG, G::SPRINT,
             std::min(1.0f, (speed - G::JOG.speed) /
                                (G::SPRINT.speed - G::JOG.speed)));
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
  /** Real seconds of the current frame. */
  float frameSeconds = 0.0f;
  float attackDirection = 1.0f;
  float directionChangeSeconds = 0.0f;
  /** The play jumped this frame (playback skip): cameras cut, not ease. */
  bool jumped = false;
  /** Daylight preset this frame (kick-off time or the view toggle). */
  bool day = false;
  LightRig light = nightLight();
  /** Ball panel rotation and its on-screen roll direction. */
  float ballSpin = 0.0f;
  Vec3 lastBallWorld;
  ImVec2 ballRoll{1.0f, 0.0f};

  /** Kits indexed home, home keeper, away, away keeper. */
  std::array<KitColors, 4> kitList{};
  std::array<ImU32, 4> numberColors{};
  std::array<ImU32, 4> gloveColors{};
  MatchShirtNumbers numbers;

  // --- per-frame match context --------------------------------------------
  /** Simulated seconds since the previous frame (0 while paused). */
  float simSeconds = 0.0f;
  float lastMatchMinutes = -1.0f;
  bool reducedMotion = false;
  bool livePlay = false;
  Vec3 ballWorld;
  GoalkeeperState homeKeeperState = GoalkeeperState::SET_POSITION;
  GoalkeeperState awayKeeperState = GoalkeeperState::SET_POSITION;
  const std::vector<PlayerMatchStats>* playerStats = nullptr;

  // --- kick detection -----------------------------------------------------
  const Player* lastPossessor = nullptr;
  Vector2F lastBallPosition{-1.0f, -1.0f};
  float lastBallStepSpeed = 0.0f;

  // --- goal presentation --------------------------------------------------
  GoalMomentState goal;
  /** 0..1 weight of the players' goal reactions this frame. */
  float celebrationWeight = 0.0f;
  /** 0..1 weight of the crowd's goal celebration this frame. */
  float crowdCelebration = 0.0f;
  /** Idle sway and celebration hop offsets per crowd phase (metres). */
  std::array<float, Stadium3D::CROWD_PHASES> crowdSway{};
  std::array<float, Stadium3D::CROWD_PHASES> crowdHop{};
  /** CHEERS_* bit of the celebrating supporters (0: nobody). */
  std::uint8_t cheeringMask = 0;
  int batchVertices = 0;
  int batchIndices = 0;

  /** Clip-space pitch grid vertices of this frame. */
  std::vector<Vec4> pitchClip;
  std::vector<ImVec2> pitchScreen;
  /** Grid colours with the current stripe tint (rebuilt on change). */
  std::vector<ImU32> pitchColors;
  float pitchSwing = 0.0f;
  bool pitchDay = false;
  std::vector<AnimationSlot> slots;
  std::vector<ImVec2> points;
  std::vector<Primitive> primitives;
  std::vector<SortKey> keys;
  std::vector<const char*> texts;
  std::vector<std::pair<float, std::uint32_t>> sectionOrder;
  std::vector<PlayerOnScreen> playersOnScreen;

  State()
  {
    points.reserve(8192);
    primitives.reserve(2048);
    keys.reserve(2048);
    texts.reserve(64);
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
  /** Radial-gradient ellipse (dark centre, clear rim) for soft shadows. */
  void drawSoftEllipse(Vec3 centre, Vec3 axisA, Vec3 axisB, ImU32 color);
  /** The mown playing surface (vertex grid, per-frame stripe tint). */
  void drawPitch();
  void drawShadows(const MatchRenderSnapshot& snapshot, float alpha);
  void drawStadium();
  /** Crowd and pitch geometry go through chunked reservations (16-bit
   * indices); release before any other draw-list call. */
  void reserveBatch(int vertices, int indices);
  void releaseBatch();
  void batchFan(std::span<const ImVec2> screen, std::span<const ImU32> colors);
  void batchQuad(ImVec2 bottomLeft, ImVec2 bottomRight, ImVec2 topRight,
                 ImVec2 topLeft, ImU32 bottom, ImU32 top);
  /** Fills the idle-sway and goal-hop tables for this frame. */
  void updateCrowdMotion();
  void drawCrowd(const Stadium3D::Face& face);
  void drawCrowdFlags(const Stadium3D::Face& face, float upX, float upY);
  void drawVignette();

  // --- sorted layer -------------------------------------------------------
  void addPolygon(std::span<const ScreenPoint> screen, ImU32 color,
                  float objectDepth, float localDepth);
  void addLine(Vec3 from, Vec3 to, ImU32 color, float thickness,
               float objectDepth);
  void addBox(const std::array<Vec3, 8>& corners, ImU32 color,
              float objectDepth);
  /** Lit prism between two rings of the same size (up to 8 sides). */
  void addPrism(std::span<const Vec3> bottom, std::span<const Vec3> top,
                ImU32 color, float objectDepth, bool capBottom, bool capTop);
  /** Advances a player's gait, pose memory and keeper stances. */
  void updateMotion(const MatchRenderPlayer& player, AnimationSlot& slot,
                    float alpha, float deltaSeconds);
  int shirtNumberFor(const MatchRenderPlayer& player);
  GoalMood goalMoodFor(const MatchRenderPlayer& player) const;
  void addPlayer(const MatchRenderPlayer& player, AnimationSlot& slot,
                 float alpha);
  void addBackPrint(const AnimationSlot& slot, std::size_t kitIndex,
                    Vec3 waist, Vec3 torsoUp, Vec3 torsoForward,
                    Vec3 bodySide, float scale, float objectDepth);
  void drawBackText(const Primitive& primitive);
  /** Starts kick and header poses when the ball leaves a player. */
  void detectTouches(const MatchRenderSnapshot& snapshot);
  /** Tracks the goal being celebrated (sting, net ripple, reactions). */
  void updateGoalMoment(const MatchRenderSnapshot& snapshot);
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
  void drawGoalSting(const MatchRenderSnapshot& snapshot);
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
  kitList = {kits.home, kits.homeGoalkeeper, kits.away, kits.awayGoalkeeper};
  for (std::size_t index = 0; index < kitList.size(); ++index)
  {
    numberColors[index] = kitNumberColor(kitList[index]);
    gloveColors[index] = goalkeeperGloveColor(kitList[index]);
  }
  numbers.reset();
  slots.clear();
  goal = GoalMomentState{};
  lastPossessor = nullptr;
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
    if (directionChangeSeconds >= Tuning::Camera::ATTACK_SWITCH_SECONDS ||
        (jumped && direction != attackDirection))
    {
      attackDirection = direction;
      directionChangeSeconds = 0.0f;
    }
  }
  focus.attackDirection = attackDirection;
  focus.livePlay = snapshot.state == MatchState::PLAYING;
  focus.shotInFlight = focus.livePlay && snapshot.ball.isShot;
  focus.goalCelebration = goal.active;
  focus.reducedMotion = reducedMotion;
  if (goal.active && !goal.ownGoal && goal.scorer != 0)
  {
    for (const MatchRenderPlayer& player : snapshot.players)
    {
      if (!player.player || player.player->getId() != goal.scorer) continue;
      focus.celebration = RenderMath::worldFromPitch(lerpRenderPosition(
          player.previousPosition, player.currentPosition, alpha));
      focus.hasCelebration = true;
    }
  }
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
  const ImU32 topColor = day ? Tuning::Day::TOP_COLOR : Tuning::Sky::TOP_COLOR;
  const ImU32 middleColor =
      day ? Tuning::Day::MIDDLE_COLOR : Tuning::Sky::MIDDLE_COLOR;
  const ImU32 horizonColor =
      day ? Tuning::Day::HORIZON_COLOR : Tuning::Sky::HORIZON_COLOR;
  const std::array<ImU32, 4> upper{topColor, topColor, middleColor,
                                   middleColor};
  const std::array<ImVec2, 4> upperQuad{
      ImVec2{minimum.x, minimum.y}, ImVec2{maximum.x, minimum.y},
      ImVec2{maximum.x, middle}, ImVec2{minimum.x, middle}};
  emitRaw(upperQuad, upper);
  const std::array<ImU32, 4> lower{middleColor, middleColor, horizonColor,
                                   horizonColor};
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

void MatchRenderer3D::State::drawSoftEllipse(Vec3 centre, Vec3 axisA,
                                             Vec3 axisB, ImU32 color)
{
  // A dark core fading through a ring to a clear rim: a soft shadow with a
  // feathered edge that needs no anti-aliasing fringe. Written into the
  // chunked batch; callers release it before other draw-list calls.
  using Sh = Tuning::Shadow;
  constexpr std::size_t RIM = Sh::SEGMENTS;
  const float alpha = static_cast<float>((color >> IM_COL32_A_SHIFT) & 0xFFU);
  const ImU32 core = withAlpha(
      color, static_cast<std::uint8_t>(alpha * Sh::CORE_ALPHA_SHARE));
  const ImU32 clear = withAlpha(color, 0);
  const auto rim = [&](std::size_t index, float share)
  {
    const float angle = TWO_PI * static_cast<float>(index % RIM) /
                        static_cast<float>(RIM);
    return centre + (axisA * std::cos(angle) + axisB * std::sin(angle)) * share;
  };
  std::array<ClipVertex, RIM> outer{};
  std::array<Vec4, RIM> inner{};
  const Vec4 middle = projection.toClip(centre);
  bool allInFront = middle.w >= projection.nearPlane;
  for (std::size_t index = 0; index < RIM; ++index)
  {
    outer[index].clip = projection.toClip(rim(index, 1.0f));
    inner[index] = projection.toClip(rim(index, Sh::CORE_RADIUS_SHARE));
    allInFront = allInFront && outer[index].clip.w >= projection.nearPlane &&
                 inner[index].w >= projection.nearPlane;
  }
  if (!allInFront)
  {
    // Rare (a low free camera): clip every piece against the near plane.
    releaseBatch();
    std::array<Vec3, RIM + 2> world{};
    std::array<ImU32, RIM + 2> colors{};
    world[0] = centre;
    colors[0] = color;
    for (std::size_t index = 0; index <= RIM; ++index)
    {
      world[index + 1] = rim(index, Sh::CORE_RADIUS_SHARE);
      colors[index + 1] = core;
    }
    drawWorldPolygon(world, colors, false);
    for (std::size_t index = 0; index < RIM; ++index)
    {
      const std::array<Vec3, 4> quad{rim(index, Sh::CORE_RADIUS_SHARE),
                                     rim(index, 1.0f), rim(index + 1, 1.0f),
                                     rim(index + 1, Sh::CORE_RADIUS_SHARE)};
      const std::array<ImU32, 4> quadColors{core, clear, clear, core};
      drawWorldPolygon(quad, quadColors, false);
    }
    return;
  }
  if (outsideClipVolume(outer)) return;
  std::array<ImVec2, RIM + 1> fan{};
  std::array<ImU32, RIM + 1> fanColors{};
  std::array<ImVec2, RIM> edge{};
  fan[0] = toImVec(projection.clipToScreen(middle));
  fanColors[0] = color;
  for (std::size_t index = 0; index < RIM; ++index)
  {
    fan[index + 1] = toImVec(projection.clipToScreen(inner[index]));
    fanColors[index + 1] = core;
    edge[index] = toImVec(projection.clipToScreen(outer[index].clip));
  }
  // The fan closes on itself: centre, ring 0..RIM-1, ring 0 again.
  reserveBatch(static_cast<int>(RIM) + 1, static_cast<int>(RIM) * 3);
  const auto first = static_cast<ImDrawIdx>(drawList->_VtxCurrentIdx);
  for (std::size_t index = 0; index <= RIM; ++index)
    drawList->PrimWriteVtx(fan[index], whitePixel, fanColors[index]);
  for (std::size_t index = 0; index < RIM; ++index)
  {
    drawList->PrimWriteIdx(first);
    drawList->PrimWriteIdx(static_cast<ImDrawIdx>(first + 1 + index));
    drawList->PrimWriteIdx(
        static_cast<ImDrawIdx>(first + 1 + (index + 1) % RIM));
  }
  batchVertices -= static_cast<int>(RIM) + 1;
  batchIndices -= static_cast<int>(RIM) * 3;
  for (std::size_t index = 0; index < RIM; ++index)
  {
    const std::size_t next = (index + 1) % RIM;
    const std::array<ImVec2, 4> quad{fan[index + 1], edge[index], edge[next],
                                     fan[next + 1]};
    const std::array<ImU32, 4> quadColors{core, clear, clear, core};
    batchFan(quad, quadColors);
  }
}

void MatchRenderer3D::State::drawPitch()
{
  using G = Tuning::Grass;
  const Stadium3D::PitchGrid& grid = geometry.pitch;
  if (grid.points.empty()) return;
  // Blades leaning away from the eye catch more light: the stripe pattern
  // follows the view along the mowing direction (across the pitch).
  const float flat = std::max(
      std::hypot(projection.forward.x, projection.forward.y), 1e-3f);
  const float across = projection.forward.y / flat;
  const float contrast =
      G::STRIPE_CONTRAST *
      (G::STRIPE_SIDE_SHARE + (1.0f - G::STRIPE_SIDE_SHARE) * std::abs(across));
  // Quantised, so the tinted colours are only rebuilt when it changes.
  const float swing =
      std::round(contrast * (across >= 0.0f ? 1.0f : -1.0f) * 500.0f) / 500.0f;
  if (swing != pitchSwing || day != pitchDay ||
      pitchColors.size() != grid.colors.size())
  {
    pitchSwing = swing;
    pitchDay = day;
    pitchColors.resize(grid.colors.size());
    const float sunlit = day ? Tuning::Day::GRASS_LIGHT : 1.0f;
    for (int stripe = 0; stripe < grid.stripes; ++stripe)
    {
      const float stripeLight =
          (stripe % 2 == 0 ? 1.0f + swing : 1.0f - swing) * sunlit;
      for (int column = 0; column <= grid.columnsPerStripe; ++column)
      {
        for (int row = 0; row <= grid.rows; ++row)
        {
          const std::size_t index = grid.index(stripe, column, row);
          pitchColors[index] = shadeColor(grid.colors[index], stripeLight);
        }
      }
    }
  }
  pitchClip.resize(grid.points.size());
  pitchScreen.resize(grid.points.size());
  for (std::size_t index = 0; index < grid.points.size(); ++index)
  {
    pitchClip[index] = projection.toClip(grid.points[index]);
    if (pitchClip[index].w >= projection.nearPlane)
      pitchScreen[index] = toImVec(projection.clipToScreen(pitchClip[index]));
  }

  for (int stripe = 0; stripe < grid.stripes; ++stripe)
  {
    for (int column = 0; column < grid.columnsPerStripe; ++column)
    {
      for (int row = 0; row < grid.rows; ++row)
      {
        const std::array<std::size_t, 4> corner{
            grid.index(stripe, column, row),
            grid.index(stripe, column + 1, row),
            grid.index(stripe, column + 1, row + 1),
            grid.index(stripe, column, row + 1)};
        std::array<ClipVertex, 4> vertices{};
        bool allInFront = true;
        for (std::size_t index = 0; index < 4; ++index)
        {
          vertices[index] = {pitchClip[corner[index]],
                             pitchColors[corner[index]]};
          allInFront =
              allInFront && vertices[index].clip.w >= projection.nearPlane;
        }
        if (!allInFront)
        {
          // Rare: the cell crosses the near plane (low free camera).
          releaseBatch();
          std::array<Vec3, 4> world{};
          std::array<ImU32, 4> colors{};
          for (std::size_t index = 0; index < 4; ++index)
          {
            world[index] = grid.points[corner[index]];
            colors[index] = vertices[index].color;
          }
          drawWorldPolygon(world, colors, false);
          continue;
        }
        if (outsideClipVolume(vertices)) continue;
        std::array<ImVec2, 4> screen{};
        std::array<ImU32, 4> colors{};
        for (std::size_t index = 0; index < 4; ++index)
        {
          screen[index] = pitchScreen[corner[index]];
          colors[index] = vertices[index].color;
        }
        batchFan(screen, colors);
      }
    }
  }
  releaseBatch();
}

void MatchRenderer3D::State::drawShadows(const MatchRenderSnapshot& snapshot,
                                         float alpha)
{
  // Four floodlight towers cast four faint blades per player plus a darker
  // contact shadow: the classic night-match look. By day the sun casts one.
  using F = Tuning::Floodlight;
  using Sh = Tuning::Shadow;
  const std::array<Vec3, 4> lights{
      Vec3{-F::CORNER_OFFSET_X, -F::CORNER_OFFSET_Y, 0.0f},
      Vec3{LENGTH + F::CORNER_OFFSET_X, -F::CORNER_OFFSET_Y, 0.0f},
      Vec3{-F::CORNER_OFFSET_X, WIDTH + F::CORNER_OFFSET_Y, 0.0f},
      Vec3{LENGTH + F::CORNER_OFFSET_X, WIDTH + F::CORNER_OFFSET_Y, 0.0f}};
  const Vec3 sunAway =
      RenderMath::normalize({-light.direction.x, -light.direction.y, 0.0f});
  const float sunLength =
      Sh::SUN_LENGTH_SHARE *
      std::hypot(light.direction.x, light.direction.y) /
      std::max(light.direction.z, 0.2f);
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
      releaseBatch();
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
    const float standing = playerHeightMetres(player);
    if (day)
    {
      const float bladeLength = standing * sunLength;
      const Vec3 across{-sunAway.y, sunAway.x, 0.0f};
      drawSoftEllipse(root + sunAway * (bladeLength * 0.45f),
                      sunAway * (bladeLength * 0.55f + Sh::PENUMBRA),
                      across * (Sh::BLADE_HALF_WIDTH + Sh::PENUMBRA),
                      IM_COL32(0, 0, 0, Sh::SUN_ALPHA));
    }
    else
    {
      // Each mast throws a blade as long as its geometry dictates, darker
      // for the nearer (brighter) lamps. Up close the blades fade so the
      // four of them never read as a hard "X" under the boots.
      const float close = std::clamp(
          (pixelHeight - Sh::CLOSE_FADE_START_PIXELS) /
              (Sh::CLOSE_FADE_FULL_PIXELS - Sh::CLOSE_FADE_START_PIXELS),
          0.0f, 1.0f);
      const float closeShare = 1.0f - (1.0f - Sh::CLOSE_SHARE) * close;
      std::array<float, 4> weight{};
      float weightSum = 0.0f;
      for (std::size_t index = 0; index < lights.size(); ++index)
      {
        const Vec3 offset = root - lights[index];
        weight[index] = 1.0f / std::max(RenderMath::dot(offset, offset), 1.0f);
        weightSum += weight[index];
      }
      for (std::size_t index = 0; index < lights.size(); ++index)
      {
        Vec3 away = root - lights[index];
        const float reach = std::hypot(away.x, away.y);
        away = RenderMath::normalize(away);
        const Vec3 across{-away.y, away.x, 0.0f};
        const float bladeLength = std::clamp(
            standing * reach / F::MAST_HEIGHT, Sh::MIN_BLADE, Sh::MAX_BLADE);
        const float share = std::clamp(
            weight[index] / weightSum * static_cast<float>(lights.size()),
            Sh::MIN_LAMP_SHARE, Sh::MAX_LAMP_SHARE);
        const auto darkness = static_cast<std::uint8_t>(
            static_cast<float>(Sh::BLADE_ALPHA) * share * closeShare);
        drawSoftEllipse(root + away * (bladeLength * 0.45f),
                        away * (bladeLength * 0.55f + Sh::PENUMBRA),
                        across * (Sh::BLADE_HALF_WIDTH + Sh::PENUMBRA),
                        IM_COL32(0, 0, 0, darkness));
      }
    }
    drawSoftEllipse(root, {Sh::CONTACT_RADIUS, 0.0f, 0.0f},
                    {0.0f, Sh::CONTACT_RADIUS, 0.0f}, Sh::CONTACT_COLOR);
    if (player.possessesBall)
    {
      releaseBatch();
      const float radius =
          Sh::RING_RADIUS *
          (1.0f + (reducedMotion ? 0.0f
                                 : Sh::RING_PULSE *
                                       std::sin(elapsedSeconds *
                                                Sh::RING_PULSE_SPEED)));
      std::array<ImVec2, Sh::RING_SEGMENTS> ring{};
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
                              Sh::RING_COLOR, ImDrawFlags_Closed,
                              Sh::RING_THICKNESS);
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
  // By day the lifted ball's shadow slides away from the sun.
  const Vec3 slide = day ? sunAway * (height * sunLength) : Vec3{};
  drawSoftEllipse(RenderMath::worldFromPitch(ball) + slide,
                  {radius, 0.0f, 0.0f}, {0.0f, radius, 0.0f},
                  IM_COL32(0, 0, 0, shadowAlpha));
  releaseBatch();
}

void MatchRenderer3D::State::reserveBatch(int vertices, int indices)
{
  if (vertices <= batchVertices && indices <= batchIndices) return;
  releaseBatch();
  // Chunks stay far below the 16-bit index limit of one reservation.
  batchVertices = std::max(vertices, BATCH_CHUNK_VERTICES);
  batchIndices = std::max(indices, BATCH_CHUNK_VERTICES * 3 / 2);
  drawList->PrimReserve(batchIndices, batchVertices);
}

void MatchRenderer3D::State::releaseBatch()
{
  if (batchVertices > 0 || batchIndices > 0)
    drawList->PrimUnreserve(batchIndices, batchVertices);
  batchVertices = 0;
  batchIndices = 0;
}

void MatchRenderer3D::State::batchFan(std::span<const ImVec2> screen,
                                      std::span<const ImU32> colors)
{
  const auto count = static_cast<int>(screen.size());
  reserveBatch(count, (count - 2) * 3);
  const auto first = static_cast<ImDrawIdx>(drawList->_VtxCurrentIdx);
  for (int index = 0; index < count; ++index)
    drawList->PrimWriteVtx(screen[static_cast<std::size_t>(index)], whitePixel,
                           colors[static_cast<std::size_t>(index)]);
  for (int index = 2; index < count; ++index)
  {
    drawList->PrimWriteIdx(first);
    drawList->PrimWriteIdx(static_cast<ImDrawIdx>(first + index - 1));
    drawList->PrimWriteIdx(static_cast<ImDrawIdx>(first + index));
  }
  batchVertices -= count;
  batchIndices -= (count - 2) * 3;
}

void MatchRenderer3D::State::batchQuad(ImVec2 bottomLeft, ImVec2 bottomRight,
                                       ImVec2 topRight, ImVec2 topLeft,
                                       ImU32 bottom, ImU32 top)
{
  const std::array<ImVec2, 4> quad{bottomLeft, bottomRight, topRight, topLeft};
  const std::array<ImU32, 4> colors{bottom, bottom, top, top};
  batchFan(quad, colors);
}

void MatchRenderer3D::State::updateCrowdMotion()
{
  using C = Tuning::Crowd;
  // One small table per frame; every spectator reads its phase from it.
  for (std::size_t phase = 0; phase < Stadium3D::CROWD_PHASES; ++phase)
  {
    const float offset = TWO_PI * static_cast<float>(phase) /
                         static_cast<float>(Stadium3D::CROWD_PHASES);
    const float rate = 0.75f + 0.5f * static_cast<float>((phase * 7U) % 16U) /
                                   16.0f;
    crowdSway[phase] =
        reducedMotion
            ? 0.0f
            : C::SWAY * std::sin(elapsedSeconds * C::SWAY_SPEED * rate +
                                 offset);
    crowdHop[phase] =
        reducedMotion
            ? 0.0f
            : C::HOP * crowdCelebration *
                  std::abs(std::sin(elapsedSeconds * C::HOP_SPEED *
                                        (0.85f + 0.3f * rate) +
                                    offset * 3.0f));
  }
  cheeringMask = 0;
  if (goal.active && crowdCelebration > 0.0f)
  {
    cheeringMask = goal.byHome ? Stadium3D::CrowdFlags::CHEERS_HOME
                               : Stadium3D::CrowdFlags::CHEERS_AWAY;
  }
}

void MatchRenderer3D::State::drawCrowd(const Stadium3D::Face& face)
{
  using C = Tuning::Crowd;
  namespace Flags = Stadium3D::CrowdFlags;
  if (face.clumpEnd <= face.clumpBegin) return;
  {
    // Skip the whole tier when it (heads and flags included) is off screen.
    std::array<ClipVertex, 8> bounds{};
    bool anyInFront = false;
    for (std::size_t corner = 0; corner < 4; ++corner)
    {
      bounds[corner].clip = projection.toClip(face.corners[corner]);
      bounds[corner + 4].clip = projection.toClip(
          face.corners[corner] + UP * (C::DOT_HEIGHT + C::POLE_HEIGHT));
      anyInFront = anyInFront ||
                   bounds[corner].clip.w >= projection.nearPlane ||
                   bounds[corner + 4].clip.w >= projection.nearPlane;
    }
    bool allInFront = true;
    for (const ClipVertex& vertex : bounds)
      allInFront = allInFront && vertex.clip.w >= projection.nearPlane;
    if (!anyInFront || (allInFront && outsideClipVolume(bounds))) return;
  }
  const Vec3 centre = (face.corners[0] + face.corners[2]) * 0.5f;
  ScreenPoint centreScreen;
  ScreenPoint aboveScreen;
  if (!projection.project(centre, centreScreen) ||
      !projection.project(centre + UP, aboveScreen))
    return;
  // Screen-space "up" per metre at the face centre, rescaled per depth.
  const float upX = (aboveScreen.x - centreScreen.x) * centreScreen.depth;
  const float upY = (aboveScreen.y - centreScreen.y) * centreScreen.depth;
  const float upLength = std::max(std::hypot(upX, upY), 1e-3f);
  // Screen "across" is perpendicular to up, so tilted views stay upright.
  const float acrossX = -upY / upLength;
  const float acrossY = upX / upLength;
  const float left = projection.rect.x;
  const float right = projection.rect.x + projection.rect.width;
  const float top = projection.rect.y;
  const float bottom = projection.rect.y + projection.rect.height;
  const float seatFocal = C::SEAT_SPACING * projection.focalPixels;
  const float halfDotFocal = C::DOT_WIDTH * 0.5f * projection.focalPixels;
  const bool cheering = cheeringMask != 0U;
  const auto point = [&](float x, float y, float across, float up, float dx,
                         float dy)
  {
    return ImVec2{x + acrossX * across + dx * up,
                  y + acrossY * across + dy * up};
  };

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
      ScreenPoint base = projection.clipToScreen(clumpClip);
      ScreenPoint upper = projection.clipToScreen(topClip);
      const float topHalf =
          clump.halfWidth * projection.focalPixels / topClip.w;
      if (std::max(base.x, upper.x) + halfWidth < left ||
          std::min(base.x, upper.x) - halfWidth > right ||
          std::min(base.y, upper.y) > bottom ||
          std::max(base.y, upper.y) < top)
        continue;
      if (cheering && (clump.flags & cheeringMask) != 0U)
      {
        const float hop = crowdHop[clump.phase] * C::DOT_HEIGHT * clumpInverse;
        upper.x += upX * hop;
        upper.y += upY * hop;
      }
      batchQuad({base.x - halfWidth, base.y}, {base.x + halfWidth, base.y},
                {upper.x + topHalf, upper.y}, {upper.x - topHalf, upper.y},
                shadeColor256(clump.color, 218U), clump.color);
      continue;
    }
    {
      // The clump's own bounds first: most of a stand is off screen.
      const ScreenPoint base = projection.clipToScreen(clumpClip);
      const float reach = (clump.halfWidth + C::DOT_WIDTH) *
                          projection.focalPixels * clumpInverse;
      const float rise =
          (C::DOT_HEIGHT * 2.0f +
           C::ROW_DEPTH * static_cast<float>(C::CLUMP_ROWS)) *
          projection.focalPixels * clumpInverse;
      if (base.x + reach < left || base.x - reach > right ||
          base.y - rise > bottom || base.y + rise < top)
        continue;
    }
    // Near: individual spectators whose colours fade towards the clump
    // average while they are still small (cheap mip-mapping).
    const auto detail = static_cast<std::uint32_t>(
        256.0f * std::clamp((seatPixels - C::CLUMP_PIXELS) /
                                (C::FULL_DETAIL_PIXELS - C::CLUMP_PIXELS),
                            0.0f, 1.0f));
    for (std::uint32_t index = clump.dotBegin; index < clump.dotEnd; ++index)
    {
      const Stadium3D::CrowdDot& dot = geometry.crowd[index];
      const Vec4 clip = projection.toClip(dot.base);
      if (clip.w < projection.nearPlane) continue;
      ScreenPoint base = projection.clipToScreen(clip);
      const float inverseDepth = 1.0f / clip.w;
      const float halfWidth = halfDotFocal * inverseDepth;
      const float dx = upX * C::DOT_HEIGHT * inverseDepth;
      const float dy = upY * C::DOT_HEIGHT * inverseDepth;
      if (base.x + halfWidth < left || base.x - halfWidth > right ||
          base.y + dy > bottom || base.y < top)
        continue;
      const bool celebrating = cheering && (dot.flags & cheeringMask) != 0U;
      const float lift =
          crowdSway[dot.phase] + (celebrating ? crowdHop[dot.phase] : 0.0f);
      base.x += dx * lift;
      base.y += dy * lift;
      const ImU32 body =
          detail < 256U ? mixColor256(clump.color, dot.body, detail) : dot.body;
      const ImU32 head =
          detail < 256U ? mixColor256(clump.color, dot.head, detail) : dot.head;
      if (halfWidth * 2.0f < C::HEAD_DETAIL_PIXELS)
      {
        // Head-and-shoulders outline: reads as a person, not a block.
        const float headHalf = halfWidth * C::HEAD_WIDTH_SHARE;
        const std::array<ImVec2, 6> outline{
            point(base.x, base.y, -halfWidth, 0.0f, dx, dy),
            point(base.x, base.y, halfWidth, 0.0f, dx, dy),
            point(base.x, base.y, halfWidth, C::BODY_SHARE, dx, dy),
            point(base.x, base.y, headHalf, 1.0f, dx, dy),
            point(base.x, base.y, -headHalf, 1.0f, dx, dy),
            point(base.x, base.y, -halfWidth, C::BODY_SHARE, dx, dy)};
        const std::array<ImU32, 6> colors{shadeColor256(body, 184U),
                                          shadeColor256(body, 184U),
                                          body,
                                          head,
                                          head,
                                          body};
        batchFan(outline, colors);
        continue;
      }
      // A seated silhouette: torso with sloping shoulders and a round head.
      const float shoulder = C::BODY_SHARE;
      const float shoulderLow = shoulder - C::SHOULDER_DROP;
      const float narrow = halfWidth * C::SHOULDER_SHARE;
      const ImU32 lower = shadeColor256(body, 184U);
      const std::array<ImVec2, 6> torso{
          point(base.x, base.y, -halfWidth, 0.0f, dx, dy),
          point(base.x, base.y, halfWidth, 0.0f, dx, dy),
          point(base.x, base.y, halfWidth, shoulderLow, dx, dy),
          point(base.x, base.y, narrow, shoulder, dx, dy),
          point(base.x, base.y, -narrow, shoulder, dx, dy),
          point(base.x, base.y, -halfWidth, shoulderLow, dx, dy)};
      const std::array<ImU32, 6> torsoColors{lower, lower, body,
                                             body,  body,  body};
      batchFan(torso, torsoColors);
      if (celebrating)
      {
        // Arms thrown up over the head, hands in skin colour.
        const float armHalf = halfWidth * C::ARM_WIDTH_SHARE;
        const float reach = 1.0f + C::ARM_REACH;
        for (const float sideSign : {-1.0f, 1.0f})
        {
          const float from = sideSign * narrow;
          const float to = sideSign * halfWidth * 1.15f;
          batchQuad(point(base.x, base.y, from - armHalf, shoulder, dx, dy),
                    point(base.x, base.y, from + armHalf, shoulder, dx, dy),
                    point(base.x, base.y, to + armHalf, reach, dx, dy),
                    point(base.x, base.y, to - armHalf, reach, dx, dy), body,
                    head);
        }
        if ((dot.flags & Flags::SCARF) != 0U)
        {
          const KitColors& kit =
              (dot.flags & Flags::AWAY_FAN) != 0U ? kits.away : kits.home;
          const float scarfHalf = halfWidth * C::SCARF_WIDTH_SHARE * 0.5f;
          const float scarfBottom = reach - C::SCARF_HEIGHT_SHARE;
          for (const float sideSign : {-1.0f, 1.0f})
          {
            const ImU32 color = sideSign < 0.0f ? kit.shirt : kit.trim;
            batchQuad(point(base.x, base.y, 0.0f, scarfBottom, dx, dy),
                      point(base.x, base.y, sideSign * scarfHalf, scarfBottom,
                            dx, dy),
                      point(base.x, base.y, sideSign * scarfHalf, reach, dx,
                            dy),
                      point(base.x, base.y, 0.0f, reach, dx, dy),
                      shadeColor(color, 0.85f), color);
          }
        }
      }
      const float headRadius = (1.0f - shoulder - C::NECK_GAP) * 0.5f;
      const float headMiddle = 1.0f - headRadius;
      const float headHalf = halfWidth * C::HEAD_WIDTH_SHARE;
      std::array<ImVec2, HEAD_SIDES> outline{};
      std::array<ImU32, HEAD_SIDES> headColors{};
      for (std::size_t corner = 0; corner < HEAD_SIDES; ++corner)
      {
        outline[corner] = point(base.x, base.y,
                                headHalf * HEAD_SHAPE[corner][0],
                                headMiddle + headRadius * HEAD_SHAPE[corner][1],
                                dx, dy);
        headColors[corner] =
            HEAD_SHAPE[corner][1] < 0.0f ? shadeColor256(head, 210U) : head;
      }
      batchFan(outline, headColors);
    }
  }
  drawCrowdFlags(face, upX, upY);
  releaseBatch();
}

void MatchRenderer3D::State::drawCrowdFlags(const Stadium3D::Face& face,
                                            float upX, float upY)
{
  using C = Tuning::Crowd;
  const bool cheering = cheeringMask != 0U;
  for (std::uint32_t index = face.flagBegin; index < face.flagEnd; ++index)
  {
    const Stadium3D::CrowdFlag& flag = geometry.flags[index];
    const Vec4 clip = projection.toClip(flag.base);
    if (clip.w < projection.nearPlane) continue;
    const float inverseDepth = 1.0f / clip.w;
    if (C::FLAG_HEIGHT * projection.focalPixels * inverseDepth <
        C::FLAG_MIN_PIXELS)
      continue;
    const bool celebrating = cheering && (flag.flags & cheeringMask) != 0U;
    const float energy = celebrating ? 1.0f + crowdCelebration : 1.0f;
    const float lift = celebrating ? crowdHop[flag.phase] * C::DOT_HEIGHT : 0.0f;
    const float wavePhase =
        reducedMotion ? static_cast<float>(flag.phase)
                      : elapsedSeconds * C::FLAG_WAVE_SPEED * energy +
                            static_cast<float>(flag.phase);
    const Vec3 poleBottom = flag.base + UP * lift;
    const Vec3 poleTop = poleBottom + UP * C::POLE_HEIGHT;
    // Two cloth panels (one per colour) rippling away from the pole.
    std::array<Vec3, 6> cloth{};
    for (std::size_t column = 0; column < 3; ++column)
    {
      const float share = static_cast<float>(column) * 0.5f;
      const float wave = C::FLAG_WAVE * energy * share *
                         std::sin(wavePhase + share * 2.8f);
      const Vec3 edge = poleTop + flag.along * (C::FLAG_LENGTH * share) +
                        UP * wave;
      cloth[column] = edge;
      cloth[column + 3] = edge - UP * C::FLAG_HEIGHT;
    }
    std::array<ImVec2, 6> screen{};
    bool visible = true;
    for (std::size_t corner = 0; corner < cloth.size() && visible; ++corner)
    {
      ScreenPoint projected;
      visible = projection.project(cloth[corner], projected);
      screen[corner] = toImVec(projected);
    }
    ScreenPoint bottomScreen;
    if (!visible || !projection.project(poleBottom, bottomScreen)) continue;
    const float poleHalf =
        std::max(0.5f, C::DOT_WIDTH * 0.08f * projection.focalPixels *
                           inverseDepth);
    const float normal = std::max(std::hypot(upX, upY), 1e-3f);
    const float offsetX = -upY / normal * poleHalf;
    const float offsetY = upX / normal * poleHalf;
    batchQuad({bottomScreen.x - offsetX, bottomScreen.y - offsetY},
              {bottomScreen.x + offsetX, bottomScreen.y + offsetY},
              {screen[0].x + offsetX, screen[0].y + offsetY},
              {screen[0].x - offsetX, screen[0].y - offsetY},
              C::POLE_COLOR, C::POLE_COLOR);
    for (std::size_t panel = 0; panel < 2; ++panel)
    {
      const ImU32 color = panel == 0 ? flag.primary : flag.secondary;
      batchQuad(screen[panel + 3], screen[panel + 4], screen[panel + 1],
                screen[panel], shadeColor(color, 0.82f), color);
    }
  }
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
      // Floodlights are off by day.
      if (face.glow != 0 && !day)
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
    addPolygon(quad,
               shadeColor(color, lighting(RenderMath::normalize(normal), light)),
               objectDepth, localDepth);
  }
}

void MatchRenderer3D::State::addPrism(std::span<const Vec3> bottom,
                                      std::span<const Vec3> top, ImU32 color,
                                      float objectDepth, bool capBottom,
                                      bool capTop)
{
  constexpr std::size_t MAX_SIDES = 8;
  const std::size_t sides = std::min({bottom.size(), top.size(), MAX_SIDES});
  if (sides < 3) return;
  std::array<ScreenPoint, MAX_SIDES> low{};
  std::array<ScreenPoint, MAX_SIDES> high{};
  Vec3 lowCentre;
  Vec3 highCentre;
  for (std::size_t index = 0; index < sides; ++index)
  {
    if (!projection.project(bottom[index], low[index]) ||
        !projection.project(top[index], high[index]))
      return;
    lowCentre = lowCentre + bottom[index];
    highCentre = highCentre + top[index];
  }
  const float inverse = 1.0f / static_cast<float>(sides);
  lowCentre = lowCentre * inverse;
  highCentre = highCentre * inverse;
  const Vec3 axisCentre = (lowCentre + highCentre) * 0.5f;
  for (std::size_t index = 0; index < sides; ++index)
  {
    const std::size_t next = (index + 1) % sides;
    const Vec3 faceCentre =
        (bottom[index] + bottom[next] + top[index] + top[next]) * 0.25f;
    Vec3 normal = RenderMath::cross(bottom[next] - bottom[index],
                                    top[index] - bottom[index]);
    if (RenderMath::dot(normal, faceCentre - axisCentre) < 0.0f)
      normal = normal * -1.0f;
    if (RenderMath::dot(normal, projection.eye - faceCentre) <= 0.0f) continue;
    const std::array<ScreenPoint, 4> quad{low[index], low[next], high[next],
                                          high[index]};
    addPolygon(quad,
               shadeColor(color, lighting(RenderMath::normalize(normal), light)),
               objectDepth,
               (quad[0].depth + quad[1].depth + quad[2].depth + quad[3].depth) *
                   0.25f);
  }
  const auto cap = [&](const std::array<ScreenPoint, MAX_SIDES>& ring,
                       Vec3 centre, Vec3 outward)
  {
    if (RenderMath::dot(outward, projection.eye - centre) <= 0.0f) return;
    float depth = 0.0f;
    for (std::size_t index = 0; index < sides; ++index)
      depth += ring[index].depth;
    addPolygon(std::span<const ScreenPoint>(ring.data(), sides),
               shadeColor(color,
                          lighting(RenderMath::normalize(outward), light)),
               objectDepth, depth * inverse);
  };
  if (capTop) cap(high, highCentre, highCentre - lowCentre);
  if (capBottom) cap(low, lowCentre, lowCentre - highCentre);
}

void MatchRenderer3D::State::updateMotion(const MatchRenderPlayer& player,
                                         AnimationSlot& slot, float alpha,
                                         float deltaSeconds)
{
  using P = Tuning::Player;
  using Q = Tuning::Pose;
  const Vec3 root = RenderMath::worldFromPitch(lerpRenderPosition(
      player.previousPosition, player.currentPosition, alpha));
  if (slot.player != player.player)
  {
    slot = AnimationSlot{};
    slot.player = player.player;
    slot.lastPosition = root;
    slot.lastSpeed = player.speedMetresPerSecond;
    if (player.player)
    {
      const std::uint32_t looks = cosmeticHash(player.player->getId());
      slot.phase = static_cast<float>(looks % 628U) / 100.0f;
      slot.leftFooted = (looks >> 12) % 5U == 0U;
      slot.number = shirtNumberFor(player);
      slot.backName = player.player->getLastName();
      for (char& letter : slot.backName)
      {
        if (letter >= 'a' && letter <= 'z')
          letter = static_cast<char>(letter - 'a' + 'A');
      }
    }
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
  slot.gaitSpeed +=
      ((groundSpeed < P::IDLE_SPEED ? 0.0f : groundSpeed) - slot.gaitSpeed) *
      RenderMath::dampingFactor(Tuning::Gait::SPEED_RATE, deltaSeconds);
  const float cycleMetres =
      P::STRIDE_BASE_METRES + P::STRIDE_PER_SPEED * groundSpeed;
  slot.phase = std::fmod(
      slot.phase + TWO_PI * std::min(distance / cycleMetres,
                                     P::MAX_CYCLES_PER_FRAME),
      TWO_PI);

  // Acceleration and turn rate per simulated second, so a paused match
  // freezes the pose and fast playback does not exaggerate it.
  const float yaw = RenderMath::worldYawFromFacing(RenderMath::lerpAngle(
      player.previousFacingAngle, player.currentFacingAngle, alpha));
  if (simSeconds > 0.0f)
  {
    const float rawAcceleration =
        std::clamp((groundSpeed - slot.lastSpeed) / simSeconds, -12.0f, 12.0f);
    slot.acceleration +=
        (rawAcceleration - slot.acceleration) *
        RenderMath::dampingFactor(Q::ACCEL_SMOOTHING, simSeconds);
    const float rawTurn =
        slot.hasYaw ? std::clamp(RenderMath::wrapAngle(yaw - slot.lastYaw) /
                                     simSeconds,
                                 -Q::MAX_TURN_RATE, Q::MAX_TURN_RATE)
                    : 0.0f;
    slot.turnRate += (rawTurn - slot.turnRate) *
                     RenderMath::dampingFactor(Q::TURN_SMOOTHING, simSeconds);
    slot.kickSeconds = std::max(0.0f, slot.kickSeconds - simSeconds);
  }
  slot.lastSpeed = groundSpeed;
  slot.lastYaw = yaw;
  slot.hasYaw = true;

  // Keeper stances ease in and out.
  using K = Tuning::Keeper;
  const float blend = RenderMath::dampingFactor(K::DIVE_RATE, deltaSeconds);
  float setTarget = 0.0f;
  float holdTarget = 0.0f;
  float diveTarget = 0.0f;
  if (player.isGoalkeeper && player.onPitch)
  {
    const GoalkeeperState keeperState = player.isHomeTeam
                                            ? homeKeeperState
                                            : awayKeeperState;
    const Vec3 toBall = ballWorld - root;
    const float ballDistance = std::hypot(toBall.x, toBall.y);
    if (player.isDiving || keeperState == GoalkeeperState::DIVE)
    {
      if (slot.diveBlend < 0.05f)
      {
        const Vec3 side{-std::sin(yaw), std::cos(yaw), 0.0f};
        slot.diveSide = RenderMath::dot(toBall, side) >= 0.0f ? 1.0f : -1.0f;
      }
      diveTarget = 1.0f;
    }
    else if (player.possessesBall || keeperState == GoalkeeperState::HOLD)
    {
      holdTarget = 1.0f;
    }
    else if (livePlay && keeperState == GoalkeeperState::SET_POSITION &&
             ballDistance < K::SET_DISTANCE)
    {
      setTarget = 1.0f;
    }
  }
  slot.setBlend += (setTarget - slot.setBlend) * blend;
  slot.holdBlend += (holdTarget - slot.holdBlend) * blend;
  slot.diveBlend += (diveTarget - slot.diveBlend) * blend;
}

int MatchRenderer3D::State::shirtNumberFor(const MatchRenderPlayer& player)
{
  // Shared with the 2D view, so both show the same numbers.
  return numbers.numberFor(player, playerStats);
}

GoalMood MatchRenderer3D::State::goalMoodFor(
    const MatchRenderPlayer& player) const
{
  if (!goal.active || !player.onPitch) return GoalMood::NONE;
  if (player.isHomeTeam != goal.byHome) return GoalMood::DEJECTED;
  if (!goal.ownGoal && player.player && player.player->getId() == goal.scorer)
    return GoalMood::SCORER;
  return GoalMood::CELEBRATING;
}

void MatchRenderer3D::State::addPlayer(const MatchRenderPlayer& player,
                                       AnimationSlot& slot, float alpha)
{
  using P = Tuning::Player;
  using Q = Tuning::Pose;
  const Vec3 root = RenderMath::worldFromPitch(lerpRenderPosition(
      player.previousPosition, player.currentPosition, alpha));
  ScreenPoint anchor;
  if (!projection.project(root + UP * P::HIP_HEIGHT, anchor)) return;
  const RenderMath::ScreenRect& rect = projection.rect;
  if (anchor.x < rect.x - P::CULL_MARGIN_PIXELS ||
      anchor.x > rect.x + rect.width + P::CULL_MARGIN_PIXELS ||
      anchor.y < rect.y - P::CULL_MARGIN_PIXELS ||
      anchor.y > rect.y + rect.height + P::CULL_MARGIN_PIXELS)
    return;

  const bool keeper = player.isGoalkeeper;
  const std::size_t kitIndex =
      (player.isHomeTeam ? 0U : 2U) + (keeper ? 1U : 0U);
  const KitColors& kit = kitList[kitIndex];
  const std::uint32_t looks =
      cosmeticHash(player.player ? player.player->getId() + 17U : 17U);
  const ImU32 skin = SKIN_TONES[looks % SKIN_TONES.size()];
  const ImU32 hair = HAIR_COLORS[(looks >> 8) % HAIR_COLORS.size()];
  const ImU32 hands = keeper ? gloveColors[kitIndex] : skin;
  const ImU32 bootColor = BOOT_COLORS[(looks >> 16) % BOOT_COLORS.size()];
  const HairStyle hairStyle = HAIR_STYLES[(looks >> 20) % HAIR_STYLES.size()];
  const float heightMetres = playerHeightMetres(player);
  const float scale = P::SCALE * heightMetres / P::REFERENCE_HEIGHT_METRES;
  const float pixelHeight =
      heightMetres * projection.focalPixels / anchor.depth;

  const float yaw = RenderMath::worldYawFromFacing(RenderMath::lerpAngle(
      player.previousFacingAngle, player.currentFacingAngle, alpha));
  const Vec3 forward{std::cos(yaw), std::sin(yaw), 0.0f};
  const Vec3 flatSide{-forward.y, forward.x, 0.0f};
  const float stride = slot.stride;

  // --- pose ---------------------------------------------------------------
  // Walk, jog and sprint blend with the smoothed speed; below a walk the
  // swing fades into the idle stance.
  const Tuning::Gait::Shape gait = gaitShape(slot.gaitSpeed);
  const float motion =
      std::clamp(slot.gaitSpeed / Tuning::Gait::WALK.speed, 0.0f, 1.0f);
  Pose pose;
  for (std::size_t index = 0; index < 2; ++index)
  {
    const float legPhase =
        slot.phase + (index == 0 ? 0.0f : std::numbers::pi_v<float>);
    const float outward = index == 0 ? 1.0f : -1.0f;
    pose.thigh[index] = motion * gait.thigh * std::sin(legPhase);
    pose.knee[index] =
        -motion * gait.knee * (0.5f - 0.5f * std::cos(legPhase));
    pose.armSwing[index] = -motion * gait.arm * std::sin(legPhase);
    pose.armSpread[index] = outward * 0.08f;
    pose.elbow[index] = P::ELBOW_BEND + (gait.elbow - P::ELBOW_BEND) * motion;
  }
  // Lean into acceleration, sit back when braking, bank into turns.
  const float accelerationLean = std::clamp(
      slot.acceleration * Q::ACCEL_LEAN, -Q::MAX_BRAKE_LEAN, Q::MAX_ACCEL_LEAN);
  pose.lean = motion * gait.lean + accelerationLean;
  pose.turnRoll =
      std::clamp(slot.turnRate * player.speedMetresPerSecond * Q::TURN_LEAN,
                 -Q::MAX_TURN_LEAN, Q::MAX_TURN_LEAN);
  const float stop = std::clamp(
      (-slot.acceleration - Q::STOP_DECELERATION) / Q::STOP_DECELERATION, 0.0f,
      1.0f);
  if (stop > 0.0f)
  {
    // The leading leg plants in front, both knees give.
    const std::size_t lead = std::sin(slot.phase) >= 0.0f ? 0U : 1U;
    pose.thigh[lead] += stop * 0.35f;
    pose.thigh[1 - lead] -= stop * 0.15f;
    pose.knee[0] -= stop * Q::STOP_KNEE_BEND;
    pose.knee[1] -= stop * Q::STOP_KNEE_BEND;
    pose.hipDrop += stop * Q::STOP_HIP_DROP;
  }

  if (slot.kickSeconds > 0.0f)
  {
    using Kk = Tuning::Kick;
    Pose kick = pose;
    const float duration = slot.header ? Kk::HEADER_SECONDS : Kk::SECONDS;
    const float progress = std::clamp(1.0f - slot.kickSeconds / duration,
                                      0.0f, 1.0f);
    const float envelope =
        std::clamp(std::min(progress, 1.0f - progress) * 6.0f, 0.0f, 1.0f);
    if (slot.header)
    {
      kick.lift = Kk::HEADER_JUMP * std::sin(std::numbers::pi_v<float> *
                                             progress);
      kick.lean = -0.25f + (Kk::HEADER_NOD + 0.25f) * progress;
      for (std::size_t index = 0; index < 2; ++index)
      {
        const float outward = index == 0 ? 1.0f : -1.0f;
        kick.thigh[index] = 0.25f;
        kick.knee[index] = -0.7f;
        kick.armSwing[index] = 0.9f;
        kick.armSpread[index] = outward * 0.55f;
        kick.elbow[index] = 0.9f;
      }
    }
    else
    {
      const std::size_t leg = slot.leftFooted ? 0U : 1U;
      const auto segment = [](float from, float to, float t)
      { return from + (to - from) * std::clamp(t, 0.0f, 1.0f); };
      float thigh = 0.0f;
      float knee = 0.0f;
      if (progress < 0.35f)
      {
        thigh = segment(0.0f, Kk::BACKSWING, progress / 0.35f);
        knee = segment(0.0f, Kk::BACKSWING_KNEE, progress / 0.35f);
      }
      else if (progress < 0.6f)
      {
        thigh = segment(Kk::BACKSWING, Kk::FOLLOW_THROUGH,
                        (progress - 0.35f) / 0.25f);
        knee = segment(Kk::BACKSWING_KNEE, -0.05f, (progress - 0.35f) / 0.25f);
      }
      else
      {
        thigh = segment(Kk::FOLLOW_THROUGH, 0.2f, (progress - 0.6f) / 0.4f);
        knee = segment(-0.05f, -0.3f, (progress - 0.6f) / 0.4f);
      }
      kick.thigh[leg] = thigh;
      kick.knee[leg] = knee;
      kick.thigh[1 - leg] = 0.08f;
      kick.knee[1 - leg] = -0.3f;
      kick.lean = Kk::TORSO_LEAN;
      for (std::size_t index = 0; index < 2; ++index)
      {
        const float outward = index == 0 ? 1.0f : -1.0f;
        // The arm opposite the kicking leg reaches forward for balance.
        kick.armSwing[index] = index == leg ? -0.2f : 0.45f;
        kick.armSpread[index] = outward * Kk::ARM_ABDUCTION;
        kick.elbow[index] = 0.4f;
      }
    }
    pose = mixPose(pose, kick, envelope);
  }

  if (keeper)
  {
    using K = Tuning::Keeper;
    if (slot.setBlend > 0.01f)
    {
      Pose set = pose;
      set.hipDrop = K::SET_HIP_DROP;
      set.lean = K::SET_LEAN;
      for (std::size_t index = 0; index < 2; ++index)
      {
        const float outward = index == 0 ? 1.0f : -1.0f;
        set.thigh[index] = K::SET_THIGH;
        set.knee[index] = K::SET_KNEE;
        set.legSpread[index] = outward * K::SET_LEG_SPREAD;
        set.armSwing[index] = K::SET_ARM_FORWARD;
        set.armSpread[index] = outward * K::SET_ARM_SPREAD;
        set.elbow[index] = 0.45f;
      }
      pose = mixPose(pose, set, slot.setBlend * (1.0f - stride));
    }
    if (slot.holdBlend > 0.01f)
    {
      Pose hold = pose;
      for (std::size_t index = 0; index < 2; ++index)
      {
        hold.armSwing[index] = K::HOLD_ARM;
        hold.armSpread[index] = (index == 0 ? 1.0f : -1.0f) * 0.12f;
        hold.elbow[index] = K::HOLD_ELBOW;
      }
      pose = mixPose(pose, hold, slot.holdBlend);
    }
    if (slot.diveBlend > 0.01f)
    {
      Pose dive = pose;
      dive.bodyRoll = slot.diveSide * K::DIVE_ROLL;
      dive.lift = K::DIVE_LIFT;
      dive.lean = 0.0f;
      dive.turnRoll = 0.0f;
      dive.hipDrop = 0.0f;
      for (std::size_t index = 0; index < 2; ++index)
      {
        const float outward = index == 0 ? 1.0f : -1.0f;
        dive.thigh[index] = index == 0 ? 0.15f : -0.1f;
        dive.knee[index] = -0.25f;
        dive.legSpread[index] = outward * 0.06f;
        dive.armSwing[index] = std::numbers::pi_v<float> - 0.1f;
        dive.armSpread[index] = outward * 0.12f;
        dive.elbow[index] = 0.05f;
      }
      pose = mixPose(pose, dive, slot.diveBlend);
    }
  }

  const GoalMood mood = goalMoodFor(player);
  if (mood != GoalMood::NONE && celebrationWeight > 0.0f)
  {
    using C = Tuning::Celebration;
    Pose react = pose;
    if (mood == GoalMood::DEJECTED)
    {
      react.lean = C::DEJECTED_LEAN;
      for (std::size_t index = 0; index < 2; ++index)
      {
        react.armSwing[index] = 0.05f;
        react.armSpread[index] = (index == 0 ? 1.0f : -1.0f) * 0.06f;
        react.elbow[index] = 0.15f;
      }
    }
    else
    {
      const float hop =
          reducedMotion
              ? 0.0f
              : C::HOP * (1.0f - stride) *
                    std::abs(std::sin(elapsedSeconds * C::HOP_SPEED +
                                      static_cast<float>(looks % 7U)));
      react.lift = std::max(react.lift, hop);
      react.lean = std::min(react.lean, 0.1f);
      for (std::size_t index = 0; index < 2; ++index)
      {
        const float outward = index == 0 ? 1.0f : -1.0f;
        if (mood == GoalMood::SCORER)
        {
          // Arms out like wings on the run to the corner flag.
          react.armSwing[index] = 0.15f;
          react.armSpread[index] = outward * C::WINGS_SPREAD;
          react.elbow[index] = 0.05f;
        }
        else
        {
          react.armSwing[index] = C::ARMS_UP;
          react.armSpread[index] = outward * C::ARMS_SPREAD;
          react.elbow[index] = 0.2f;
        }
      }
    }
    pose = mixPose(pose, react, celebrationWeight);
  }

  // --- skeleton -----------------------------------------------------------
  const float bob = motion * gait.bob * std::abs(std::sin(slot.phase));
  const float roll = pose.turnRoll + pose.bodyRoll;
  const Vec3 bodyUp = UP * std::cos(roll) + flatSide * std::sin(roll);
  const Vec3 bodySide = flatSide * std::cos(roll) - UP * std::sin(roll);
  // Turns bank from the boots; dives pivot around the hips.
  const Vec3 hipUp =
      UP * std::cos(pose.bodyRoll) + flatSide * std::sin(pose.bodyRoll);
  const Vec3 base = root + UP * ((bob + pose.lift) * scale) +
                    (UP - hipUp) * (P::HIP_HEIGHT * scale);
  const BodyFrame legFrame{bodyUp, forward, bodySide};
  const float objectDepth = anchor.depth;
  const bool detailed = pixelHeight >= P::HAND_MIN_PIXELS;
  // Up close the body is built from prisms instead of boxes.
  const bool rounded = pixelHeight >= P::ROUND_MIN_PIXELS;

  Vec3 lastDirection = UP;
  const auto limb = [&](Vec3 joint, const BodyFrame& frame, float swing,
                        float spread, float limbLength, float halfWidth,
                        ImU32 color)
  {
    const Vec3 direction =
        (frame.up * -std::cos(swing) + frame.forward * std::sin(swing)) *
            std::cos(spread) +
        frame.side * std::sin(spread);
    const Vec3 along = direction * -1.0f;
    Vec3 acrossA = RenderMath::cross(along, frame.forward);
    if (RenderMath::dot(acrossA, acrossA) < 0.09f)
      acrossA = RenderMath::cross(along, frame.side);
    acrossA = RenderMath::normalize(acrossA);
    const Vec3 acrossB = RenderMath::cross(along, acrossA);
    const Vec3 end = joint + direction * (limbLength * scale);
    if (rounded)
    {
      std::array<Vec3, P::LIMB_SIDES> top{};
      std::array<Vec3, P::LIMB_SIDES> bottom{};
      for (std::size_t side = 0; side < top.size(); ++side)
      {
        const float angle = TWO_PI * (static_cast<float>(side) + 0.5f) /
                            static_cast<float>(top.size());
        const Vec3 offset = (acrossA * std::cos(angle) +
                             acrossB * std::sin(angle)) *
                            (halfWidth * scale);
        top[side] = joint + offset;
        bottom[side] = end + offset;
      }
      addPrism(bottom, top, color, objectDepth, true, true);
      lastDirection = direction;
      return end;
    }
    const Vec3 centre = joint + direction * (limbLength * 0.5f * scale);
    std::array<Vec3, 8> corners{};
    for (std::size_t corner = 0; corner < 8; ++corner)
    {
      corners[corner] =
          centre +
          acrossA * (((corner & 1U) != 0U ? 1.0f : -1.0f) * halfWidth * scale) +
          acrossB * (((corner & 2U) != 0U ? 1.0f : -1.0f) * halfWidth * scale) +
          along * (((corner & 4U) != 0U ? 1.0f : -1.0f) * limbLength * 0.5f *
                   scale);
    }
    addBox(corners, color, objectDepth);
    lastDirection = direction;
    return joint + direction * (limbLength * scale);
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

  for (std::size_t index = 0; index < 2; ++index)
  {
    const float sideSign = index == 0 ? 1.0f : -1.0f;
    const Vec3 hip = base + bodyUp * ((P::HIP_HEIGHT - pose.hipDrop) * scale) +
                     bodySide * (sideSign * P::HIP_SPREAD * scale);
    const Vec3 kneeJoint =
        limb(hip, legFrame, pose.thigh[index], pose.legSpread[index],
             P::THIGH_LENGTH, P::THIGH_HALF_WIDTH, skin);
    const Vec3 ankle = limb(kneeJoint, legFrame,
                            pose.thigh[index] + pose.knee[index],
                            pose.legSpread[index], P::SHIN_LENGTH,
                            P::SHIN_HALF_WIDTH, kit.socks);
    const Vec3 boot = ankle + forward * (P::BOOT_FORWARD * scale);
    orientedBox(boot, forward * (P::BOOT_HALF_LENGTH * scale),
                bodySide * (P::BOOT_HALF_WIDTH * scale),
                bodyUp * (P::BOOT_HALF_HEIGHT * scale), bootColor);
    if (rounded)
    {
      orientedBox(boot - bodyUp * ((P::BOOT_HALF_HEIGHT + P::SOLE_HALF_HEIGHT) *
                                   scale),
                  forward * (P::BOOT_HALF_LENGTH * scale),
                  bodySide * (P::BOOT_HALF_WIDTH * scale),
                  bodyUp * (P::SOLE_HALF_HEIGHT * scale), P::SOLE_COLOR);
    }
  }
  orientedBox(base + bodyUp * ((P::SHORTS_HEIGHT - pose.hipDrop) * scale),
              forward * (P::SHORTS_HALF_DEPTH * scale),
              bodySide * (P::SHORTS_HALF_WIDTH * scale),
              bodyUp * (P::SHORTS_HALF_HEIGHT * scale), kit.shorts);

  // Tapered torso leaning with the pose.
  const Vec3 torsoUp =
      bodyUp * std::cos(pose.lean) + forward * std::sin(pose.lean);
  const Vec3 torsoForward =
      forward * std::cos(pose.lean) - bodyUp * std::sin(pose.lean);
  const Vec3 waist = base + bodyUp * ((P::TORSO_BASE - pose.hipDrop) * scale);
  const Vec3 chest = waist + torsoUp * (P::TORSO_LENGTH * scale);
  if (rounded)
  {
    // A chamfered section: rounder flanks, a broad flat back for the print.
    constexpr std::array<std::array<float, 2>, P::TORSO_SIDES> SECTION{{
        {0.72f, 1.0f},
        {1.0f, 0.55f},
        {1.0f, -0.55f},
        {0.72f, -1.0f},
        {-0.72f, -1.0f},
        {-1.0f, -0.55f},
        {-1.0f, 0.55f},
        {-0.72f, 1.0f},
    }};
    std::array<Vec3, P::TORSO_SIDES> lower{};
    std::array<Vec3, P::TORSO_SIDES> upper{};
    for (std::size_t corner = 0; corner < SECTION.size(); ++corner)
    {
      const auto ring = [&](Vec3 centre, float width, float depth)
      {
        return centre + bodySide * (SECTION[corner][0] * width * scale) +
               torsoForward * (SECTION[corner][1] * depth * scale);
      };
      lower[corner] = ring(waist, P::WAIST_HALF_WIDTH, P::WAIST_HALF_DEPTH);
      upper[corner] = ring(chest, P::CHEST_HALF_WIDTH, P::CHEST_HALF_DEPTH);
    }
    addPrism(lower, upper, kit.shirt, objectDepth, false, true);
  }
  else
  {
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
                      bodySide * ((corner & 2U) != 0U ? width : -width);
    }
    addBox(torso, kit.shirt, objectDepth);
  }
  if (detailed)
  {
    orientedBox(chest, torsoForward * (P::COLLAR_HALF_SIZE * scale),
                bodySide * (P::COLLAR_HALF_SIZE * scale),
                torsoUp * (P::COLLAR_HALF_HEIGHT * scale), kit.trim);
  }
  addBackPrint(slot, kitIndex, waist, torsoUp, torsoForward, bodySide, scale,
               objectDepth);

  const BodyFrame armFrame{torsoUp, torsoForward, bodySide};
  for (std::size_t index = 0; index < 2; ++index)
  {
    const float sideSign = index == 0 ? 1.0f : -1.0f;
    const Vec3 shoulder = chest - torsoUp * (P::SHOULDER_DROP * scale) +
                          bodySide * (sideSign * P::SHOULDER_SPREAD * scale);
    const Vec3 elbow =
        limb(shoulder, armFrame, pose.armSwing[index], pose.armSpread[index],
             P::UPPER_ARM_LENGTH, P::UPPER_ARM_HALF_WIDTH, kit.shirt);
    const Vec3 wrist =
        limb(elbow, armFrame, pose.armSwing[index] + pose.elbow[index],
             pose.armSpread[index], P::FOREARM_LENGTH, P::FOREARM_HALF_WIDTH,
             skin);
    if (detailed)
    {
      const float half =
          (keeper ? P::GLOVE_HALF_SIZE : P::HAND_HALF_SIZE) * scale;
      orientedBox(wrist + lastDirection * half, torsoForward * half,
                  bodySide * (half * 0.75f), torsoUp * half, hands);
    }
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
    primitive.reference = static_cast<std::uint32_t>(hairStyle);
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

void MatchRenderer3D::State::addBackPrint(const AnimationSlot& slot,
                                          std::size_t kitIndex, Vec3 waist,
                                          Vec3 torsoUp, Vec3 torsoForward,
                                          Vec3 bodySide, float scale,
                                          float objectDepth)
{
  using N = Tuning::Number;
  using P = Tuning::Player;
  if (slot.number <= 0) return;
  const float backDepth =
      (P::WAIST_HALF_DEPTH + P::CHEST_HALF_DEPTH) * 0.5f * scale + 0.004f;
  const Vec3 centre = waist + torsoUp * (N::CENTRE_UP * scale) -
                      torsoForward * backDepth;
  const Vec3 toEye = RenderMath::normalize(projection.eye - centre);
  if (RenderMath::dot(torsoForward * -1.0f, toEye) < N::MIN_FACING) return;
  // Seen from behind, the player's right is the viewer's right.
  const Vec3 across = bodySide * -1.0f;
  const Vec3 down = torsoUp * -1.0f;
  const auto print = [&](Vec3 middle, float glyphHeight, float minimumPixels,
                         const char* text)
  {
    // Digits and capitals fill ~72% of the font's line height.
    const float lineHeight = glyphHeight / 0.72f;
    ScreenPoint mid;
    ScreenPoint below;
    if (!projection.project(middle, mid) ||
        !projection.project(middle + down * (lineHeight * 0.5f), below))
      return;
    const float linePixels =
        2.0f * std::hypot(below.x - mid.x, below.y - mid.y);
    if (linePixels * 0.72f < minimumPixels) return;
    const float step = std::round(std::log(linePixels / N::MIN_FONT_SIZE) /
                                  std::log(N::FONT_STEP));
    const float fontSize = std::clamp(
        N::MIN_FONT_SIZE * std::pow(N::FONT_STEP, step), N::MIN_FONT_SIZE,
        N::MAX_FONT_SIZE);
    const ImVec2 size =
        ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text);
    if (size.x <= 0.0f || size.y <= 0.0f) return;
    const float halfWidth = lineHeight * size.x / size.y * 0.5f;
    ScreenPoint right;
    if (!projection.project(middle + across * halfWidth, right)) return;
    Primitive primitive;
    primitive.firstPoint = static_cast<std::uint32_t>(points.size());
    primitive.pointCount = 3;
    primitive.kind = PrimitiveKind::TEXT;
    primitive.color = numberColors[kitIndex];
    primitive.size = fontSize;
    primitive.reference = static_cast<std::uint32_t>(texts.size());
    texts.push_back(text);
    points.push_back(toImVec(mid));
    points.push_back(toImVec(right));
    points.push_back(toImVec(below));
    // Just in front of the shirt's back face.
    keys.push_back({objectDepth, mid.depth - 0.05f,
                    static_cast<std::uint32_t>(primitives.size())});
    primitives.push_back(primitive);
  };
  const auto number = static_cast<std::size_t>(std::clamp(slot.number, 0, 99));
  print(centre, N::HEIGHT * scale, N::MIN_PIXELS, NUMBER_TEXT[number].data());
  if (!slot.backName.empty())
  {
    print(centre + torsoUp * ((N::HEIGHT * 0.5f + N::NAME_GAP +
                               N::NAME_HEIGHT * 0.5f) *
                              scale),
          N::NAME_HEIGHT * scale, N::NAME_MIN_PIXELS, slot.backName.c_str());
  }
}

void MatchRenderer3D::State::drawBackText(const Primitive& primitive)
{
  // Glyphs laid out once, then mapped onto the shirt with the affine frame
  // (centre, half-width and half-height points) projected for this player.
  const ImVec2* frame = points.data() + primitive.firstPoint;
  const char* text = texts[primitive.reference];
  ImFont* font = ImGui::GetFont();
  const ImVec2 size =
      font->CalcTextSizeA(primitive.size, FLT_MAX, 0.0f, text);
  if (size.x <= 0.0f || size.y <= 0.0f) return;
  const ImVec2 canonical{projection.rect.x, projection.rect.y};
  const ImVec4 unclipped{-1e6f, -1e6f, 1e6f, 1e6f};
  const int firstVertex = drawList->VtxBuffer.Size;
  // Two passes a hair apart give the heavy block digits of a real kit.
  const float weight = primitive.size * Tuning::Number::WEIGHT;
  drawList->AddText(font, primitive.size, canonical, primitive.color, text,
                    nullptr, 0.0f, &unclipped);
  drawList->AddText(font, primitive.size, {canonical.x + weight, canonical.y},
                    primitive.color, text, nullptr, 0.0f, &unclipped);
  const float width = size.x + weight;
  const ImVec2 axisX{frame[1].x - frame[0].x, frame[1].y - frame[0].y};
  const ImVec2 axisY{frame[2].x - frame[0].x, frame[2].y - frame[0].y};
  for (int index = firstVertex; index < drawList->VtxBuffer.Size; ++index)
  {
    ImDrawVert& vertex = drawList->VtxBuffer[index];
    const float u = (vertex.pos.x - canonical.x) / width * 2.0f - 1.0f;
    const float v = (vertex.pos.y - canonical.y) / size.y * 2.0f - 1.0f;
    vertex.pos = {frame[0].x + axisX.x * u + axisY.x * v,
                  frame[0].y + axisX.y * u + axisY.y * v};
  }
}

void MatchRenderer3D::State::detectTouches(const MatchRenderSnapshot& snapshot)
{
  using Kk = Tuning::Kick;
  const MatchRenderBall& ball = snapshot.ball;
  const Player* possessor = ball.possessedBy;
  const bool newStep = ball.currentPosition.x != lastBallPosition.x ||
                       ball.currentPosition.y != lastBallPosition.y;
  if (!newStep)
  {
    if (possessor) lastPossessor = possessor;
    return;
  }
  lastBallPosition = ball.currentPosition;
  const Vec3 from = RenderMath::worldFromPitch(ball.previousPosition);
  const Vec3 to = RenderMath::worldFromPitch(ball.currentPosition);
  const float stepMetres = RenderMath::length(to - from);
  const float speed = stepMetres / MatchTuning::Timing::FIXED_STEP_SECONDS;
  const float previousSpeed = lastBallStepSpeed;
  lastBallStepSpeed = speed;
  const Player* kicker = nullptr;
  if (livePlay && stepMetres < Kk::TELEPORT_METRES &&
      speed >= Kk::MIN_BALL_SPEED)
  {
    if (lastPossessor && possessor != lastPossessor)
    {
      kicker = lastPossessor;
    }
    else if (!possessor && speed - previousSpeed >= Kk::SPEED_JUMP)
    {
      // First-time passes, shots and clearances never register possession:
      // the nearest player to where the ball was struck hit it.
      float best = Kk::REACH_METRES;
      for (const MatchRenderPlayer& player : snapshot.players)
      {
        if (!player.onPitch) continue;
        const Vec3 at = RenderMath::worldFromPitch(player.previousPosition);
        const float distance = std::hypot(at.x - from.x, at.y - from.y);
        if (distance < best)
        {
          best = distance;
          kicker = player.player;
        }
      }
    }
  }
  lastPossessor = possessor;
  if (!kicker) return;
  for (std::size_t index = 0; index < snapshot.players.size(); ++index)
  {
    if (snapshot.players[index].player != kicker) continue;
    AnimationSlot& slot = slots[index];
    if (slot.kickSeconds > 0.0f) return;
    slot.header = ball.previousHeightMetres >= Kk::HEADER_HEIGHT &&
                  !snapshot.players[index].isGoalkeeper;
    slot.kickSeconds = slot.header ? Kk::HEADER_SECONDS : Kk::SECONDS;
    return;
  }
}

void MatchRenderer3D::State::updateGoalMoment(
    const MatchRenderSnapshot& snapshot)
{
  using C = Tuning::Celebration;
  const bool scoring = snapshot.state == MatchState::GOAL;
  if (!scoring)
  {
    goal.active = false;
    celebrationWeight = 0.0f;
    crowdCelebration = 0.0f;
    return;
  }
  if (!goal.active)
  {
    goal = GoalMomentState{};
    goal.active = true;
    goal.byHome = snapshot.goalScoredByHome;
    // The goal the ball is nearest to took it, whichever way teams play.
    goal.goalIndex = ballWorld.x < LENGTH * 0.5f ? 0U : 1U;
    goal.impact = {goal.goalIndex == 0 ? -Tuning::Goal::BASE_DEPTH * 0.7f
                                       : LENGTH + Tuning::Goal::BASE_DEPTH * 0.7f,
                   std::clamp(ballWorld.y,
                              WIDTH * 0.5f - Tuning::Goal::WIDTH * 0.5f,
                              WIDTH * 0.5f + Tuning::Goal::WIDTH * 0.5f),
                   std::clamp(ballWorld.z, 0.3f, Tuning::Goal::HEIGHT - 0.3f)};
    if (snapshot.events)
    {
      const std::vector<MatchEvent>& events = *snapshot.events;
      const std::size_t oldest = events.size() > 64 ? events.size() - 64 : 0;
      for (std::size_t index = events.size(); index-- > oldest;)
      {
        const MatchEvent& event = events[index];
        if (event.type != MatchEventType::GOAL &&
            event.type != MatchEventType::OWN_GOAL)
          continue;
        goal.ownGoal = event.type == MatchEventType::OWN_GOAL;
        goal.scorer = event.primaryPlayerId;
        goal.minute = MatchClock::minuteLabel(event.timeMinute, event.period,
                                              event.addedMinute > 0.0f);
        break;
      }
    }
    for (const MatchRenderPlayer& player : snapshot.players)
    {
      if (!goal.ownGoal && player.player &&
          player.player->getId() == goal.scorer)
        goal.scorerName = player.player->getName();
    }
  }
  goal.shownSeconds += frameSeconds;
  goal.celebrationSeconds = std::max(
      0.0f, MatchTuning::Timing::GOAL_CELEBRATION_SECONDS -
                snapshot.goalCelebrationRemaining);
  const auto envelope = [](float seconds, float hold)
  {
    const float in = std::clamp(seconds / 0.6f, 0.0f, 1.0f);
    const float out = std::clamp(
        1.0f - (seconds - hold) / C::FADE_SECONDS, 0.0f, 1.0f);
    return std::min(in, out);
  };
  celebrationWeight = envelope(goal.celebrationSeconds, C::PLAYER_SECONDS);
  crowdCelebration = envelope(goal.celebrationSeconds, C::CROWD_SECONDS);
}

void MatchRenderer3D::State::drawGoalSting(const MatchRenderSnapshot& snapshot)
{
  // A broadcast lower third in the style of the focus HUD chips: a club
  // colour bar, the goal call and score, then the scorer and minute.
  using S = Tuning::Sting;
  if (!goal.active || goal.shownSeconds > S::SECONDS) return;
  const float t = goal.shownSeconds;
  const float fadeIn = std::clamp(t / S::FADE_IN_SECONDS, 0.0f, 1.0f);
  const float fadeOut =
      std::clamp((S::SECONDS - t) / S::FADE_OUT_SECONDS, 0.0f, 1.0f);
  const float opacity = std::min(fadeIn, fadeOut);
  if (opacity <= 0.0f) return;
  const float ease = 1.0f - (1.0f - fadeIn) * (1.0f - fadeIn);
  const float slide = reducedMotion ? 0.0f : (1.0f - ease) * S::SLIDE_EM;

  ImFont* font = ImGui::GetFont();
  const float em = ImGui::GetFontSize();
  const float titleSize = em * S::TITLE_SCALE;
  const float padding = em * S::PADDING_EM;
  const char* call = LOC("MATCH_GOAL_BANNER");
  char score[16];
  std::snprintf(score, sizeof(score), "%d - %d", snapshot.homeScore,
                snapshot.awayScore);
  std::string detail = goal.scorerName;
  if (!goal.minute.empty())
    detail += (detail.empty() ? "" : "  ") + goal.minute;
  const ImVec2 callSize =
      font->CalcTextSizeA(titleSize, FLT_MAX, 0.0f, call);
  const ImVec2 scoreSize = font->CalcTextSizeA(titleSize, FLT_MAX, 0.0f, score);
  const ImVec2 detailSize =
      font->CalcTextSizeA(em, FLT_MAX, 0.0f, detail.c_str());
  const float bar = em * S::BAR_EM;
  const float titleWidth = callSize.x + padding * 2.0f + scoreSize.x;
  const float width =
      bar + padding * 2.0f + std::max(titleWidth, detailSize.x);
  const float height = callSize.y + (detail.empty() ? 0.0f : detailSize.y) +
                       padding * 1.5f;
  const float x = projection.rect.x +
                  std::max(padding, (projection.rect.width - width) * 0.5f) -
                  slide * em;
  const float y = projection.rect.y + projection.rect.height -
                  height - projection.rect.height * S::BOTTOM_SHARE;
  const auto fade = [opacity](ImU32 color)
  {
    const auto alpha = static_cast<float>((color >> IM_COL32_A_SHIFT) & 0xFFU);
    return withAlpha(color, static_cast<std::uint8_t>(alpha * opacity));
  };
  const float rounding = em * S::ROUNDING_EM;
  drawList->AddRectFilled({x, y}, {x + width, y + height},
                          fade(S::PANEL_COLOR), rounding);
  const KitColors& kit = goal.byHome ? kits.home : kits.away;
  drawList->AddRectFilled({x, y}, {x + bar, y + height}, fade(kit.shirt),
                          rounding, ImDrawFlags_RoundCornersLeft);
  drawList->AddRectFilled({x + bar * 0.6f, y}, {x + bar, y + height},
                          fade(kit.trim));
  const float textX = x + bar + padding;
  const float textY = y + padding * 0.75f;
  drawList->AddText(font, titleSize, {textX, textY}, fade(S::CALL_COLOR),
                    call);
  drawList->AddText(font, titleSize,
                    {textX + callSize.x + padding * 2.0f, textY},
                    fade(S::TEXT_COLOR), score);
  if (!detail.empty())
  {
    drawList->AddText(font, em, {textX, textY + callSize.y},
                      fade(S::DETAIL_COLOR), detail.c_str());
  }
}

void MatchRenderer3D::State::addGoals()
{
  using R = Tuning::Ripple;
  for (std::size_t goalIndex = 0; goalIndex < geometry.goals.size();
       ++goalIndex)
  {
    const Stadium3D::Goal& frame = geometry.goals[goalIndex];
    // The net that took the goal billows out from the ball and settles.
    const bool rippling = goal.active && goal.goalIndex == goalIndex &&
                          goal.shownSeconds < R::SECONDS;
    const float age = goal.shownSeconds;
    const Vec3 mouth{goalIndex == 0 ? 0.0f : LENGTH, WIDTH * 0.5f,
                     Tuning::Goal::HEIGHT * 0.5f};
    for (const Stadium3D::NetPanel& net : frame.nets)
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
      Vec3 outward = RenderMath::normalize(
          RenderMath::cross(net.corners[1] - net.corners[0],
                            net.corners[3] - net.corners[0]));
      const Vec3 panelCentre = (net.corners[0] + net.corners[2]) * 0.5f;
      if (RenderMath::dot(outward, panelCentre - mouth) < 0.0f)
        outward = outward * -1.0f;
      const auto displaced = [&](Vec3 point)
      {
        const float distance = RenderMath::length(point - goal.impact);
        const float push = R::AMPLITUDE * std::exp(-R::DECAY * age) *
                           std::exp(-distance / R::REACH) *
                           std::cos(R::WAVE_NUMBER * distance -
                                    R::WAVE_SPEED * age);
        return point + outward * push;
      };
      const auto strand = [&](Vec3 from, Vec3 to)
      {
        ScreenPoint a;
        ScreenPoint b;
        if (!rippling)
        {
          if (!projection.project(from, a) || !projection.project(to, b))
            return;
          points.push_back(toImVec(a));
          points.push_back(toImVec(b));
          return;
        }
        if (!projection.project(displaced(from), a)) return;
        for (int segment = 1; segment <= R::SEGMENTS; ++segment)
        {
          const Vec3 point = displaced(RenderMath::lerp(
              from, to,
              static_cast<float>(segment) / static_cast<float>(R::SEGMENTS)));
          if (!projection.project(point, b)) return;
          points.push_back(toImVec(a));
          points.push_back(toImVec(b));
          a = b;
        }
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
    for (const auto& support : frame.supports)
    {
      const float depth = projection.depth((support[0] + support[1]) * 0.5f);
      addLine(support[0], support[1], Tuning::Goal::SUPPORT_COLOR,
              Tuning::Goal::POST_SIZE * 0.5f, depth);
    }
    for (const Stadium3D::Box& part : frame.frame)
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
  // The panels roll with the ground the ball covers, in its on-screen
  // direction of travel.
  const Vec3 world = RenderMath::worldFromPitch(ball, height + B::RADIUS);
  const Vec3 moved = world - lastBallWorld;
  const float distance = RenderMath::length(moved);
  lastBallWorld = world;
  ScreenPoint ahead;
  if (distance > 1e-4f && distance < Tuning::Player::TELEPORT_METRES &&
      projection.project(world + moved * (1.0f / distance), ahead))
  {
    const float dx = ahead.x - screen.x;
    const float dy = ahead.y - screen.y;
    const float length = std::hypot(dx, dy);
    if (length > 1e-3f) ballRoll = {dx / length, dy / length};
    ballSpin = std::fmod(
        ballSpin + std::min(distance / B::RADIUS, B::SPIN_MAX_PER_FRAME),
        TWO_PI);
  }
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
        const float pi = std::numbers::pi_v<float>;
        const ImU32 hair = primitive.secondaryColor;
        switch (static_cast<HairStyle>(primitive.reference))
        {
          case HairStyle::SHORT:
            drawList->PathArcTo(first[0], radius * 1.04f, pi + 0.2f,
                                TWO_PI - 0.2f);
            drawList->PathFillConvex(hair);
            break;
          case HairStyle::BUZZ:
            // Cropped close: the skin shows through.
            drawList->PathArcTo(first[0], radius * 1.01f, pi + 0.1f,
                                TWO_PI - 0.1f);
            drawList->PathFillConvex(mixColor(hair, primitive.color, 0.45f));
            break;
          case HairStyle::VOLUME:
            drawList->AddCircleFilled(
                {first[0].x, first[0].y - radius * 0.28f}, radius * 0.86f,
                hair);
            break;
          case HairStyle::LONG:
            // Down past the ears on both sides.
            drawList->PathArcTo(first[0], radius * 1.06f, pi - 0.2f,
                                TWO_PI + 0.2f);
            drawList->PathFillConvex(hair);
            break;
          case HairStyle::SHAVED:
            break;
        }
        break;
      }
      case PrimitiveKind::BALL:
      {
        const float radius = primitive.size;
        drawList->AddCircleFilled(first[0], radius, Tuning::Ball::SHADE_COLOR);
        drawList->AddCircleFilled(
            {first[0].x - radius * 0.14f, first[0].y - radius * 0.14f},
            radius * 0.82f, Tuning::Ball::COLOR);
        if (radius >= 3.0f)
        {
          // Three dark panels turning over the ball along its roll.
          const ImVec2 across{-ballRoll.y, ballRoll.x};
          for (int patch = 0; patch < 3; ++patch)
          {
            const float phase =
                ballSpin + TWO_PI * static_cast<float>(patch) / 3.0f;
            const float facing = std::cos(phase);
            if (facing < -0.1f) continue;
            const float shift = std::sin(phase) * radius * 0.52f;
            const float side = static_cast<float>(patch - 1) * radius * 0.28f;
            drawList->AddCircleFilled(
                {first[0].x + ballRoll.x * shift + across.x * side,
                 first[0].y + ballRoll.y * shift + across.y * side},
                radius * 0.26f * (0.5f + 0.5f * std::max(facing, 0.0f)),
                Tuning::Ball::PATCH_COLOR);
          }
        }
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
      case PrimitiveKind::TEXT:
        drawBackText(primitive);
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
  const ImU32 dark =
      day ? Tuning::Day::VIGNETTE_COLOR : Tuning::Sky::VIGNETTE_COLOR;
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
  s.frameSeconds = deltaSeconds;
  s.reducedMotion = Theme::reducedMotion();
  // Daylight from the kick-off time, or forced by the view option; night
  // otherwise (and when the time is unknown).
  s.day = options.dayLook ||
          (options.kickoffMinutes >= Tuning::Day::DAY_FROM_MINUTES &&
           options.kickoffMinutes < Tuning::Day::DAY_UNTIL_MINUTES);
  s.light = s.day ? dayLight() : nightLight();

  s.prepareMatch(snapshot);
  if (s.slots.size() != snapshot.players.size())
    s.slots.resize(snapshot.players.size());
  const float alpha = snapshot.interpolationAlpha;
  // Simulated time of this frame from the match clock (0 while paused).
  const float rawSimSeconds =
      s.lastMatchMinutes >= 0.0f
          ? (snapshot.matchTimeMinutes - s.lastMatchMinutes) * 60.0f
          : 0.0f;
  s.simSeconds =
      std::clamp(rawSimSeconds, 0.0f, Tuning::Pose::MAX_SIM_SECONDS);
  const bool firstFrame = s.lastMatchMinutes < 0.0f;
  s.lastMatchMinutes = snapshot.matchTimeMinutes;
  s.livePlay = snapshot.state == MatchState::PLAYING;
  const Vec3 previousBall = s.ballWorld;
  s.ballWorld = RenderMath::worldFromPitch(
      lerpRenderPosition(snapshot.ball.previousPosition,
                         snapshot.ball.currentPosition, alpha),
      ballHeightMetres(snapshot.ball, alpha));
  // A playback jump (highlights skipping ahead, a new match state) moves
  // the play far in one frame: the cameras cut to it instead of easing.
  s.jumped = !firstFrame &&
             (std::abs(rawSimSeconds) > Tuning::Camera::JUMP_SIM_SECONDS ||
              RenderMath::length(s.ballWorld - previousBall) >
                  Tuning::Camera::JUMP_METRES);
  s.homeKeeperState = snapshot.homeGoalkeeperState;
  s.awayKeeperState = snapshot.awayGoalkeeperState;
  s.playerStats = snapshot.playerStats;
  s.updateGoalMoment(snapshot);
  for (std::size_t index = 0; index < snapshot.players.size(); ++index)
    s.updateMotion(snapshot.players[index], s.slots[index], alpha,
                   deltaSeconds);
  s.detectTouches(snapshot);
  s.updateCrowdMotion();
  const MatchCameraFocus focus = s.computeFocus(snapshot, deltaSeconds);
  // Mouse input maps through the frame the user was looking at.
  if (s.jumped && options.cameraMode != MatchCameraMode::FREE)
    s.camera.snap(focus, options.cameraMode);
  else
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
  s.drawPitch();
  for (const Stadium3D::GroundPolygon& polygon : s.geometry.wear)
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
  s.texts.clear();
  s.playersOnScreen.clear();
  for (std::size_t index = 0; index < snapshot.players.size(); ++index)
    s.addPlayer(snapshot.players[index], s.slots[index], alpha);
  s.addGoals();
  s.addCornerFlags();
  s.addBoards();
  s.addBall(snapshot, alpha);
  s.flushSorted();

  s.drawVignette();
  s.drawLabelsAndHover(options);
  s.drawScoreBug(snapshot, options);
  s.drawGoalSting(snapshot);
  drawList->PopClipRect();
}
