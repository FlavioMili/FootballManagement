// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "gui/render/match_stadium_3d.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>

#include "gui/render/match_render_3d_tuning.h"

namespace Stadium3D
{
namespace
{
using Tuning = MatchRender3DTuning;

constexpr float LENGTH = MatchTuning::Pitch::LENGTH_METRES;
constexpr float WIDTH = MatchTuning::Pitch::WIDTH_METRES;
constexpr float HALF_LENGTH = LENGTH * 0.5f;
constexpr float HALF_WIDTH = WIDTH * 0.5f;
constexpr Vec3 UP{0.0f, 0.0f, 1.0f};
constexpr float TWO_PI = 2.0f * std::numbers::pi_v<float>;

/// Soft floodlit falloff: brightest at the centre spot, darker in corners.
float groundLight(float x, float y)
{
  const float dx = (x - HALF_LENGTH) / HALF_LENGTH;
  const float dy = (y - HALF_WIDTH) / HALF_WIDTH;
  return std::max(0.62f, 1.0f - Tuning::Grass::VIGNETTE_LENGTH * dx * dx -
                             Tuning::Grass::VIGNETTE_WIDTH * dy * dy);
}

float unitHash(std::uint32_t a, std::uint32_t b, std::uint32_t c)
{
  const std::uint32_t mixed =
      cosmeticHash(a * 0x9e3779b1U ^
                   cosmeticHash(b * 0x85ebca77U ^ (c + Tuning::Crowd::SEED)));
  return static_cast<float>(mixed & 0xFFFFU) / 65536.0f;
}

void addRectangle(std::vector<GroundPolygon>& target, float x0, float y0,
                  float x1, float y1, ImU32 color, bool lit)
{
  GroundPolygon polygon;
  polygon.count = 4;
  polygon.points[0] = {x0, y0, 0.0f};
  polygon.points[1] = {x1, y0, 0.0f};
  polygon.points[2] = {x1, y1, 0.0f};
  polygon.points[3] = {x0, y1, 0.0f};
  for (std::size_t index = 0; index < 4; ++index)
  {
    polygon.colors[index] =
        lit ? shadeColor(color, groundLight(polygon.points[index].x,
                                            polygon.points[index].y))
            : color;
  }
  target.push_back(polygon);
}

void addLine(std::vector<GroundPolygon>& target, float x0, float y0, float x1,
             float y1)
{
  const float halfWidth = Tuning::Markings::LINE_WIDTH * 0.5f;
  const float dx = x1 - x0;
  const float dy = y1 - y0;
  const float lineLength = std::sqrt(dx * dx + dy * dy);
  const float ux = dx / lineLength;
  const float uy = dy / lineLength;
  // Extend by half a width so perpendicular lines meet with square corners.
  x0 -= ux * halfWidth;
  y0 -= uy * halfWidth;
  x1 += ux * halfWidth;
  y1 += uy * halfWidth;
  GroundPolygon polygon;
  polygon.count = 4;
  polygon.points[0] = {x0 - uy * halfWidth, y0 + ux * halfWidth, 0.0f};
  polygon.points[1] = {x1 - uy * halfWidth, y1 + ux * halfWidth, 0.0f};
  polygon.points[2] = {x1 + uy * halfWidth, y1 - ux * halfWidth, 0.0f};
  polygon.points[3] = {x0 + uy * halfWidth, y0 - ux * halfWidth, 0.0f};
  polygon.colors.fill(Tuning::Markings::COLOR);
  target.push_back(polygon);
}

void addArc(std::vector<GroundPolygon>& target, float cx, float cy,
            float radius, float startAngle, float endAngle, int segments)
{
  const float inner = radius - Tuning::Markings::LINE_WIDTH * 0.5f;
  const float outer = radius + Tuning::Markings::LINE_WIDTH * 0.5f;
  for (int segment = 0; segment < segments; ++segment)
  {
    const float a0 = startAngle + (endAngle - startAngle) *
                                      static_cast<float>(segment) /
                                      static_cast<float>(segments);
    const float a1 = startAngle + (endAngle - startAngle) *
                                      static_cast<float>(segment + 1) /
                                      static_cast<float>(segments);
    GroundPolygon polygon;
    polygon.count = 4;
    polygon.points[0] = {cx + std::cos(a0) * inner, cy + std::sin(a0) * inner,
                         0.0f};
    polygon.points[1] = {cx + std::cos(a0) * outer, cy + std::sin(a0) * outer,
                         0.0f};
    polygon.points[2] = {cx + std::cos(a1) * outer, cy + std::sin(a1) * outer,
                         0.0f};
    polygon.points[3] = {cx + std::cos(a1) * inner, cy + std::sin(a1) * inner,
                         0.0f};
    polygon.colors.fill(Tuning::Markings::COLOR);
    target.push_back(polygon);
  }
}

void addSpot(std::vector<GroundPolygon>& target, float cx, float cy)
{
  GroundPolygon polygon;
  polygon.count = static_cast<std::uint8_t>(Tuning::Markings::SPOT_SEGMENTS);
  for (std::size_t index = 0; index < polygon.count; ++index)
  {
    const float angle =
        TWO_PI * static_cast<float>(index) / static_cast<float>(polygon.count);
    polygon.points[index] = {
        cx + std::cos(angle) * Tuning::Markings::SPOT_RADIUS,
        cy + std::sin(angle) * Tuning::Markings::SPOT_RADIUS, 0.0f};
  }
  polygon.colors.fill(Tuning::Markings::COLOR);
  target.push_back(polygon);
}

void buildGround(Geometry& geometry)
{
  const float outside = Tuning::Grass::OUTSIDE_EXTENT;
  addRectangle(geometry.ground, -outside, -outside, LENGTH + outside,
               WIDTH + outside, Tuning::Grass::OUTSIDE_COLOR, false);

  // Surround split in a coarse grid so the lighting gradient carries on.
  constexpr int SURROUND_CELLS = 4;
  const float sx0 = -Tuning::Grass::SURROUND_X;
  const float sy0 = -Tuning::Grass::SURROUND_Y;
  const float sx1 = LENGTH + Tuning::Grass::SURROUND_X;
  const float sy1 = WIDTH + Tuning::Grass::SURROUND_Y;
  for (int column = 0; column < SURROUND_CELLS; ++column)
  {
    for (int row = 0; row < SURROUND_CELLS; ++row)
    {
      const float x0 = sx0 + (sx1 - sx0) * static_cast<float>(column) /
                                 static_cast<float>(SURROUND_CELLS);
      const float x1 = sx0 + (sx1 - sx0) * static_cast<float>(column + 1) /
                                 static_cast<float>(SURROUND_CELLS);
      const float y0 = sy0 + (sy1 - sy0) * static_cast<float>(row) /
                                 static_cast<float>(SURROUND_CELLS);
      const float y1 = sy0 + (sy1 - sy0) * static_cast<float>(row + 1) /
                                 static_cast<float>(SURROUND_CELLS);
      addRectangle(geometry.ground, x0, y0, x1, y1,
                   Tuning::Grass::SURROUND_COLOR, true);
    }
  }

  // Mowing stripes along the length with a faint cross-cut checker.
  const float stripeLength =
      LENGTH / static_cast<float>(Tuning::Grass::STRIPES);
  const float bandWidth =
      WIDTH / static_cast<float>(Tuning::Grass::WIDTH_BANDS);
  for (int stripe = 0; stripe < Tuning::Grass::STRIPES; ++stripe)
  {
    for (int band = 0; band < Tuning::Grass::WIDTH_BANDS; ++band)
    {
      float boost = stripe % 2 == 0 ? Tuning::Grass::LIGHT_STRIPE_BOOST : 1.0f;
      if (band % 2 == 0) boost *= Tuning::Grass::CROSS_BAND_BOOST;
      addRectangle(geometry.ground, static_cast<float>(stripe) * stripeLength,
                   static_cast<float>(band) * bandWidth,
                   static_cast<float>(stripe + 1) * stripeLength,
                   static_cast<float>(band + 1) * bandWidth,
                   shadeColor(Tuning::Grass::PITCH_COLOR, boost), true);
    }
  }
}

void buildMarkings(Geometry& geometry)
{
  auto& lines = geometry.markings;
  addLine(lines, 0.0f, 0.0f, LENGTH, 0.0f);
  addLine(lines, 0.0f, WIDTH, LENGTH, WIDTH);
  addLine(lines, 0.0f, 0.0f, 0.0f, WIDTH);
  addLine(lines, LENGTH, 0.0f, LENGTH, WIDTH);
  addLine(lines, HALF_LENGTH, 0.0f, HALF_LENGTH, WIDTH);
  addArc(lines, HALF_LENGTH, HALF_WIDTH, Tuning::Markings::CIRCLE_RADIUS, 0.0f,
         TWO_PI, Tuning::Markings::CIRCLE_SEGMENTS);
  addSpot(lines, HALF_LENGTH, HALF_WIDTH);

  const float arcHalfAngle =
      std::acos((Tuning::Markings::PENALTY_AREA_DEPTH -
                 Tuning::Markings::PENALTY_SPOT_DISTANCE) /
                Tuning::Markings::CIRCLE_RADIUS);
  for (const float goalLine : {0.0f, LENGTH})
  {
    const float inward = goalLine == 0.0f ? 1.0f : -1.0f;
    const auto addBox = [&](float depth, float width)
    {
      const float x = goalLine + inward * depth;
      const float y0 = HALF_WIDTH - width * 0.5f;
      const float y1 = HALF_WIDTH + width * 0.5f;
      addLine(lines, goalLine, y0, x, y0);
      addLine(lines, goalLine, y1, x, y1);
      addLine(lines, x, y0, x, y1);
    };
    addBox(Tuning::Markings::PENALTY_AREA_DEPTH,
           Tuning::Markings::PENALTY_AREA_WIDTH);
    addBox(Tuning::Markings::GOAL_AREA_DEPTH,
           Tuning::Markings::GOAL_AREA_WIDTH);
    const float spotX =
        goalLine + inward * Tuning::Markings::PENALTY_SPOT_DISTANCE;
    addSpot(lines, spotX, HALF_WIDTH);
    const float facing = inward > 0.0f ? 0.0f : std::numbers::pi_v<float>;
    addArc(lines, spotX, HALF_WIDTH, Tuning::Markings::CIRCLE_RADIUS,
           facing - arcHalfAngle, facing + arcHalfAngle,
           Tuning::Markings::PENALTY_ARC_SEGMENTS);
  }

  const float quarter = std::numbers::pi_v<float> * 0.5f;
  const std::array<std::array<float, 3>, 4> corners{{
      {0.0f, 0.0f, 0.0f},
      {LENGTH, 0.0f, quarter},
      {LENGTH, WIDTH, 2.0f * quarter},
      {0.0f, WIDTH, 3.0f * quarter},
  }};
  for (const auto& corner : corners)
  {
    addArc(lines, corner[0], corner[1], Tuning::Markings::CORNER_ARC_RADIUS,
           corner[2], corner[2] + quarter,
           Tuning::Markings::CORNER_ARC_SEGMENTS);
  }
}

struct ProfilePoint
{
  float d = 0.0f;
  float z = 0.0f;
};

/**
 * Ground footprint of one stand section. Every seating row at depth `d`
 * (metres away from the pitch) runs from `left(d)` to `right(d)`. Straight
 * sections move both ends along the stand's outward axis; corner sections
 * fan out from the shared front corner along both neighbouring stands.
 */
struct SectionShape
{
  Vec3 leftOrigin;
  Vec3 leftStep;
  Vec3 rightOrigin;
  Vec3 rightStep;
  /** Horizontal unit vector pointing away from the pitch. */
  Vec3 outward;
  /** Horizontal distance along `outward` per metre of depth. */
  float run = 1.0f;
  Side side = Side::NONE;
  bool awayEnd = false;

