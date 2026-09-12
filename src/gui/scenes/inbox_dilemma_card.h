// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "model/stories.h"

class GameController;

/**
 * @brief The two answers of a decision moment in the inbox: each answer
 * lists what it will do (morale, trust, match fitness, money) before the
 * manager picks it.
 */
namespace InboxDilemmaCard
{
/** @brief One effect line; sign tells a gain (+1) from a cost (-1). */
struct EffectLine
{
  std::string text;
  int sign = 0;
};

/**
 * @brief Localised effect lines of an answer, e.g. "Morale of Rossi: +5",
 * "Club cost: €12K". @p subject and @p other name the players involved.
 */
std::vector<EffectLine> effectLines(const DilemmaEffects& effects,
                                    const std::string& subject,
                                    const std::string& other);

/**
 * @brief Draws the answers of the open moment in the current card.
 * @return The answer taken this frame (0 or 1) once it has been applied,
 * or nothing.
 */
std::optional<int> render(GameController& controller);
}  // namespace InboxDilemmaCard
