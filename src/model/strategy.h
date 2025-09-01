// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

/* TODO: These might later become a JSON file so that they can be modded
 *
 * DESCRIPTION OF ALL SLIDERS
 *
 * pressing      : Team-wide intensity of pressing opponents (0 = none, 1 = maximum)
 * riskTaking    : Aggressiveness in attack vs. defensive caution (0 = very safe, 1 = very risky)
 * offensiveBias : Likelihood to pass forward or safer options (0 = mostly safe, 1 = mostly forward)
 * widthUsage    : Horizontal spread of the team on the field (0 = narrow, 1 = full width)
 * compactness   : Closeness of players to the goalkeeper (0 = very close, 1 = very high)
 *
 * These sliders influence movement, positioning, passing decisions, and 
 * pressing behavior for all players during simulation. They can be 
 * combined with per-role weight maps for fine-tuned behavior.
 */
struct StrategySliders {
  float pressing      = 0.5f;  // medium press
  float riskTaking    = 0.5f;  // medium risk
  float offensiveBias = 0.5f;  // balanced passing
  float widthUsage    = 0.5f;  // balanced width
  float compactness   = 0.5f;  // medium compactness
};

/**
 * RoleWeights defines the behavior of a single player during a match simulation.
 * - attackWeight  : tendency to engage in offensive actions
 * - defenseWeight : tendency to engage in defensive actions
 * - movementRadius: distance (in grid cells) the player can 
 *     move from their lineup position as a rule of thumb, this
 *     would be applied to a theoretical heatmap of where the actions
 *     are going to take place
 *
 * During simulation, these weights are applied only within the movementRadius
 * so that we might calculate better what is going on in a certain area of the field.
 */
struct RoleWeights {
  float attackWeight  = 0.5f;
  float defenseWeight = 0.5f;
  int movementRadius  = 1; // distance from lineup position
};

/**
 * Strategy represents the tactical setup of a team for a match.
 * It combines sliders and per-role weights to determine player behavior.
 */
class Strategy {
 public:
  Strategy(); // initializes global sliders and default weights

  /**
     * Get weights of a player at a specific grid cell.
     * - roleIndex: 0 = goalkeeper, 1-10 = outfield players
     * - gridPosition: grid position to evaluate
     * Returns 0 if the cell is outside the player's movement area.
     */
  float getAttackWeight(int roleIndex, int gridPosition) const;
  float getDefenseWeight(int roleIndex, int gridPosition) const;

  void setPressing(float value)    { sliders.pressing = value; }
  void setRiskTaking(float value)  { sliders.riskTaking = value; }
  void setOffensiveBias(float value) { sliders.offensiveBias = value; }
  void setWidthUsage(float value)  { sliders.widthUsage = value; }
  void setCompactness(float value) { sliders.compactness = value; }

  // --- Setters for player role weights ---
  void setOutfieldWeights(int playerIndex, float attack, float defense, int radius);

  RoleWeights& getRole(int roleIndex);                  // non-const
  const RoleWeights& getRole(int roleIndex) const;      // const

  void setAllSliders(const StrategySliders& newSliders); // set all sliders at once
  StrategySliders getSliders() const;                    // retrieve current slider state

  // apply same weights to all outfield
  void setAllOutfieldWeights(float attack, float defense, int radius);

 private:
  // Global sliders affecting all players
  StrategySliders sliders;

  // Per-role weight definitions
  RoleWeights goalkeeper = {0.0, 1.0, 1};   // single goalkeeper
  RoleWeights outfield[10]; // 10 outfield players
};