  Vec3 left(ProfilePoint point) const
  {
    return leftOrigin + leftStep * point.d + UP * point.z;
  }
  Vec3 right(ProfilePoint point) const
  {
    return rightOrigin + rightStep * point.d + UP * point.z;
  }
};

class StandBuilder
{
 public:
  StandBuilder(Geometry& target, const MatchKits& matchKits)
      : geometry(target), kits(matchKits)
  {
  }

  /// Straight stand split into equal sections along `along`.
  void buildStand(Vec3 origin, Vec3 along, Vec3 outward, float length,
                  int sections, Side side)
  {
    const float sectionLength = length / static_cast<float>(sections);
    for (int index = 0; index < sections; ++index)
    {
      SectionShape shape;
      shape.leftOrigin =
          origin + along * (sectionLength * static_cast<float>(index));
      shape.rightOrigin = shape.leftOrigin + along * sectionLength;
      shape.leftStep = outward;
      shape.rightStep = outward;
      shape.outward = outward;
      shape.side = side;
      shape.awayEnd = side == Side::EAST;
      buildSection(shape);
    }
  }

  /// Corner infill joining two perpendicular stands at their front corner.
  void buildCorner(Vec3 corner, Vec3 outwardA, Vec3 outwardB, Side side)
  {
    SectionShape shape;
    shape.leftOrigin = corner;
    shape.rightOrigin = corner;
    shape.leftStep = outwardA;
    shape.rightStep = outwardB;
    shape.outward = RenderMath::normalize(outwardA + outwardB);
    shape.run = RenderMath::dot(outwardA, shape.outward);
    shape.side = side;
    buildSection(shape);
  }

