// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <imgui.h>

#include <array>
#include <cstdint>
#include <vector>

#include "gui/render/match_kit_colors.h"
#include "gui/render/match_render_math.h"

/**
 * Static world-space geometry of the 3D match view: ground, markings, goals,
 * corner flags, advertising boards, stands with crowd, and floodlights.
 *
 * Everything is built once per match (the crowd needs the kit colours) and
 * only re-projected per frame by `MatchRenderer3D`. No RNG is involved: all
 * variation comes from a fixed integer hash so the stadium is identical on
 * every frame and every run.
 */
namespace Stadium3D
{
using RenderMath::Vec3;

/** Convex ground polygon (drawn in fixed order, no depth sorting). */
struct GroundPolygon
{
  std::array<Vec3, 8> points{};
  std::array<ImU32, 8> colors{};
  std::uint8_t count = 0;
};

/** Planar convex face of a stand or floodlight with per-corner colours. */
struct Face
{
  std::array<Vec3, 4> corners{};
  std::array<ImU32, 4> colors{};
  Vec3 normal;
  /** Crowd clumps drawn right after this face when it is visible. */
  std::uint32_t clumpBegin = 0;
  std::uint32_t clumpEnd = 0;
  /** Non-zero for floodlight heads: colour of the halo drawn on top. */
  ImU32 glow = 0;
};

/** One spectator: a small upright billboard on a tier. */
struct CrowdDot
{
  Vec3 base;
  ImU32 body = 0;
  ImU32 head = 0;
};

/**
 * A few neighbouring spectators (up to CLUMP_SEATS x CLUMP_ROWS) drawn as
 * one billboard when they are far away. Members are stored back row first
 * so nearer rows overlap the ones behind them.
 */
struct CrowdClump
{
  Vec3 base; /**< Bottom centre on the front row. */
  float halfWidth = 0.0f;
  /** Billboard height covering every member row, metres. */
  float height = 0.0f;
  /** Average colour of the members (the far-away look). */
  ImU32 color = 0;
  std::uint32_t dotBegin = 0;
  std::uint32_t dotEnd = 0;
};

/** Which stand a section belongs to; used to hide the stand behind the eye. */
enum class Side : std::uint8_t
{
  SOUTH,
  NORTH,
  WEST,
  EAST,
  NONE,
};

/** Depth-sorted unit: faces are drawn in stored order (back to front). */
struct Section
{
  std::uint32_t faceBegin = 0;
  std::uint32_t faceEnd = 0;
  Vec3 centre;
  Side side = Side::NONE;
};

/** Pitch-facing advertising board. */
struct AdBoard
{
  Vec3 origin; /**< Bottom-left corner as seen from the pitch. */
  Vec3 right;  /**< Unit vector along the board, left to right. */
  Vec3 normal; /**< Unit vector facing the pitch. */
  std::uint8_t sponsor = 0;
};

/** Oriented box given by its eight corners (bit 0 = +a, 1 = +b, 2 = +c). */
struct Box
{
  std::array<Vec3, 8> corners{};
  ImU32 color = 0;
};

/** Bilinear net panel with its mesh resolution. */
struct NetPanel
{
  std::array<Vec3, 4> corners{};
  std::uint8_t rows = 1;
  std::uint8_t columns = 1;
};

struct Goal
{
  std::array<Box, 3> frame{}; /**< Two posts and the crossbar. */
  std::array<NetPanel, 4> nets{};
  std::array<std::array<Vec3, 2>, 2> supports{};
};

struct SponsorStyle
{
  const char* text = "";
  ImU32 background = 0;
  ImU32 foreground = 0;
};

/** Original, fictional advertisers shown on the boards. */
inline constexpr std::array<SponsorStyle, 8> SPONSORS{{
    {"KESTRELINE", IM_COL32(18, 70, 150, 255), IM_COL32(255, 255, 255, 255)},
    {"VOLTRIX", IM_COL32(236, 196, 30, 255), IM_COL32(20, 20, 24, 255)},
    {"ORBANO TELECOM", IM_COL32(200, 30, 48, 255),
     IM_COL32(255, 255, 255, 255)},
    {"PINEFORGE", IM_COL32(16, 110, 70, 255), IM_COL32(240, 250, 240, 255)},
    {"AQUALYNE", IM_COL32(20, 170, 200, 255), IM_COL32(10, 30, 60, 255)},
    {"SOLENTA", IM_COL32(245, 245, 245, 255), IM_COL32(220, 70, 20, 255)},
    {"BRIGHTWELL", IM_COL32(30, 30, 36, 255), IM_COL32(250, 210, 60, 255)},
    {"TORVANA", IM_COL32(120, 40, 150, 255), IM_COL32(255, 255, 255, 255)},
}};

/** Face index table for `Box` corners; each face winds CCW from outside. */
inline constexpr std::array<std::array<std::uint8_t, 4>, 6> BOX_FACES{{
    {0, 4, 6, 2},
    {1, 3, 7, 5},
    {0, 1, 5, 4},
    {2, 6, 7, 3},
    {0, 2, 3, 1},
    {4, 5, 7, 6},
}};

/** Builds an axis-aligned box from its minimum and maximum corners. */
Box makeAxisBox(Vec3 minimum, Vec3 maximum, ImU32 color);

struct Geometry
{
  std::vector<GroundPolygon> ground;
  std::vector<GroundPolygon> markings;
  std::vector<Face> faces;
  std::vector<Section> sections;
  std::vector<CrowdDot> crowd;
  std::vector<CrowdClump> clumps;
  std::vector<AdBoard> boards;
  std::array<Goal, 2> goals{};
  std::array<Vec3, 4> cornerFlags{};

  /** Rebuilds every static element; the crowd wears the given kits. */
  void build(const MatchKits& kits);
};
}  // namespace Stadium3D
