// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include "gamedate.h"
#include <array>
#include <cstdint>
#include <utility>

// This file can be used for global constants, enums, and functions
// that are used across the entire project.
// If possible, use constexpr

constexpr uint8_t FREE_AGENTS_TEAM_ID = 0;
constexpr const char *FREE_AGENTS_TEAM_NAME = "Free agents";

constexpr uint8_t TEAMS_PER_LEAGUE = 16;
constexpr uint8_t MIN_STAT_VAL = 1;
constexpr uint8_t MAX_STAT_VAL = 100;

// Player Development
constexpr float PLAYER_STAT_INCREASE_BASE = 0.03f;
constexpr float PLAYER_AGE_FACTOR_DECAY_RATE = 0.05f;
constexpr float PLAYER_AGE_FACTOR_DECLINE_AGE = 31.5;

// Player Retirement
constexpr uint8_t PLAYER_RETIREMENT_AGE_THRESHOLD = 32;
constexpr float PLAYER_RETIREMENT_BASE_CHANCE = 0.01f;
constexpr float PLAYER_RETIREMENT_CHANCE_INCREASE_PER_YEAR = 0.05f;

/* This is the size of the grid where to insert players
 * it could be used to generate heatmaps where the actions
 * take place during a match simulation. So that the
 * local average stats of all players (attacker vs defenders)
 * do make more of a difference during the game.
 */
static constexpr uint8_t LINEUP_GRID_ROWS = 5;
static constexpr uint8_t LINEUP_GRID_COLS = 5;
static constexpr uint8_t LINEUP_GRID_SIZE = LINEUP_GRID_ROWS * LINEUP_GRID_COLS;

// Functions to access the array of lineup's grid
constexpr uint8_t toIndex(uint8_t row, uint8_t col) {
  return static_cast<uint8_t>(row * LINEUP_GRID_COLS + col);
}

constexpr std::pair<int, int> toRowCol(int i) {
  return {i / LINEUP_GRID_COLS, i % LINEUP_GRID_COLS};
}

constexpr std::array<uint8_t, 12> DAYS_PER_MONTH = {31, 28, 31, 30, 31, 30,
                                                    31, 31, 30, 31, 30, 31};
const GameDateValue START_DATE(2025, 7, 2);
const GameDateValue START_SEASON_DATE(2025, 9, 1);
