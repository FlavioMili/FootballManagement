// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstddef>
#include <cstdint>

struct TransferMarketSceneTuning final
{
  struct Layout final
  {
    static constexpr float STANDARD_BUTTON_WIDTH = 150.0f;
    /** Dialog width, clamped to this share of the viewport. */
    static constexpr float DIALOG_WIDTH = 620.0f;
    static constexpr float DIALOG_VIEWPORT_SHARE = 0.92f;
    static constexpr float LABEL_WIDTH = 190.0f;
    static constexpr float SEARCH_WIDTH = 220.0f;
    static constexpr float COMBO_WIDTH = 150.0f;
    static constexpr float SLIDER_WIDTH = 170.0f;
    static constexpr float INPUT_WIDTH = 200.0f;
    /** Tables scroll horizontally below these widths. */
    static constexpr float TARGET_TABLE_MIN_WIDTH = 880.0f;
    static constexpr float WIDE_TABLE_MIN_WIDTH = 820.0f;
    /** Summary tiles per row before they wrap. */
    static constexpr int SUMMARY_TILES = 5;
    static constexpr float SUMMARY_TILE_MIN_WIDTH = 170.0f;
  };

  struct Filters final
  {
    static constexpr std::size_t SEARCH_BUFFER_SIZE = 64;
    static constexpr int MINIMUM_AGE = 15;
    static constexpr int MAXIMUM_AGE = 45;
    static constexpr int DEFAULT_MAXIMUM_AGE = 40;
    static constexpr int MAXIMUM_OVERALL = 99;
    /** Players estimated per search (best estimates first). */
    static constexpr std::size_t RESULT_LIMIT = 400;
  };

  struct Tables final
  {
    static constexpr int TARGET_COLUMN_COUNT = 11;
    static constexpr int SQUAD_COLUMN_COUNT = 9;
    static constexpr int OFFER_COLUMN_COUNT = 7;
    static constexpr int TALK_COLUMN_COUNT = 6;
    static constexpr int LOAN_COLUMN_COUNT = 7;
    static constexpr int HISTORY_COLUMN_COUNT = 6;
    static constexpr std::size_t HISTORY_LIMIT = 2000;
  };

  struct MoneyInput final
  {
    static constexpr std::uint32_t SMALL_STEP = 100'000;
    static constexpr std::uint32_t LARGE_STEP = 1'000'000;
    static constexpr std::uint32_t WAGE_SMALL_STEP = 100;
    static constexpr std::uint32_t WAGE_LARGE_STEP = 1'000;
    static constexpr std::uint16_t TARGET_STEP = 1;
    static constexpr std::uint16_t TARGET_LARGE_STEP = 5;
  };

  static constexpr float PERCENT_SCALE = 100.0f;
};
