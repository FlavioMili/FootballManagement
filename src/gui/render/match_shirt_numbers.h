// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <bitset>
#include <cstddef>
#include <utility>
#include <vector>

#include "gui/render/match_render_snapshot.h"
#include "model/player.h"

/**
 * Shirt numbers of one match, handed out the same way by the 2D and 3D
 * views: players wear their squad number. A player without one (an academy
 * player called up) gets a spare number from 50 up, which squad numbers
 * rarely reach, so it does not take the number of a teammate.
 */
class MatchShirtNumbers
{
 public:
  MatchShirtNumbers() { owners.reserve(40); }

  /** Forgets every number (a new fixture). */
  void reset()
  {
    home.reset();
    away.reset();
    owners.clear();
  }

  /** The player's number, assigned on first request; 0 without a player.
   * @p stats is unused since players have squad numbers; it stays for the
   * views that pass the engine's statistics. */
  int numberFor(const MatchRenderPlayer& player,
                const std::vector<PlayerMatchStats>* /*stats*/)
  {
    if (!player.player) return 0;
    for (const auto& [owner, number] : owners)
      if (owner == player.player) return number;
    std::bitset<NUMBERS>& worn = player.isHomeTeam ? home : away;
    int number = player.player->getSquadNumber();
    if (number <= 0 || number >= NUMBERS ||
        worn.test(static_cast<std::size_t>(number)))
      number = spareNumber(worn);
    if (number > 0) worn.set(static_cast<std::size_t>(number));
    owners.emplace_back(player.player, number);
    return number;
  }

 private:
  static constexpr int NUMBERS = 100;
  static constexpr int FIRST_SPARE = 50;

  static int spareNumber(const std::bitset<NUMBERS>& worn)
  {
    for (int number = FIRST_SPARE; number < NUMBERS; ++number)
      if (!worn.test(static_cast<std::size_t>(number))) return number;
    for (int number = 1; number < FIRST_SPARE; ++number)
      if (!worn.test(static_cast<std::size_t>(number))) return number;
    return 0;
  }

  std::bitset<NUMBERS> home;
  std::bitset<NUMBERS> away;
  std::vector<std::pair<const Player*, int>> owners;
};
