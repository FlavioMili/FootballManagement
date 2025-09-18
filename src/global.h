// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>

// This file can be used for global constants, enums, and functions
// that are used across the entire project.
// If possible, use constexpr

// PATHS
constexpr const char *LOGGER_PATH = "logs/log.txt";
constexpr const char *STATS_CONFIG_PATH = "assets/config/stats_config.json";
constexpr const char *LEAGUE_NAMES_PATH =
    "assets/names_files/league_names.json";
constexpr const char *TEAM_NAMES_PATH = "assets/names_files/team_names.json";
constexpr const char *FIRST_NAMES_PATH = "assets/names_files/first_names.json";
constexpr const char *LAST_NAMES_PATH = "assets/names_files/last_names.json";

constexpr const char *SCHEMA_PATH = "assets/db/schema.sql";
constexpr const char *QUERIES_PATH = "assets/db/queries.sql";

constexpr uint8_t FREE_AGENTS_TEAM_ID = 0;
constexpr const char *FREE_AGENTS_TEAM_NAME = "Free agents";

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

/* This is the size of the grid where to insert players
 * it could be used to generate heatmaps where the actions
 * take place during a match simulation. So that the
 * local average stats of all players (attacker vs defenders)
 * do make more of a difference during the game.
 */
static constexpr int LINEUP_GRID_ROWS = 5;
static constexpr int LINEUP_GRID_COLS = 5;
static constexpr int LINEUP_GRID_SIZE = LINEUP_GRID_ROWS * LINEUP_GRID_COLS;

// Functions to access the array of lineup's grid
constexpr int toIndex(int row, int col) { return row * LINEUP_GRID_COLS + col; }
constexpr std::pair<int, int> toRowCol(int i) {
  return {i / LINEUP_GRID_COLS, i % LINEUP_GRID_COLS};
}

// Fonts
constexpr const char *FONT_PATH = "assets/fonts/font.ttf";

/* This is an enum used in the settings manager to set the
 * text language, to be supported in the future
 * It will ve used also as nationality of Players
 */
enum class Language : uint8_t {
  EN, IT, ES, FR, DE, MX, CN, RO,
  PT, NL, SE, PL, GR, TR, RU, AR,
  JP, BR, KR, BE, CH, HR, DK, FI,
  IE, NO, SK, CZ, HU, UA, US, 
};

const std::unordered_map<Language, std::string> languageToString{
    {Language::EN, "English"},    {Language::IT, "Italian"},
    {Language::ES, "Spanish"},    {Language::FR, "French"},
    {Language::DE, "German"},     {Language::MX, "Mexican"},
    {Language::CN, "Chinese"},    {Language::RO, "Romanian"},
    {Language::PT, "Portuguese"}, {Language::NL, "Dutch"},
    {Language::SE, "Swedish"},    {Language::PL, "Polish"},
    {Language::GR, "Greek"},      {Language::TR, "Turkish"},
    {Language::RU, "Russian"},    {Language::AR, "Arabic"},
    {Language::JP, "Japanese"},   {Language::BR, "Brazilian"},
    {Language::KR, "Korean"},     {Language::BE, "Belgian"},
    {Language::CH, "Swiss"},      {Language::HR, "Croatian"},
    {Language::DK, "Danish"},     {Language::FI, "Finnish"},
    {Language::IE, "Irish"},      {Language::NO, "Norwegian"},
    {Language::SK, "Slovak"},     {Language::CZ, "Czech"},
    {Language::HU, "Hungarian"},  {Language::UA, "Ukrainian"},
    {Language::US, "American"}};

const std::unordered_map<std::string, Language> stringToLanguage{
    {"English", Language::EN},    {"Italian", Language::IT},
    {"Spanish", Language::ES},    {"French", Language::FR},
    {"German", Language::DE},     {"Mexican", Language::MX},
    {"Chinese", Language::CN},    {"Romanian", Language::RO},
    {"Portuguese", Language::PT}, {"Dutch", Language::NL},
    {"Swedish", Language::SE},    {"Polish", Language::PL},
    {"Greek", Language::GR},      {"Turkish", Language::TR},
    {"Russian", Language::RU},    {"Arabic", Language::AR},
    {"Japanese", Language::JP},   {"Brazilian", Language::BR},
    {"Korean", Language::KR},     {"Belgian", Language::BE},
    {"Swiss", Language::CH},      {"Croatian", Language::HR},
    {"Danish", Language::DK},     {"Finnish", Language::FI},
    {"Irish", Language::IE},      {"Norwegian", Language::NO},
    {"Slovak", Language::SK},     {"Czech", Language::CZ},
    {"Hungarian", Language::HU},  {"Ukrainian", Language::UA},
    {"American", Language::US}};
