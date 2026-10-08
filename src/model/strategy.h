// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <algorithm>
#include <vector>

#include "model/tactics.h"

/**
 * @struct StrategySliders
 * @brief Team-wide tactical preferences in [0, 1].
 *
 * MatchEngine combines these base values with score state, relative team
 * strength and fading shouts in computeEffectiveSliders(). Movement and action
 * selection consume those effective values. Individual behavior comes from
 * SlotInstruction/RoleProfile in tactics.h. See
 * docs/development/player-behavior.md.
 */
struct StrategySliders
{
  float pressing = 0.5f; /**< Team-wide intensity of pressing */
  float riskTaking =
      0.5f; /**< Aggressiveness in attack vs. defensive caution */
  float offensiveBias =
      0.5f;                 /**< Likelihood to pass forward vs. safer options */
  float widthUsage = 0.5f;  /**< Horizontal spread of the team */
  float compactness = 0.5f; /**< Squeeze defensive lines and follow the ball */
};

/**
 * @struct RoleWeights
 * @brief Legacy grid-based strategy weights, retained by the Strategy API.
 *
 * getAttackWeight()/getDefenseWeight() evaluate these within movementRadius
 * grid cells. The live MatchEngine does not call those methods: extend its
 * player behavior through tactical slot instructions and RoleProfile instead.
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
 * Stores team sliders, tactical slot/keeper roles, possession offsets and
 * opposition orders. The engine copies a Strategy and resolves its role
 * profiles; changing a club's Strategy alone does not update a running match.
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
  void setPressing(float value)
  {
    sliders.pressing = std::clamp(value, 0.0f, 1.0f);
  }

  /** @brief Sets the risk taking slider value. */
  void setRiskTaking(float value)
  {
    sliders.riskTaking = std::clamp(value, 0.0f, 1.0f);
  }

  /** @brief Sets the offensive bias slider value. */
  void setOffensiveBias(float value)
  {
    sliders.offensiveBias = std::clamp(value, 0.0f, 1.0f);
  }

  /** @brief Sets the width usage slider value. */
  void setWidthUsage(float value)
  {
    sliders.widthUsage = std::clamp(value, 0.0f, 1.0f);
  }

  /** @brief Sets the compactness slider value. */
  void setCompactness(float value)
  {
    sliders.compactness = std::clamp(value, 0.0f, 1.0f);
  }

  // --- Setters for player role weights ---
  /**
   * @brief Sets the role weights for a specific outfield player.
   * @param playerIndex The zero-based outfield index (0-9).
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

  // --- Roles, duties and the in-possession shape ---
  /** Every stored slot instruction (all formations used so far). */
  const std::vector<SlotInstruction>& getSlotInstructions() const
  {
    return slots;
  }
  /** Replaces the slot instructions (persistence). */
  void setSlotInstructions(std::vector<SlotInstruction> instructions);
  /**
   * Instruction of the slot at @p anchor (lineup coordinates): the nearest
   * stored one within TacticsTuning::SLOT_MATCH_DISTANCE, else nullptr.
   */
  const SlotInstruction* findSlot(Vector2F anchor) const;
  /** Stores the instruction of the slot at its anchor (replacing it). */
  void setSlot(const SlotInstruction& instruction);
  TacticalRole getKeeperRole() const { return keeper_role; }
  void setKeeperRole(TacticalRole role);

  /**
   * Instructions against players of the next opponent. Not saved with the
   * tactic: the club's opposition plan is the saved copy and refreshes them.
   */
  const std::vector<PlayerInstruction>& getOppositionOrders() const
  {
    return opposition_orders;
  }
  void setOppositionOrders(std::vector<PlayerInstruction> orders);

 private:
  // Global sliders affecting all players
  StrategySliders sliders;

  // Per-role weight definitions
  RoleWeights goalkeeper = {0.0f, 1.0f, 1};  // single goalkeeper
  RoleWeights outfield[10];                  // 10 outfield players

  std::vector<SlotInstruction> slots;
  TacticalRole keeper_role = TacticalRole::Standard;
  std::vector<PlayerInstruction> opposition_orders;
};
