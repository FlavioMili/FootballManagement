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
  void setGoalkeeper(const Player* gk);

  /**
   * @brief Gets the goalkeeper.
   * @return Pointer to the goalkeeper Player object.
   */
  const Player* getGoalkeeper() const;

  // Outfield Players
  struct PositionedPlayer
  {
    const Player* player;
    Vector2F position;
  };

  /**
   * @brief Adds an outfield player to the pitch at the specified coordinate.
   * @param player Pointer to the Player object.
   * @param position The position on the pitch (x, y in [0.0, 1.0]).
   */
  void addOutfieldPlayer(const Player* player, Vector2F position);

  /**
   * @brief Updates the position of an existing outfield player.
   * @param playerID The ID of the player to move.
   * @param newPosition The new position on the pitch.
   * @return true if the player was found and moved, false otherwise.
   */
  bool moveOutfieldPlayer(PlayerID playerID, Vector2F newPosition);

  /**
   * @brief Removes an outfield player from the pitch.
   * @param playerID The ID of the player to remove.
   */
  void removeOutfieldPlayer(PlayerID playerID);

  /**
   * @brief Swaps a bench player with an outfield player.
   * @param benchPlayerID The ID of the substitute.
   * @param pitchPlayerID The ID of the current starting player.
   * @return true if successful, false otherwise.
   */
  bool swapPlayers(PlayerID benchPlayerID, PlayerID pitchPlayerID);

  /**
   * @brief Gets all positioned outfield players.
   * @return A vector of positioned players.
   */
  const std::vector<PositionedPlayer>& getOutfieldPlayers() const;

  // Reserves
  /**
   * @brief Sets the reserve players.
   * @param subs A vector of pointers to the reserve Player objects.
   */
  void setReserves(const std::vector<const Player*>& subs);

  /**
   * @brief Gets the reserve players.
   * @return A vector of pointers to the reserve Player objects.
   */
  const std::vector<const Player*>& getReserves() const;

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
   * @param gamedata The game data containing player information.
   * @param allPlayerIDs A vector of all available player IDs.
   * @param stats_config The stats configuration for evaluating players.
   */
  void generateStartingXI(const class GameData& gamedata,
                          const std::vector<PlayerID>& allPlayerIDs,
                          const StatsConfig& stats_config);

  /**
   * @brief Clears the outfield players and goalkeeper.
   */
  void clear()
  {
    goalkeeper = nullptr;
    outfield_players.clear();
  }

 private:
  const Player* goalkeeper;
  std::vector<PositionedPlayer> outfield_players;
  std::vector<const Player*> reserves;
  Strategy strategy;
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
