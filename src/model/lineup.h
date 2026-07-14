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

#include "global/global.h"
#include "global/types.h"
#include "player.h"
#include "strategy.h"

/**
 * @class Lineup
 * @brief Manages the starting XI and reserves of a team.
 *
 * This class will manage the starting 11 of the team,
 * the reserves, possibly changing depending on the type
 * of competition of the next match, the strategy and
 * tactics of the team.
 */
class Lineup
{
 public:
  /**
   * @brief Constructs a Lineup object.
   */
  Lineup();

  // Goalkeeper
  /**
   * @brief Sets the goalkeeper.
   * @param gk Pointer to the Player object.
   */
  void setGoalkeeper(Player* gk);

  /**
   * @brief Gets the goalkeeper.
   * @return Pointer to the goalkeeper Player object.
   */
  Player* getGoalkeeper() const;

  // Grid (outfield players)
  /**
   * @brief Places a player in the outfield grid by row and column.
   * @param row Row index.
   * @param col Column index.
   * @param player Pointer to the Player object.
   */
  void placePlayer(int row, int col, Player* player);

  /**
   * @brief Removes a player from the outfield grid by row and column.
   * @param row Row index.
   * @param col Column index.
   */
  void removePlayer(int row, int col);

  /**
   * @brief Gets a player from the outfield grid by row and column.
   * @param row Row index.
   * @param col Column index.
   * @return Pointer to the Player object.
   */
  Player* getPlayerAt(int row, int col) const;

  /**
   * @brief Places a player in the outfield grid by linear index.
   * @param index Linear index.
   * @param player Pointer to the Player object.
   */
  void placePlayer(int index, Player* player);

  /**
   * @brief Removes a player from the outfield grid by linear index.
   * @param index Linear index.
   */
  void removePlayer(int index);

  /**
   * @brief Gets a player from the outfield grid by linear index.
   * @param index Linear index.
   * @return Pointer to the Player object.
   */
  Player* getPlayerAt(int index) const;

  // Helpers for index conversions
  /**
   * @brief Converts row and column to a linear index.
   * @param row Row index.
   * @param col Column index.
   * @return The linear index.
   */
  static int toIndex(int row, int col) { return row * LINEUP_GRID_COLS + col; }

  /**
   * @brief Converts a linear index to row and column.
   * @param index Linear index.
   * @return A pair containing the row and column.
   */
  static std::pair<int, int> toRowCol(int index)
  {
    return {index / LINEUP_GRID_COLS, index % LINEUP_GRID_COLS};
  }

  // Reserves
  /**
   * @brief Sets the reserve players.
   * @param subs A vector of pointers to the reserve Player objects.
   */
  void setReserves(const std::vector<Player*>& subs);

  /**
   * @brief Gets the reserve players.
   * @return A vector of pointers to the reserve Player objects.
   */
  const std::vector<Player*>& getReserves() const;

  // Strategy
  /**
   * @brief Sets the strategy for this lineup.
   * @param strat The strategy to set.
   */
  void setStrategy(const Strategy& strat);

  /**
   * @brief Gets the strategy for this lineup.
   * @return The strategy.
   */
  const Strategy& getStrategy() const;

  // Debug / visualisation
  /**
   * @brief Converts the lineup to a string representation for debugging.
   * @return The string representation of the lineup.
   */
  std::string toString() const;

  /**
   * @brief Generates a starting XI automatically.
   * @param allPlayers A vector of all available player IDs.
   * @param stats_config The stats configuration for evaluating players.
   */
  void generateStartingXI(const std::vector<PlayerID>& allPlayers,
                          const StatsConfig& stats_config);

  /**
   * @brief Clears the grid and goalkeeper.
   */
  void gridClear()
  {
    goalkeeper = nullptr;
    grid.fill(nullptr);
  }

 private:
  Player* goalkeeper;
  std::array<Player*, LINEUP_GRID_SIZE> grid;  // 25 slots, many may be empty
  std::vector<Player*> reserves;
  Strategy strategy;

  // Helper
};

/***************************************************************
 * This is how to visualise the grid where to place the players:
 *
 *   GK: (always separate, fixed role)
 *
 *   Row 0 → furthest forward (attacking line)
 *   Row 4 → deepest outfield line (defensive line)
 *
 *   Example 5×5 grid (row × col):
 *
 *       (0,0)   (0,1)   (0,2)   (0,3)   (0,4)
 *       (1,0)   (1,1)   (1,2)   (1,3)   (1,4)
 *       (2,0)   (2,1)   (2,2)   (2,3)   (2,4)
 *       (3,0)   (3,1)   (3,2)   (3,3)   (3,4)
 *       (4,0)   (4,1)   (4,2)   (4,3)   (4,4)
 *
 * To place a player in a position, use:
 *   index = row * LINEUP_GRID_COLS + col;
 *
 * Example: Striker at (0,2) → index = 0 * 5 + 2 = 2
 *          Left-back at (4,0) → index = 4 * 5 + 0 = 20
 *
 ***************************************************************/