 private:
  Geometry& geometry;
  const MatchKits& kits;
  std::uint32_t sectionCounter = 0;

  Face& addProfileFace(const SectionShape& shape, ProfilePoint from,
                       ProfilePoint to, ImU32 nearColor, ImU32 farColor,
                       float inset = 0.0f)
  {
    Face& face = geometry.faces.emplace_back();
    const auto trimmed = [&](ProfilePoint point, bool rightEnd)
    {
      const Vec3 left = shape.left(point);
      const Vec3 right = shape.right(point);
      const float rowLength = RenderMath::length(right - left);
      const float t = rowLength > 2.0f * inset ? inset / rowLength : 0.0f;
      return RenderMath::lerp(left, right, rightEnd ? 1.0f - t : t);
    };
    face.corners = {trimmed(from, false), trimmed(from, true),
                    trimmed(to, true), trimmed(to, false)};
    face.colors = {nearColor, nearColor, farColor, farColor};
    face.normal = RenderMath::normalize(shape.outward * (-(to.z - from.z)) +
                                        UP * ((to.d - from.d) * shape.run));
    return face;
  }

  ImU32 crowdClothes(const SectionShape& shape, std::uint32_t key) const
  {
    const float teamShare = shape.awayEnd ? Tuning::Crowd::AWAY_END_SHARE
                                          : Tuning::Crowd::HOME_SHARE;
    if (unitHash(key, 3U, 0U) < teamShare)
    {
      const KitColors& kit = shape.awayEnd ? kits.away : kits.home;
      return unitHash(key, 4U, 0U) < 0.75f ? kit.shirt : kit.trim;
    }
    constexpr std::array<ImU32, 8> NEUTRAL{
        IM_COL32(30, 36, 60, 255),    IM_COL32(26, 26, 30, 255),
        IM_COL32(118, 120, 128, 255), IM_COL32(222, 222, 228, 255),
        IM_COL32(60, 82, 122, 255),   IM_COL32(112, 52, 42, 255),
        IM_COL32(72, 82, 52, 255),    IM_COL32(170, 150, 120, 255)};
    return NEUTRAL[static_cast<std::size_t>(unitHash(key, 5U, 0U) * 8.0f) %
                   NEUTRAL.size()];
  }

