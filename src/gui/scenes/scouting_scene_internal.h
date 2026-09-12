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
#include <string>

#include "model/scouting.h"

/**
 * @brief Drawing helpers shared by the translation units of ScoutingScene
 * (scouting_scene.cpp and scouting_scouts_page.cpp).
 */
namespace ScoutingUi
{
/** @brief Font DPI factor for pixel sizes. */
float dpi();

/** @brief ASCII lower case, for name searches. */
std::string lower(std::string text);

/** @brief "68–76". */
std::string rangeText(float low, float high);

ImVec4 gradeColor(ScoutGrade grade);
const char* gradeLabel(ScoutGrade grade);

/** @brief Thin bar showing how well the club knows a player (0-100). */
void knowledgeBar(std::uint8_t knowledge);
}  // namespace ScoutingUi
