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
