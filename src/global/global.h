// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <array>
#include <cstdint>
#include <utility>

#include "gamedate.h"

// This file can be used for global constants, enums, and functions
// that are used across the entire project.
// If possible, use constexpr

/** @brief Team ID reserved for free agents. */
constexpr uint8_t FREE_AGENTS_TEAM_ID = 0;

/** @brief The name of the free agents team. */
constexpr const char* FREE_AGENTS_TEAM_NAME = "Free agents";

/** @brief Number of teams in each league. */
constexpr uint8_t TEAMS_PER_LEAGUE = 16;

/** @brief Minimum possible value for a player statistic. */
constexpr uint8_t MIN_STAT_VAL = 1;

/** @brief Maximum possible value for a player statistic. */
constexpr uint8_t MAX_STAT_VAL = 100;

// Player Development
/** @brief Base rate at which player statistics increase. */
constexpr float PLAYER_STAT_INCREASE_BASE = 0.03f;

/** @brief Rate at which the age factor decays for player development. */
constexpr float PLAYER_AGE_FACTOR_DECAY_RATE = 0.05f;

/** @brief Age at which player performance starts to decline. */
constexpr float PLAYER_AGE_FACTOR_DECLINE_AGE = 31.5f;

// Player Retirement
/** @brief Minimum age at which a player might consider retirement. */
constexpr uint8_t PLAYER_RETIREMENT_AGE_THRESHOLD = 32;

/** @brief Base probability of a player retiring. */
constexpr float PLAYER_RETIREMENT_BASE_CHANCE = 0.01f;

/** @brief Amount by which the retirement chance increases each year after the
 * threshold. */
constexpr float PLAYER_RETIREMENT_CHANCE_INCREASE_PER_YEAR = 0.05f;

/**
 * @brief This is the size of the grid where to insert players
 * it could be used to generate heatmaps where the actions
 * take place during a match simulation. So that the
 * local average stats of all players (attacker vs defenders)
 * do make more of a difference during the game.
 */
static constexpr uint8_t LINEUP_GRID_ROWS = 5;
static constexpr uint8_t LINEUP_GRID_COLS = 5;
static constexpr uint8_t LINEUP_GRID_SIZE = LINEUP_GRID_ROWS * LINEUP_GRID_COLS;

// Functions to access the array of lineup's grid

/**
 * @brief Converts a 2D grid position into a 1D index.
 * @param row The row index.
 * @param col The column index.
 * @return The 1D index representing the position in the lineup grid.
 */
constexpr uint8_t toIndex(uint8_t row, uint8_t col)
{
  return static_cast<uint8_t>(row * LINEUP_GRID_COLS + col);
}

/**
 * @brief Converts a 1D index into a 2D grid row/column pair.
 * @param i The 1D index in the lineup grid.
 * @return A pair containing the row and column indices.
 */
constexpr std::pair<int, int> toRowCol(int i)
{
  return {i / LINEUP_GRID_COLS, i % LINEUP_GRID_COLS};
}

/** @brief Array mapping each month (0-11) to its number of days (ignoring leap
 * years for simplicity). */
constexpr std::array<uint8_t, 12> DAYS_PER_MONTH = {31, 28, 31, 30, 31, 30,
                                                    31, 31, 30, 31, 30, 31};

/** @brief The starting date of the game. */
const GameDateValue START_DATE(2025, 7, 2);

/** @brief The date when the season officially starts. */
const GameDateValue START_SEASON_DATE(2025, 9, 1);
