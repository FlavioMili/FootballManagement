// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <utility>
#include <vector>

#include "gui/render/match_render_math.h"
#include "gui/render/match_render_snapshot.h"
#include "model/player.h"

/**
 * Cosmetic shirt numbers of one match, handed out the same way by the 2D
 * and 3D views (players have no squad number of their own). Starters and
 * their roles come from the engine's statistics, so a view opened after
 * substitutions still numbers the starting eleven 1-11.
 */
class MatchShirtNumbers
{
 public:
  MatchShirtNumbers() { owners.reserve(40); }

  /** Forgets every number (a new fixture). */
  void reset()
  {
    home = {};
    away = {};
    owners.clear();
  }

  /** The player's number, assigned on first request; 0 without a player. */
  int numberFor(const MatchRenderPlayer& player,
                const std::vector<PlayerMatchStats>* stats)
  {
    if (!player.player) return 0;
    for (const auto& [owner, number] : owners)
      if (owner == player.player) return number;
    PlayerRole role = player.player->getRole();
    bool starter = true;
    if (stats)
    {
      for (const PlayerMatchStats& entry : *stats)
      {
        if (entry.playerId != player.player->getId()) continue;
        starter = entry.started;
        if (entry.role != PlayerRole::UNKNOWN) role = entry.role;
        break;
      }
    }
    if (player.isGoalkeeper && starter) role = PlayerRole::GK;
    const int number =
        (player.isHomeTeam ? home : away).take(role, starter);
    owners.emplace_back(player.player, number);
    return number;
  }

 private:
  RenderMath::ShirtNumbers home;
  RenderMath::ShirtNumbers away;
  std::vector<std::pair<const Player*, int>> owners;
};
