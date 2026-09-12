// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <imgui.h>

#include <cstdint>

#include "global/types.h"

/** Colours of one strip, packed as `IM_COL32`. */
struct KitColors
{
  ImU32 shirt = 0;
  ImU32 trim = 0;
  ImU32 shorts = 0;
  ImU32 socks = 0;
};

/** The four strips worn in a match. */
struct MatchKits
{
  KitColors home;
  KitColors away;
  KitColors homeGoalkeeper;
  KitColors awayGoalkeeper;
};

/**
 * Picks deterministic, original strips from the team ids. The away side falls
 * back to a change strip when its shirt is too close to the home shirt, and
 * both goalkeepers wear colours distinct from every outfield shirt and from
 * each other.
 */
MatchKits chooseMatchKits(TeamID homeTeam, TeamID awayTeam);

/** Perceptual ("redmean") RGB distance between two packed colours. */
float kitColorDistance(ImU32 first, ImU32 second);

/** Minimum `kitColorDistance` treated as a readable contrast. */
inline constexpr float KIT_CLASH_DISTANCE = 190.0f;

/** Scales the RGB channels of a packed colour, keeping its alpha. */
inline ImU32 shadeColor(ImU32 color, float factor)
{
  const auto scale = [factor](ImU32 value, int shift)
  {
    const float scaled =
        static_cast<float>((value >> shift) & 0xFFU) * factor + 0.5f;
    return static_cast<ImU32>(scaled < 0.0f     ? 0.0f
                              : scaled > 255.0f ? 255.0f
                                                : scaled)
           << shift;
  };
  return scale(color, IM_COL32_R_SHIFT) | scale(color, IM_COL32_G_SHIFT) |
         scale(color, IM_COL32_B_SHIFT) | (color & IM_COL32_A_MASK);
}

/** Linear blend of two packed colours (alpha included). */
inline ImU32 mixColor(ImU32 first, ImU32 second, float t)
{
  const auto blend = [t](ImU32 a, ImU32 b, int shift)
  {
    const float from = static_cast<float>((a >> shift) & 0xFFU);
    const float to = static_cast<float>((b >> shift) & 0xFFU);
    return static_cast<ImU32>(from + (to - from) * t + 0.5f) << shift;
  };
  return blend(first, second, IM_COL32_R_SHIFT) |
         blend(first, second, IM_COL32_G_SHIFT) |
         blend(first, second, IM_COL32_B_SHIFT) |
         blend(first, second, IM_COL32_A_SHIFT);
}

/** Replaces the alpha channel of a packed colour. */
constexpr ImU32 withAlpha(ImU32 color, std::uint8_t alpha)
{
  return (color & ~IM_COL32_A_MASK) |
         (static_cast<ImU32>(alpha) << IM_COL32_A_SHIFT);
}

/** Small deterministic integer hash shared by the 3D view's cosmetics. */
constexpr std::uint32_t cosmeticHash(std::uint32_t value)
{
  value ^= value >> 16;
  value *= 0x7feb352dU;
  value ^= value >> 15;
  value *= 0x846ca68bU;
  value ^= value >> 16;
  return value;
}
