// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>

/** Named transfer-market and contract-negotiation tuning values. */
struct TransferTuning final
{
  struct Contract final
  {
    static constexpr std::uint8_t MINIMUM_YEARS = 1;
    static constexpr std::uint8_t MAXIMUM_YEARS = 5;
    static constexpr int YOUNG_PLAYER_MAXIMUM_AGE = 23;
    static constexpr int PRIME_PLAYER_MAXIMUM_AGE = 29;
    static constexpr std::uint8_t YOUNG_PLAYER_MINIMUM_YEARS = 3;
    static constexpr std::uint8_t PRIME_PLAYER_MINIMUM_YEARS = 2;
    static constexpr std::uint8_t VETERAN_MINIMUM_YEARS = 1;
    static constexpr float TRANSFER_WAGE_RAISE = 1.10f;
    static constexpr float FREE_AGENT_WAGE_RAISE = 1.05f;
    static constexpr std::uint32_t MINIMUM_WEEKLY_WAGE = 500;
    static constexpr std::int64_t MINIMUM_WEEKLY_WAGE_BUDGET = 100'000;
    static constexpr double WEEKS_PER_YEAR = 52.0;
  };
};
