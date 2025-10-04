// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "finances.h"
#include <algorithm>
#include <cstdint>
#include "gamedata.h"
#include "team.h"

Finances::Finances(std::int64_t startingBalance, Team& team_ref) :
  balance(startingBalance),
  team(team_ref),
  transfer_to_wages_ratio(0.5)
{};


void Finances::setTransferToWagesRatio(float ratio) {
  transfer_to_wages_ratio = std::clamp(ratio, 0.0f, 1.0f);
}

void Finances::addBalance(std::int64_t amount) {
  balance += amount;
}

void Finances::subtractBalance(std::int64_t amount) {
  balance -= amount;
}

std::int64_t Finances::getBalance() const noexcept {
  return balance;
}

float Finances::getTransferToWagesRatio() const noexcept {
  return transfer_to_wages_ratio;
}

int64_t Finances::getCurrentWageSpending() const {
  int64_t wages {};
  auto playerIDs = team.getPlayerIDs();

  for (auto& pID : playerIDs) {
    wages += GameData::instance().getPlayer(pID)->get().getWage();
  }
  return wages;
}
