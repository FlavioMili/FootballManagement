// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "gui/render/match_kit_colors.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <initializer_list>

namespace
{
constexpr ImU32 WHITE = IM_COL32(240, 241, 244, 255);
constexpr ImU32 NAVY = IM_COL32(22, 34, 78, 255);
constexpr ImU32 BLACK = IM_COL32(28, 28, 32, 255);

constexpr std::array<KitColors, 14> HOME_KITS{{
    {IM_COL32(200, 28, 40, 255), WHITE, WHITE, IM_COL32(200, 28, 40, 255)},
    {IM_COL32(28, 72, 190, 255), WHITE, WHITE, IM_COL32(28, 72, 190, 255)},
    {IM_COL32(112, 176, 232, 255), NAVY, WHITE, IM_COL32(112, 176, 232, 255)},
    {IM_COL32(18, 128, 68, 255), WHITE, WHITE, IM_COL32(18, 128, 68, 255)},
    {IM_COL32(246, 204, 36, 255), NAVY, NAVY, IM_COL32(246, 204, 36, 255)},
    {BLACK, WHITE, BLACK, BLACK},
    {WHITE, NAVY, NAVY, WHITE},
    {IM_COL32(242, 118, 28, 255), BLACK, BLACK, IM_COL32(242, 118, 28, 255)},
    {IM_COL32(122, 28, 52, 255), IM_COL32(120, 190, 235, 255), WHITE,
     IM_COL32(122, 28, 52, 255)},
    {IM_COL32(98, 48, 150, 255), IM_COL32(236, 190, 60, 255), WHITE,
     IM_COL32(98, 48, 150, 255)},
    {NAVY, IM_COL32(210, 40, 50, 255), NAVY, NAVY},
    {IM_COL32(0, 138, 140, 255), WHITE, BLACK, IM_COL32(0, 138, 140, 255)},
    {IM_COL32(228, 108, 158, 255), BLACK, BLACK, IM_COL32(228, 108, 158, 255)},
    {IM_COL32(124, 128, 138, 255), IM_COL32(200, 240, 60, 255), BLACK,
     IM_COL32(124, 128, 138, 255)},
}};

/// Change strips tried in order when the preferred away shirt clashes.
constexpr std::array<KitColors, 4> CHANGE_KITS{{
    {WHITE, BLACK, BLACK, WHITE},
    {BLACK, IM_COL32(246, 204, 36, 255), BLACK, BLACK},
    {IM_COL32(246, 204, 36, 255), NAVY, NAVY, IM_COL32(246, 204, 36, 255)},
    {IM_COL32(112, 176, 232, 255), NAVY, NAVY, IM_COL32(112, 176, 232, 255)},
}};

constexpr std::array<KitColors, 6> GOALKEEPER_KITS{{
    {IM_COL32(150, 226, 40, 255), BLACK, BLACK, IM_COL32(150, 226, 40, 255)},
    {IM_COL32(255, 140, 22, 255), BLACK, BLACK, IM_COL32(255, 140, 22, 255)},
    {IM_COL32(222, 42, 164, 255), BLACK, BLACK, IM_COL32(222, 42, 164, 255)},
    {IM_COL32(38, 204, 222, 255), NAVY, NAVY, IM_COL32(38, 204, 222, 255)},
    {IM_COL32(24, 24, 26, 255), IM_COL32(150, 226, 40, 255),
     IM_COL32(24, 24, 26, 255), IM_COL32(24, 24, 26, 255)},
    {IM_COL32(250, 232, 40, 255), BLACK, BLACK, IM_COL32(250, 232, 40, 255)},
}};

bool contrastsWithAll(ImU32 candidate, std::initializer_list<ImU32> others)
{
  for (const ImU32 other : others)
    if (kitColorDistance(candidate, other) < KIT_CLASH_DISTANCE) return false;
  return true;
}

KitColors pickGoalkeeperKit(TeamID team, std::initializer_list<ImU32> avoid)
{
  const std::size_t start =
      cosmeticHash(team + 0x9e37U) % GOALKEEPER_KITS.size();
  for (std::size_t offset = 0; offset < GOALKEEPER_KITS.size(); ++offset)
  {
    const KitColors& kit =
        GOALKEEPER_KITS[(start + offset) % GOALKEEPER_KITS.size()];
    if (contrastsWithAll(kit.shirt, avoid)) return kit;
  }
  return GOALKEEPER_KITS[start];
}
}  // namespace

float kitColorDistance(ImU32 first, ImU32 second)
{
  const auto channel = [](ImU32 color, int shift)
  { return static_cast<float>((color >> shift) & 0xFFU); };
  const float redA = channel(first, IM_COL32_R_SHIFT);
  const float redB = channel(second, IM_COL32_R_SHIFT);
  const float meanRed = (redA + redB) * 0.5f;
  const float dr = redA - redB;
  const float dg =
      channel(first, IM_COL32_G_SHIFT) - channel(second, IM_COL32_G_SHIFT);
  const float db =
      channel(first, IM_COL32_B_SHIFT) - channel(second, IM_COL32_B_SHIFT);
  return std::sqrt((2.0f + meanRed / 256.0f) * dr * dr + 4.0f * dg * dg +
                   (2.0f + (255.0f - meanRed) / 256.0f) * db * db);
}

MatchKits chooseMatchKits(TeamID homeTeam, TeamID awayTeam)
{
  MatchKits kits;
  kits.home = HOME_KITS[cosmeticHash(homeTeam) % HOME_KITS.size()];
  kits.away = HOME_KITS[cosmeticHash(awayTeam) % HOME_KITS.size()];
  if (homeTeam == awayTeam ||
      !contrastsWithAll(kits.away.shirt, {kits.home.shirt}))
  {
    for (const KitColors& change : CHANGE_KITS)
    {
      if (contrastsWithAll(change.shirt, {kits.home.shirt}))
      {
        kits.away = change;
        break;
      }
    }
  }
  kits.homeGoalkeeper =
      pickGoalkeeperKit(homeTeam, {kits.home.shirt, kits.away.shirt});
  kits.awayGoalkeeper = pickGoalkeeperKit(
      awayTeam, {kits.home.shirt, kits.away.shirt, kits.homeGoalkeeper.shirt});
  return kits;
}
