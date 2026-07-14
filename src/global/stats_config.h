// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <map>
#include <string>
#include <vector>

/**
 * @file stats_config.h
 * @brief Contains structures to simplify JSON parsing and loading of player
 * statistics.
 *
 * In the future, this file might include other structs useful for the project.
 * If so, it might be moved to src/.
 */

/**
 * @struct RoleFocus
 * @brief Represents the statistics and their weights for a specific player
 * role.
 */
struct RoleFocus
{
  std::vector<std::string>
      stats; /*!< List of statistic names relevant to the role. */
  std::vector<double> weights; /*!< Corresponding weights for each statistic. */
};

/**
 * @struct StatsConfig
 * @brief Configuration mapping roles to their focus areas and listing all
 * possible stats.
 */
struct StatsConfig
{
  std::map<std::string, RoleFocus>
      role_focus; /*!< Mapping of role names to their RoleFocus. */
  std::vector<std::string>
      possible_stats; /*!< List of all possible statistics. */
};
