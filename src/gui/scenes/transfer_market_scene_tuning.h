// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstddef>

struct TransferMarketSceneTuning final
{
  struct Layout final
  {
    static constexpr float STATUS_RIGHT_OFFSET = 300.0f;
    static constexpr float BACK_BUTTON_WIDTH = 120.0f;
    static constexpr float STANDARD_BUTTON_WIDTH = 120.0f;
    static constexpr float BUTTON_HEIGHT = 30.0f;
    static constexpr float BUY_TABLE_HEIGHT = 450.0f;
    static constexpr float SECONDARY_TABLE_HEIGHT = 300.0f;
  };

  struct Filters final
  {
    static constexpr std::size_t SEARCH_BUFFER_SIZE = 64;
    static constexpr int MINIMUM_AGE = 15;
    static constexpr int MAXIMUM_AGE = 45;
    static constexpr int DEFAULT_MAXIMUM_AGE = 40;
    static constexpr float DEFAULT_MAXIMUM_PRICE = 100'000'000.0f;
    static constexpr float MAXIMUM_PRICE = 200'000'000.0f;
  };

  struct Tables final
  {
    static constexpr int BUY_COLUMN_COUNT = 10;
    static constexpr int LISTINGS_COLUMN_COUNT = 5;
    static constexpr int BIDS_COLUMN_COUNT = 5;
    static constexpr int HEADER_ROW_COUNT = 1;
  };

  struct MoneyInput final
  {
    static constexpr float SMALL_STEP = 100'000.0f;
    static constexpr float LARGE_STEP = 1'000'000.0f;
    static constexpr float WAGE_SMALL_STEP = 100.0f;
    static constexpr float WAGE_LARGE_STEP = 1'000.0f;
  };

  static constexpr float PERCENT_SCALE = 100.0f;
};
