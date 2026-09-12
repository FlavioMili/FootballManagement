// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <span>

#include "global/types.h"

/**
 * Persistent squad numbers (1-99, unique within a club's senior squad).
 *
 * A new squad is numbered the traditional way: the first goalkeeper wears 1,
 * the regulars 2-11 by position (2 right back, 3 left back, 4 and 5 centre
 * backs, 6 holding midfielder, 7 right wing, 8 centre midfielder, 9 striker,
 * 10 playmaker, 11 left wing), back-up goalkeepers 12, 13, 25 or 31, the
 * other seniors 14 and up and the youngsters 30 and up. A newcomer keeps the
 * number he wore before when it is free at his new club, otherwise he takes a
 * free number of his position.
 */
namespace SquadNumbers
{
inline constexpr int MIN_NUMBER = 1;
inline constexpr int MAX_NUMBER = 99;
/** Players up to this age without a regular place get a number from 30. */
inline constexpr int YOUNG_AGE = 20;

/** True for a number a player can wear (1-99). */
constexpr bool isValid(int number)
{
  return number >= MIN_NUMBER && number <= MAX_NUMBER;
}

/** Outcome of changing a player's squad number. */
enum class Change : std::uint8_t
{
  Changed, /*!< Free number taken. */
  Swapped, /*!< The teammate wearing it took the player's old number. */
  Invalid  /*!< Not 1-99, unknown player, free agent or academy player. */
};

/** One senior player of a club. */
struct Entry
{
  PlayerID id = 0;
  PlayerRole role = PlayerRole::UNKNOWN;
  double overall = 0.0;
  int age = 0;
  /** Current number (0: none). Kept when valid and not worn by a better
   * teammate; filled in by assign() otherwise. */
  std::uint8_t number = 0;
  /** Number wanted when he has none (e.g. the one he wore before a move). */
  std::uint8_t preferred = 0;
};

/**
 * Gives every player of @p squad a valid number, unique within the squad.
 * Valid numbers are kept (on a clash the better player keeps his); the
 * others are handed out best player first. A squad without any number is
 * numbered from scratch (1-11 for the regulars, see above); in a numbered
 * squad newcomers take their preferred number if free, else a free
 * traditional number of their position, else a free squad number.
 * Deterministic: depends only on the entries, not on their order.
 */
void assign(std::span<Entry> squad);
}  // namespace SquadNumbers