  void addCrowd(Face& face, const SectionShape& shape, std::uint32_t tier,
                ProfilePoint from, ProfilePoint to, float frontLight,
                float backLight)
  {
    face.crowdBegin = static_cast<std::uint32_t>(geometry.crowd.size());
    constexpr std::array<ImU32, 4> SKIN{
        IM_COL32(236, 196, 164, 255), IM_COL32(204, 150, 112, 255),
        IM_COL32(150, 100, 70, 255), IM_COL32(96, 64, 44, 255)};
    std::uint32_t row = 0;
    for (float d = from.d + Tuning::Crowd::ROW_DEPTH * 0.5f; d < to.d;
         d += Tuning::Crowd::ROW_DEPTH, ++row)
    {
      const float t = (d - from.d) / (to.d - from.d);
      const ProfilePoint point{d, from.z + (to.z - from.z) * t};
      const Vec3 left = shape.left(point);
      const Vec3 right = shape.right(point);
      const float rowLength = RenderMath::length(right - left);
      const auto seats =
          static_cast<std::uint32_t>(rowLength / Tuning::Crowd::SEAT_SPACING);
      const float rowLight = frontLight + (backLight - frontLight) * t;
      for (std::uint32_t seat = 0; seat < seats; ++seat)
      {
        const std::uint32_t key =
            ((sectionCounter * 4U + tier) * 64U + row) * 1024U + seat;
        if (unitHash(key, 1U, 0U) < Tuning::Crowd::EMPTY_SEAT_RATIO) continue;
        const float jitter =
            (unitHash(key, 2U, 0U) - 0.5f) * Tuning::Crowd::JITTER * 2.0f;
        const float along = (static_cast<float>(seat) + 0.5f + jitter) /
                            static_cast<float>(seats);
        const ImU32 clothes = crowdClothes(shape, key);
        const ImU32 skin =
            SKIN[static_cast<std::size_t>(unitHash(key, 6U, 0U) * 4.0f) %
                 SKIN.size()];
        const float light = rowLight * (0.85f + 0.3f * unitHash(key, 7U, 0U));
        CrowdDot& dot = geometry.crowd.emplace_back();
        dot.base = RenderMath::lerp(left, right, along);
        dot.body = shadeColor(clothes, light);
        dot.head = shadeColor(skin, light);
      }
    }
    face.crowdEnd = static_cast<std::uint32_t>(geometry.crowd.size());
  }

