// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <imgui.h>

#include <string>

#include "global/types.h"
#include "gui/view_models/match_changes.h"

class GameController;
class Lineup;
class MatchEngine;

/**
 * @brief What the matchday dialogs act on: the live engine and the managed
 * side, plus the match HUD's status line they report to.
 */
struct TouchlineContext
{
  GameController& controller;
  MatchEngine& engine;
  /** Side of the managed club in this match. */
  bool home;
  TeamID team_id;
  /** Shirt colour of the managed side on the view. */
  ImU32 kit;
  /** HUD status line and whether it reports a refusal. */
  std::string& status;
  bool& status_refused;
};

/** @brief Shared frame and wording of the substitutions and tactics dialogs. */
namespace Touchline
{
/**
 * @brief Opens (once) and begins a centred modal sized to the window: at
 * most @p width x @p height unscaled pixels and 94% of the work area.
 * @param opened Set once the popup was opened; reset it to open again.
 * @return True while the dialog is shown (pair with ImGui::EndPopup()).
 */
bool beginDialog(const char* title, bool& opened, float width, float height);

/** @brief Height to keep free under a dialog body for the button row. */
float footerHeight();

/** @brief "Pause the match while this is open" (a saved setting). */
void pauseSetting();

/** @brief Whether the match pauses while a matchday dialog is open. */
bool pausesMatch();

/** @brief Why a change is refused, with the players' names where useful. */
std::string refusalText(MatchChanges::Refusal refusal,
                        const MatchEngine& engine, const std::string& outName,
                        const std::string& inName);

/** @brief Name of a player of the matchday squad (empty when unknown). */
std::string playerName(const Lineup& lineup, PlayerID player);

/** @brief Condition bar colour (fresh, tiring, exhausted). */
ImVec4 conditionColor(float condition);
}  // namespace Touchline
