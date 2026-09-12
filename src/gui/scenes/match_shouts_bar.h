// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstddef>
#include <string>

#include "gui/scenes/match_touchline.h"

/**
 * @brief Touchline shouts of the live match: one compact button per shout
 * (Shift+1 .. Shift+0, Shift+- on the keyboard). The shout in force is
 * highlighted and filled by the share of its effect that is left; shouts in
 * quick succession have less effect, which the fill shows right away.
 */
namespace MatchShoutsBar
{
/** @brief Shouts can be given now (the match is being played). */
bool available(const MatchEngine& engine);

/** @brief Height of the bar at @p width (it wraps onto more rows). */
float height(float width);

/** @brief Draws the bar in the current window, @p width wide. */
void render(const TouchlineContext& context, float width);

/**
 * @brief Shout slot of a Shift+key press by physical key (the number row,
 * then the key right of 0), or -1.
 */
int indexForScancode(int scancode);

/** @brief Name of the key that gives the shout at @p index with Shift. */
std::string keyName(std::size_t index);

/**
 * @brief Gives the shout at @p index of MatchChanges::SHOUTS and notes it
 * in the HUD status; false when shouts are not possible now.
 */
bool shout(const TouchlineContext& context, std::size_t index);
}  // namespace MatchShoutsBar
