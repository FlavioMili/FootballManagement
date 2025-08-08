#pragma once

#include <cstdint>
#include <string>

// This file can be used for global constants, enums, and functions
// that are used across the entire project.
// If possible, use constexpr

constexpr uint8_t FREE_AGENTS_TEAM_ID = 0;
constexpr std::string FREE_AGENTS_TEAM_NAME = "Free agents";

constexpr uint8_t TEAMS_PER_LEAGUE = 16;
constexpr uint8_t MIN_STAT_VAL = 1;
constexpr uint8_t MAX_STAT_VAL = 100;

// Player Development
constexpr float PLAYER_STAT_INCREASE_BASE = 0.01f;
constexpr float PLAYER_AGE_FACTOR_DECAY_RATE = 0.05f;
constexpr float PLAYER_AGE_FACTOR_DECLINE_AGE = 31.5;

// Player Retirement
constexpr int PLAYER_RETIREMENT_AGE_THRESHOLD = 32;
constexpr float PLAYER_RETIREMENT_BASE_CHANCE = 0.01f;
constexpr float PLAYER_RETIREMENT_CHANCE_INCREASE_PER_YEAR = 0.05f;
