// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

/* TODO: These might later become a JSON file so that they can be modded
 *
 * DESCRIPTION OF ALL SLIDERS
 *
 * pressing      : Team-wide intensity of pressing opponents (0 = none, 1 =
 * maximum) riskTaking    : Aggressiveness in attack vs. defensive caution (0 =
 * very safe, 1 = very risky) offensiveBias : Likelihood to pass forward or
 * safer options (0 = mostly safe, 1 = mostly forward) widthUsage    :
 * Horizontal spread of the team on the field (0 = narrow, 1 = full width)
 * compactness   : Closeness of players to the goalkeeper (0 = very close, 1 =
 * very high)
 *
 * These sliders influence movement, positioning, passing decisions, and
 * pressing behavior for all players during simulation. They can be
 * combined with per-role weight maps for fine-tuned behavior.
 */
/**
 * @struct StrategySliders
 * @brief Holds global strategy sliders for a team's tactical behavior.
 */
struct StrategySliders
{
  float pressing = 0.5f; /**< Team-wide intensity of pressing */
  float riskTaking =
      0.5f; /**< Aggressiveness in attack vs. defensive caution */
  float offensiveBias =
      0.5f;                 /**< Likelihood to pass forward vs. safer options */
  float widthUsage = 0.5f;  /**< Horizontal spread of the team */
  float compactness = 0.5f; /**< Closeness of players to the goalkeeper */
};

/**
 * @struct RoleWeights
 * @brief Defines the behavior of a single player during a match simulation.
 *
 * - attackWeight  : tendency to engage in offensive actions
 * - defenseWeight : tendency to engage in defensive actions
 * - movementRadius: distance (in grid cells) the player can
 *     move from their lineup position as a rule of thumb, this
 *     would be applied to a theoretical heatmap of where the actions
 *     are going to take place
 *
 * During simulation, these weights are applied only within the movementRadius
 * so that we might calculate better what is going on in a certain area of the
 * field.
 */
struct RoleWeights
{
  float attackWeight = 0.5f;  /**< Tendency to engage in offensive actions */
  float defenseWeight = 0.5f; /**< Tendency to engage in defensive actions */
  int movementRadius = 1;     /**< Distance from lineup position */
};

/**
 * @class Strategy
 * @brief Represents the tactical setup of a team for a match.
 *
 * It combines sliders and per-role weights to determine player behavior.
 */
class Strategy
{
 public:
  /**
   * @brief Default constructor, initializes global sliders and default weights.
   */
  Strategy();

  /**
   * @brief Get weights of a player at a specific grid cell.
   *
   * - roleIndex: 0 = goalkeeper, 1-10 = outfield players
   * - gridPosition: grid position to evaluate
   *
   * @param roleIndex The index of the player's role.
   * @param gridPosition The grid position to evaluate.
   * @return The attack weight at the given position, 0 if outside movement
   * area.
   */
  float getAttackWeight(int roleIndex, int gridPosition) const;

  /**
   * @brief Get defense weight of a player at a specific grid cell.
   * @param roleIndex The index of the player's role.
   * @param gridPosition The grid position to evaluate.
   * @return The defense weight at the given position, 0 if outside movement
   * area.
   */
  float getDefenseWeight(int roleIndex, int gridPosition) const;

  /** @brief Sets the pressing slider value. */
  void setPressing(float value) { sliders.pressing = value; }

  /** @brief Sets the risk taking slider value. */
  void setRiskTaking(float value) { sliders.riskTaking = value; }

  /** @brief Sets the offensive bias slider value. */
  void setOffensiveBias(float value) { sliders.offensiveBias = value; }

  /** @brief Sets the width usage slider value. */
  void setWidthUsage(float value) { sliders.widthUsage = value; }

  /** @brief Sets the compactness slider value. */
  void setCompactness(float value) { sliders.compactness = value; }

  // --- Setters for player role weights ---
  /**
   * @brief Sets the role weights for a specific outfield player.
   * @param playerIndex The index of the player (1-10).
   * @param attack The attack weight to set.
   * @param defense The defense weight to set.
   * @param radius The movement radius to set.
   */
  void setOutfieldWeights(int playerIndex, float attack, float defense,
                          int radius);

  /**
   * @brief Gets the role weights for a specific player (non-const).
   * @param roleIndex The index of the player.
   * @return Reference to the RoleWeights struct.
   */
  RoleWeights& getRole(int roleIndex);

  /**
   * @brief Gets the role weights for a specific player (const).
   * @param roleIndex The index of the player.
   * @return Const reference to the RoleWeights struct.
   */
  const RoleWeights& getRole(int roleIndex) const;

  /**
   * @brief Sets all sliders at once.
   * @param newSliders The new StrategySliders to apply.
   */
  void setAllSliders(const StrategySliders& newSliders);

  /**
   * @brief Retrieves the current slider state.
   * @return The current StrategySliders.
   */
  StrategySliders getSliders() const;

  /**
   * @brief Applies the same weights to all outfield players.
   * @param attack The attack weight to set.
   * @param defense The defense weight to set.
   * @param radius The movement radius to set.
   */
  void setAllOutfieldWeights(float attack, float defense, int radius);

 private:
  // Global sliders affecting all players
  StrategySliders sliders;

  // Per-role weight definitions
  RoleWeights goalkeeper = {0.0, 1.0, 1};  // single goalkeeper
  RoleWeights outfield[10];                // 10 outfield players
};