  void buildSection(const SectionShape& shape)
  {
    using S = Tuning::Stands;
    const ImU32 seats = S::SEAT_COLOR;
    const ProfilePoint frontBottom{0.0f, 0.0f};
    const ProfilePoint frontTop{0.0f, S::FRONT_WALL_HEIGHT};
    const ProfilePoint lowerTop{S::LOWER_DEPTH, S::LOWER_TOP};
    const ProfilePoint concourseTop{S::LOWER_DEPTH, S::CONCOURSE_TOP};
    const ProfilePoint upperTop{S::UPPER_DEPTH, S::UPPER_TOP};
    const ProfilePoint backTop{S::UPPER_DEPTH, S::BACK_WALL_TOP};
    const ProfilePoint roofBack{S::ROOF_BACK_DEPTH, S::ROOF_BACK_HEIGHT};
    const ProfilePoint roofFront{S::ROOF_FRONT_DEPTH, S::ROOF_FRONT_HEIGHT};
    const ProfilePoint fasciaBottom{S::ROOF_FRONT_DEPTH,
                                    S::ROOF_FRONT_HEIGHT - S::FASCIA_HEIGHT};

    Section section;
    section.side = shape.side;
    section.faceBegin = static_cast<std::uint32_t>(geometry.faces.size());
    section.centre = RenderMath::lerp(shape.left(concourseTop),
                                      shape.right(concourseTop), 0.5f);

    // Faces are stored back to front as seen from the pitch.
    addProfileFace(shape, upperTop, backTop, S::WALL_COLOR, S::WALL_COLOR);
    addProfileFace(shape, roofBack, roofFront, S::ROOF_UNDER_COLOR,
                   shadeColor(S::ROOF_UNDER_COLOR, 1.4f));
    Face& upper =
        addProfileFace(shape, concourseTop, upperTop, shadeColor(seats, 0.9f),
                       shadeColor(seats, S::BACK_ROW_LIGHT));
    addCrowd(upper, shape, 1U, concourseTop, upperTop, 0.9f, S::BACK_ROW_LIGHT);
    addProfileFace(shape, lowerTop, concourseTop, S::WALL_COLOR, S::WALL_COLOR);
    addProfileFace(shape, {S::LOWER_DEPTH, S::LOWER_TOP + S::WINDOW_INSET},
                   {S::LOWER_DEPTH, S::CONCOURSE_TOP - S::WINDOW_INSET},
                   shadeColor(S::WINDOW_COLOR, 0.7f), S::WINDOW_COLOR,
                   S::WINDOW_INSET);
    Face& lower =
        addProfileFace(shape, frontTop, lowerTop, shadeColor(seats, 1.1f),
                       shadeColor(seats, 0.8f));
    addCrowd(lower, shape, 0U, frontTop, lowerTop, 1.05f, 0.78f);
    addProfileFace(shape, frontBottom, frontTop, S::CONCRETE_COLOR,
                   shadeColor(S::CONCRETE_COLOR, 1.2f));
    addProfileFace(shape, fasciaBottom, roofFront, S::FASCIA_COLOR,
                   shadeColor(S::FASCIA_COLOR, 1.3f));
    addProfileFace(shape, roofFront, roofBack, S::ROOF_TOP_COLOR,
                   shadeColor(S::ROOF_TOP_COLOR, 0.85f));

    section.faceEnd = static_cast<std::uint32_t>(geometry.faces.size());
    geometry.sections.push_back(section);
    ++sectionCounter;
  }
};

void buildFloodlights(Geometry& geometry)
{
  using F = Tuning::Floodlight;
  const std::array<std::array<float, 2>, 4> positions{{
      {-F::CORNER_OFFSET_X, -F::CORNER_OFFSET_Y},
      {LENGTH + F::CORNER_OFFSET_X, -F::CORNER_OFFSET_Y},
      {-F::CORNER_OFFSET_X, WIDTH + F::CORNER_OFFSET_Y},
      {LENGTH + F::CORNER_OFFSET_X, WIDTH + F::CORNER_OFFSET_Y},
  }};
  for (const auto& position : positions)
  {
    Section section;
    section.side = Side::NONE;
    section.faceBegin = static_cast<std::uint32_t>(geometry.faces.size());
    section.centre = {position[0], position[1], F::MAST_HEIGHT * 0.5f};
    const float half = F::MAST_SIZE * 0.5f;
    const Box mast =
        makeAxisBox({position[0] - half, position[1] - half, 0.0f},
                    {position[0] + half, position[1] + half, F::MAST_HEIGHT},
                    F::MAST_COLOR);
    for (std::size_t faceIndex = 0; faceIndex < BOX_FACES.size() - 2;
         ++faceIndex)
    {
      Face& face = geometry.faces.emplace_back();
      for (std::size_t corner = 0; corner < 4; ++corner)
      {
        face.corners[corner] = mast.corners[BOX_FACES[faceIndex][corner]];
        face.colors[corner] = shadeColor(
            F::MAST_COLOR, face.corners[corner].z > 1.0f ? 1.0f : 0.6f);
      }
      face.normal = RenderMath::normalize(
          RenderMath::cross(face.corners[2] - face.corners[0],
                            face.corners[3] - face.corners[1]));
    }

    const Vec3 toCentre = RenderMath::normalize(
        Vec3{HALF_LENGTH - position[0], HALF_WIDTH - position[1], 0.0f});
    const Vec3 normal = RenderMath::normalize(
        toCentre * std::cos(F::HEAD_TILT) - UP * std::sin(F::HEAD_TILT));
    const Vec3 right = RenderMath::normalize(RenderMath::cross(UP, normal));
    const Vec3 panelUp = RenderMath::cross(normal, right);
    const Vec3 centre{position[0], position[1],
                      F::MAST_HEIGHT + F::HEAD_HEIGHT * 0.5f};
    const Vec3 halfRight = right * (F::HEAD_WIDTH * 0.5f);
    const Vec3 halfUp = panelUp * (F::HEAD_HEIGHT * 0.5f);
    Face& back = geometry.faces.emplace_back();
    back.corners = {centre - halfRight - halfUp, centre - halfRight + halfUp,
                    centre + halfRight + halfUp, centre + halfRight - halfUp};
    back.colors.fill(shadeColor(F::MAST_COLOR, 0.7f));
    back.normal = normal * -1.0f;
    Face& front = geometry.faces.emplace_back();
    front.corners = back.corners;
    front.colors = {F::LAMP_COLOR, shadeColor(F::LAMP_COLOR, 0.92f),
                    shadeColor(F::LAMP_COLOR, 0.92f), F::LAMP_COLOR};
    front.normal = normal;
    front.glow = F::GLOW_COLOR;
    section.faceEnd = static_cast<std::uint32_t>(geometry.faces.size());
    geometry.sections.push_back(section);
  }
}

void buildBoards(Geometry& geometry)
{
  using B = Tuning::Boards;
  struct Run
  {
    Vec3 start;
    Vec3 end;
    Vec3 normal;
  };
  const std::array<Run, 4> runs{{
      {{-B::END_OFFSET + 2.0f, -B::SIDE_OFFSET, 0.0f},
       {LENGTH + B::END_OFFSET - 2.0f, -B::SIDE_OFFSET, 0.0f},
       {0.0f, 1.0f, 0.0f}},
      {{-B::END_OFFSET + 2.0f, WIDTH + B::SIDE_OFFSET, 0.0f},
       {LENGTH + B::END_OFFSET - 2.0f, WIDTH + B::SIDE_OFFSET, 0.0f},
       {0.0f, -1.0f, 0.0f}},
      {{-B::END_OFFSET, 2.0f, 0.0f},
       {-B::END_OFFSET, WIDTH - 2.0f, 0.0f},
       {1.0f, 0.0f, 0.0f}},
      {{LENGTH + B::END_OFFSET, 2.0f, 0.0f},
       {LENGTH + B::END_OFFSET, WIDTH - 2.0f, 0.0f},
       {-1.0f, 0.0f, 0.0f}},
  }};
  std::uint8_t sponsor = 0;
  for (const Run& run : runs)
  {
    const Vec3 span = run.end - run.start;
    const float runLength = RenderMath::length(span);
    const Vec3 direction = span * (1.0f / runLength);
    const int count = static_cast<int>(runLength / (B::LENGTH + B::GAP));
    const float used = static_cast<float>(count) * (B::LENGTH + B::GAP);
    const Vec3 right = RenderMath::cross(run.normal * -1.0f, UP);
    for (int index = 0; index < count; ++index)
    {
      const float along =
          (runLength - used) * 0.5f +
          (static_cast<float>(index) + 0.5f) * (B::LENGTH + B::GAP);
      const Vec3 centre = run.start + direction * along;
      AdBoard& board = geometry.boards.emplace_back();
      board.origin = centre - right * (B::LENGTH * 0.5f);
      board.right = right;
      board.normal = run.normal;
      board.sponsor = static_cast<std::uint8_t>(sponsor++ % SPONSORS.size());
    }
    sponsor = static_cast<std::uint8_t>(sponsor + 3);
  }
}

void buildGoals(Geometry& geometry)
{
  using G = Tuning::Goal;
  const float yLeft = HALF_WIDTH - G::WIDTH * 0.5f;
  const float yRight = HALF_WIDTH + G::WIDTH * 0.5f;
  const float post = G::POST_SIZE;
  const float half = post * 0.5f;
  for (std::size_t index = 0; index < geometry.goals.size(); ++index)
  {
    const float line = index == 0 ? 0.0f : LENGTH;
    const float out = index == 0 ? -1.0f : 1.0f;
    Goal& goal = geometry.goals[index];
    goal.frame[0] =
        makeAxisBox({line - half, yLeft - post, 0.0f},
                    {line + half, yLeft, G::HEIGHT + post}, G::POST_COLOR);
    goal.frame[1] = makeAxisBox({line - half, yRight, 0.0f},
                                {line + half, yRight + post, G::HEIGHT + post},
                                G::POST_COLOR);
    goal.frame[2] =
        makeAxisBox({line - half, yLeft, G::HEIGHT},
                    {line + half, yRight, G::HEIGHT + post}, G::POST_COLOR);

    const float roofX = line + out * G::ROOF_DEPTH;
    const float baseX = line + out * G::BASE_DEPTH;
    const float roofZ = G::HEIGHT - G::ROOF_DROP;
    const auto rows = [](int value)
    { return static_cast<std::uint8_t>(value); };
    goal.nets[0] = {
        {Vec3{line, yLeft, G::HEIGHT}, Vec3{line, yRight, G::HEIGHT},
         Vec3{roofX, yRight, roofZ}, Vec3{roofX, yLeft, roofZ}},
        rows(G::NET_ROOF_ROWS),
        rows(G::NET_COLUMNS)};
    goal.nets[1] = {{Vec3{roofX, yLeft, roofZ}, Vec3{roofX, yRight, roofZ},
                     Vec3{baseX, yRight, 0.0f}, Vec3{baseX, yLeft, 0.0f}},
                    rows(G::NET_BACK_ROWS),
                    rows(G::NET_COLUMNS)};
    goal.nets[2] = {{Vec3{line, yLeft, 0.0f}, Vec3{line, yLeft, G::HEIGHT},
                     Vec3{roofX, yLeft, roofZ}, Vec3{baseX, yLeft, 0.0f}},
                    rows(G::NET_SIDE_COLUMNS),
                    rows(G::NET_BACK_ROWS)};
    goal.nets[3] = {{Vec3{line, yRight, 0.0f}, Vec3{line, yRight, G::HEIGHT},
                     Vec3{roofX, yRight, roofZ}, Vec3{baseX, yRight, 0.0f}},
                    rows(G::NET_SIDE_COLUMNS),
                    rows(G::NET_BACK_ROWS)};
    goal.supports[0] = {Vec3{roofX, yLeft, roofZ}, Vec3{roofX, yRight, roofZ}};
    goal.supports[1] = {Vec3{baseX, yLeft, 0.0f}, Vec3{baseX, yRight, 0.0f}};
  }
}
}  // namespace

Box makeAxisBox(Vec3 minimum, Vec3 maximum, ImU32 color)
{
  Box box;
  for (std::size_t corner = 0; corner < box.corners.size(); ++corner)
  {
    box.corners[corner] = {(corner & 1U) != 0U ? maximum.x : minimum.x,
                           (corner & 2U) != 0U ? maximum.y : minimum.y,
                           (corner & 4U) != 0U ? maximum.z : minimum.z};
  }
  box.color = color;
  return box;
}

void Geometry::build(const MatchKits& kits)
{
  ground.clear();
  markings.clear();
  faces.clear();
  sections.clear();
  crowd.clear();
  boards.clear();

  buildGround(*this);
  buildMarkings(*this);

  using S = Tuning::Stands;
  // Long stands reach the end stands' front line so the four corner infills
  // close the bowl without gaps.
  const float x0 = -S::END_FRONT;
  const float x1 = LENGTH + S::END_FRONT;
  const float y0 = -S::SIDE_FRONT;
  const float y1 = WIDTH + S::SIDE_FRONT;
  const Vec3 south{0.0f, -1.0f, 0.0f};
  const Vec3 north{0.0f, 1.0f, 0.0f};
  const Vec3 west{-1.0f, 0.0f, 0.0f};
  const Vec3 east{1.0f, 0.0f, 0.0f};
  StandBuilder stands(*this, kits);
  stands.buildStand({x0, y0, 0.0f}, east, south, x1 - x0, S::SIDE_SECTIONS,
                    Side::SOUTH);
  stands.buildStand({x1, y1, 0.0f}, west, north, x1 - x0, S::SIDE_SECTIONS,
                    Side::NORTH);
  stands.buildStand({x0, y1, 0.0f}, south, west, y1 - y0, S::END_SECTIONS,
                    Side::WEST);
  stands.buildStand({x1, y0, 0.0f}, north, east, y1 - y0, S::END_SECTIONS,
                    Side::EAST);
  stands.buildCorner({x0, y0, 0.0f}, west, south, Side::SOUTH);
  stands.buildCorner({x1, y0, 0.0f}, south, east, Side::SOUTH);
  stands.buildCorner({x1, y1, 0.0f}, east, north, Side::NORTH);
  stands.buildCorner({x0, y1, 0.0f}, north, west, Side::NORTH);
  buildFloodlights(*this);
  buildBoards(*this);
  buildGoals(*this);
  cornerFlags = {Vec3{0.0f, 0.0f, 0.0f}, Vec3{LENGTH, 0.0f, 0.0f},
                 Vec3{0.0f, WIDTH, 0.0f}, Vec3{LENGTH, WIDTH, 0.0f}};
}
}  // namespace Stadium3D
