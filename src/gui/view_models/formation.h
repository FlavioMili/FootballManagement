// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <array>
#include <span>
#include <unordered_set>
#include <vector>

#include "global/stats_config.h"
#include "global/types.h"

class Lineup;
class Player;

/**
 * @brief Formation presets and XI selection built on the Lineup API.
 *
 * Pitch coordinates follow Lineup: x runs from the own goal (0) to the
 * opponent's goal (1), y from the left touchline (0) to the right (1).
 */
namespace Formation
{

/** @brief One outfield slot of a formation. */
struct Slot
{
  PlayerRole role;
  Vector2F position;
};

/** @brief A named arrangement of ten outfield slots. */
struct Preset
{
  const char* name;
  std::array<Slot, 10> slots;
};

/** @brief 4-4-2, 4-3-3, 4-2-3-1, 3-5-2 and 5-3-2. */
extern const std::array<Preset, 5> PRESETS;

/**
 * @brief How well a player's natural role suits a slot role, in [0, 1].
 * 1 means natural; adjacent roles score partially; goalkeepers and outfield
 * players never swap.
 */
float roleFit(PlayerRole actual, PlayerRole expected);

/**
 * @brief Moves the current outfield players into the preset's slots, keeping
 * the same eleven and matching players to the slots that suit them best.
 */
void applyPreset(Lineup& lineup, const Preset& preset,
                 const StatsConfig& config);

/**
 * @brief Chooses the best goalkeeper and ten outfield players from the squad
 * for the preset, places them, and puts everyone else on the bench.
 */
void autoPick(Lineup& lineup, const Preset& preset,
              std::span<const Player* const> squad, const StatsConfig& config);

/**
 * @brief autoPick() that leaves unavailable players (injured, suspended) out
 * of the XI; they are kept on the bench so the squad list stays complete.
 */
void autoPickAvailable(Lineup& lineup, const Preset& preset,
                       std::span<const Player* const> squad,
                       const std::unordered_set<PlayerID>& unavailable,
                       const StatsConfig& config);

/** @brief Selected starters (goalkeeper and outfield) found in the set. */
size_t unavailableStarters(const Lineup& lineup,
                           const std::unordered_set<PlayerID>& unavailable);

/**
 * @brief Index of the preset matching the current positions, or -1 when the
 * shape has been customised.
 */
int detectPreset(const Lineup& lineup);

}  // namespace Formation
