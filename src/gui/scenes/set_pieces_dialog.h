// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <array>
#include <string>
#include <vector>

#include "model/lineup.h"

class GameController;

/**
 * @brief Captain, vice-captain and set-piece takers of the managed lineup:
 * one dropdown per duty listing the starters (best suited first), with an
 * automatic choice that names who would take it.
 */
class SetPiecesDialog
{
 public:
  /** @brief Opens the dialog on the next render(). */
  void open()
  {
    open_requested = true;
    close_requested = false;
  }

  /** @brief Closes the dialog on the next render(). */
  void close() { close_requested = true; }

  /**
   * @brief Draws the dialog while it is open.
   * @return True on the frame a designation changed.
   */
  bool render(GameController& controller);

  /** @brief One-line summary for the lineup toolbar ("Captain: Name"). */
  static std::string captainSummary(const GameController& controller);

 private:
  friend class GameFlowTest_GUIFlowLifecycle_Test;

  struct Choice
  {
    PlayerID id = 0;
    std::string label; /*!< "Name  ·  78" (suitability) or the name. */
  };

  void rebuild(const GameController& controller);

  bool open_requested = false;
  bool close_requested = false;
  std::array<std::vector<Choice>, SET_PIECE_DUTY_COUNT> choices;
  std::array<std::string, SET_PIECE_DUTY_COUNT> automatic; /*!< Previews. */
  std::array<std::string, SET_PIECE_DUTY_COUNT> current;   /*!< Previews. */
};
