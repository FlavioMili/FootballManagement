// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

class Team;  // this is needed so that we can get all players wages.

#include <cstdint>
/**
 * @class Finances
 * @brief Manages the finances of a club.
 *
 * This class handles the balance, wages, contracts, and other financial aspects
 * of a team.
 */
class Finances
{
 public:
  /**
   * @brief Constructor for the Finances class.
   * @param startingBalance The initial balance of the team.
   * @param team_ref A reference to the Team object.
   */
  Finances(std::int64_t startingBalance, Team& team_ref);

  /**
   * @brief Default destructor.
   */
  ~Finances() = default;

  /**
   * @brief Sets the ratio of the budget allocated to transfers vs wages.
   * @param ratio The ratio to set, clamped between 0.0 and 1.0.
   */
  void setTransferToWagesRatio(float ratio);

  /**
   * @brief Adds a specified amount to the balance.
   * @param amount The amount to add.
   */
  void addBalance(std::int64_t amount);

  /**
   * @brief Subtracts a specified amount from the balance.
   * @param amount The amount to subtract.
   */
  void subtractBalance(std::int64_t amount);

  /**
   * @brief Gets the current balance.
   * @return The current balance.
   */
  std::int64_t getBalance() const noexcept;

  /**
   * @brief Gets the current transfer to wages ratio.
   * @return The transfer to wages ratio.
   */
  float getTransferToWagesRatio() const noexcept;

  /**
   * @brief Gets the current total wage spending of the team.
   * @return The current wage spending.
   */
  int64_t getCurrentWageSpending() const;

 private:
  std::int64_t balance;
  Team& team;

  // Example: if this is 0.8 then we have 80% of the
  //          budget as transfer budget.
  float transfer_to_wages_ratio;
};
