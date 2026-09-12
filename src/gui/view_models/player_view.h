// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "global/stats_config.h"
#include "global/types.h"

class GameController;
class Player;

/**
 * @brief Cached, display-ready player data for tables and profiles.
 *
 * Rows are built once per refresh so rendering never recomputes overalls,
 * market values or formatted strings per frame.
 */
namespace PlayerView
{

/** @brief Broad positional group used for filters and colouring. */
enum class PositionGroup : uint8_t
{
  GOALKEEPER,
  DEFENDER,
  MIDFIELDER,
  FORWARD
};

/** @brief One table row. */
struct PlayerRow
{
  PlayerID id = 0;
  TeamID team_id = 0;
  std::string name;
  std::string name_lower;
  std::string role;
  PlayerRole role_id = PlayerRole::UNKNOWN;
  PositionGroup group = PositionGroup::MIDFIELDER;
  int age = 0;
  float overall = 0.0f;
  uint32_t wage = 0;
  int contract_years = 0;
  uint32_t market_value = 0;
  bool listed = false;
  float condition = 100.0f; /**< 0-100 physical freshness. */
  float morale = 60.0f;     /**< 0-100. */
  float form = 0.0f;        /**< Average recent match rating, 0 = none. */
  uint16_t injury_days = 0; /**< Days until fit, 0 = available. */
  uint8_t suspension = 0;   /**< League matches still to serve. */
  std::string value_text;   /**< Pre-formatted market value. */
  std::string wage_text;    /**< Pre-formatted weekly wage. */
};

/** @brief Suitability of a player for one broad position group. */
struct RoleFit
{
  PositionGroup group;
  float rating = 0.0f;
};

/** @brief Stats shown together on the profile. */
struct AttributeGroup
{
  const char* title_key;
  std::array<std::string_view, 3> stats;
};

/** @brief Attribute grouping for the profile screen. */
constexpr std::array<AttributeGroup, 3> ATTRIBUTE_GROUPS = {{
    {"PROFILE_GROUP_TECHNICAL", {"Shooting", "Passing", "Dribbling"}},
    {"PROFILE_GROUP_PHYSICAL", {"Pace", "Physicality", "Stamina"}},
    {"PROFILE_GROUP_TACTICAL", {"Vision", "Defending", "Goalkeeping"}},
}};

/** @brief Builds a display row for a player. */
PlayerRow makeRow(const GameController& controller, const Player& player);

/** @brief Broad group of a role. */
PositionGroup groupOf(PlayerRole role);

/** @brief Localisation key for a position group (short label). */
const char* groupKey(PositionGroup group);

/**
 * @brief Rates the player in every broad position group with the same
 * role-focus weights used for the overall rating, best first.
 */
std::vector<RoleFit> roleFits(const Player& player, const StatsConfig& config);

/** @brief Localised name of a player attribute (falls back to the raw name). */
std::string statLabel(std::string_view statName);

/** @brief Lower-cases ASCII text for case-insensitive search. */
std::string toLower(std::string_view text);

}  // namespace PlayerView
