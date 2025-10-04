// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

class Team; // this is needed so that we can get all players wages.

/**
  * This class will be used to manage the Finances 
  * of clubs, this means wages, contracts, balance, 
  * stadium improvement maybe and whatsoever
*/
#include <cstdint>
class Finances {
 public:
  Finances(std::int64_t startingBalance, Team& team_ref);
  ~Finances() = default;

  /*
  * This function clamps the value between 0 and 1 
  */
  void setTransferToWagesRatio(float ratio);

  void addBalance(std::int64_t amount);
  void subtractBalance(std::int64_t amount);  

  std::int64_t getBalance() const noexcept;
  float getTransferToWagesRatio() const noexcept;

  int64_t getCurrentWageSpending() const;

 private:
  std::int64_t balance;
  Team& team;

  // Example: if this is 0.8 then we have 80% of the 
  //          budget as transfer budget.
  float transfer_to_wages_ratio; 
};
