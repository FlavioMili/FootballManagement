// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <string>
#include <string_view>

#include "global/types.h"

/**
 * @class RoleUtils
 * @brief Utilities for handling player roles and their pitch mappings.
 */
class RoleUtils
{
 public:
  /**
   * @brief Converts a PlayerRole enum to its string representation.
   */
  static std::string toString(PlayerRole role);

  /**
   * @brief Localised short position name for display ("GK", "POR"). The
   * codes of toString() are for persistence and parsing only.
   */
  static const char* shortName(PlayerRole role);

  /**
   * @brief Localised full position name ("Goalkeeper", "Portiere").
   */
  static const char* longName(PlayerRole role);

  /**
   * @brief Inbox argument for the short name ("@ROLE_SHORT_GK"), so saved
   * messages follow the current language.
   */
  static std::string shortNameArg(PlayerRole role);

  /**
   * @brief Converts a string to a PlayerRole enum.
   */
  static PlayerRole fromString(std::string_view role_str);

  /**
   * @brief Gets the base pitch coordinate for a given role.
   * Coordinates are normalized: X [0.0 (home goal) to 1.0 (away goal)], Y [0.0
   * (top) to 1.0 (bottom)].
   */
  static Vector2F getBaseCoordinate(PlayerRole role);

  /**
   * @brief Gets the broad category of the role (e.g. "Striker", "Defender").
   * Used for backward compatibility with older stats logic.
   */
  static std::string getBroadCategory(PlayerRole role);
};
