// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once
#include <array>
#include <vector>
#include <string>
#include "global.h"
#include "player.h"
#include "strategy.h"

/**
  * This class will manage the starting 11 of the team,
  * the reserves, possibly changing depending on the type
  * of competition of the next match, the strategy and 
  * tactics of the team.
*/
class Lineup {
public:
  Lineup();

  // Goalkeeper
  void setGoalkeeper(Player* gk);
  Player* getGoalkeeper() const;

  // Grid (outfield players)
  void placePlayer(int row, int col, Player* player);
  void removePlayer(int row, int col);
  Player* getPlayerAt(int row, int col) const;

  void placePlayer(int index, Player* player);
  void removePlayer(int index);
  Player* getPlayerAt(int index) const;

  // Helpers for index conversions
  static int toIndex(int row, int col) { return row * LINEUP_GRID_COLS + col; }
  static std::pair<int,int> toRowCol(int index) { return { index / LINEUP_GRID_COLS, index % LINEUP_GRID_COLS }; }

  // Reserves
  void setReserves(const std::vector<Player*>& subs);
  const std::vector<Player*>& getReserves() const;

  // Strategy
  void setStrategy(const Strategy& strat);
  const Strategy& getStrategy() const;

  // Debug / visualisation
  std::string toString() const;

  void generateStartingXI(const std::vector<Player>& allPlayers, const StatsConfig& stats_config);

  void gridClear() {goalkeeper = nullptr; grid.fill(nullptr); }

private:
  Player* goalkeeper;
  std::array<Player*, LINEUP_GRID_SIZE> grid;   // 25 slots, many may be empty
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
