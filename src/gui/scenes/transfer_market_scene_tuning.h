// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

struct TransferMarketSceneTuning final
{
  struct Layout final
  {
    /** Dialog widths (single column / terms plus summary), clamped to
     * this share of the viewport. Two columns from this viewport width. */
    static constexpr float DIALOG_WIDTH = 620.0f;
    static constexpr float WIDE_DIALOG_WIDTH = 980.0f;
    static constexpr float TWO_COLUMN_MIN_VIEWPORT = 1100.0f;
    static constexpr float TERMS_COLUMN_WEIGHT = 1.25f;
    static constexpr float SUMMARY_COLUMN_WEIGHT = 1.0f;
    static constexpr float DIALOG_VIEWPORT_SHARE = 0.92f;
    static constexpr float SEARCH_WIDTH = 220.0f;
    static constexpr float COMBO_WIDTH = 150.0f;
    static constexpr float SLIDER_WIDTH = 170.0f;
    /** Tables scroll horizontally below these widths (unscaled). */
    static constexpr float TARGET_TABLE_MIN_WIDTH = 780.0f;
    static constexpr float WIDE_TABLE_MIN_WIDTH = 720.0f;
    /** Stretch weights of the name and club columns. */
    static constexpr float NAME_COLUMN_WEIGHT = 1.6f;
    static constexpr float CLUB_COLUMN_WEIGHT = 1.0f;
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
    static constexpr std::size_t RESULT_LIMIT = 1000;
    /** Affordable: estimated value up to the budget times this (fees can
     * be spread over instalments). */
    static constexpr double AFFORDABLE_VALUE_MULTIPLE = 1.3;
    /** Maximum-value presets of the filter (euros). */
    static constexpr std::array<std::int64_t, 8> MAX_VALUE_STEPS = {
        250'000,   500'000,    1'000'000,  2'000'000,
        5'000'000, 10'000'000, 20'000'000, 50'000'000};
    /** Players shown in the Recommended tab. */
    static constexpr std::size_t RECOMMENDED_LIMIT = 30;
  };

  struct Tables final
  {
    static constexpr int TARGET_COLUMN_COUNT = 12;
    static constexpr int SQUAD_COLUMN_COUNT = 9;
    static constexpr int OFFER_COLUMN_COUNT = 7;
    static constexpr int TALK_COLUMN_COUNT = 6;
    static constexpr int LOAN_COLUMN_COUNT = 7;
    static constexpr int HISTORY_COLUMN_COUNT = 6;
    static constexpr std::size_t HISTORY_LIMIT = 2000;
  };

  static constexpr float PERCENT_SCALE = 100.0f;
};
