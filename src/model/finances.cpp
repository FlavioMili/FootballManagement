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

Finances::Finances(std::int64_t balance) :
  balance(balance),
  transfer_to_wages_ratio(0.5)
{};


void Finances::setTransferToWagesRatio(double ratio) {
  transfer_to_wages_ratio = std::clamp(ratio, 0.0, 1.0);
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

double Finances::getTransferToWagesRatio() const noexcept {
  return transfer_to_wages_ratio;
}
