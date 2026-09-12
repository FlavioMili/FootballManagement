// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "global/types.h"
#include "model/match.h"
#include "model/standings.h"

class Calendar;
class GameData;
class League;
class Team;
struct StatsConfig;

/**
 * @brief Domestic competitions: league pyramids, cups, knockout resolution
 * and promotion/relegation.
 *
 * A "country" is a pyramid rooted at a league without a parent league; every
 * league whose parent chain reaches the root belongs to it. Each country has
 * one domestic cup whose ID is the root league ID (with MatchType::CUP).
 */
namespace Competitions
{
/** Teams promoted/relegated between two linked divisions of 20 teams. */
constexpr size_t PROMOTION_SLOTS = 3;

/** Deterministic seed mixing (e.g. season year, competition, round). */
uint32_t mixSeed(uint32_t a, uint32_t b, uint32_t c);

// ---------------- Pyramid ----------------
/** Top league of the pyramid containing @p league_id. */
LeagueID rootLeague(const GameData& gamedata, LeagueID league_id);
/** 1 for a top division, 2 for the division below, ... */
uint8_t leagueTier(const GameData& gamedata, LeagueID league_id);
/** Root league IDs (one per country), ascending. */
std::vector<LeagueID> countryRoots(const GameData& gamedata);
/** All leagues of a country ordered by tier then ID. */
std::vector<LeagueID> countryLeagues(const GameData& gamedata, LeagueID root);

// ---------------- Domestic cup ----------------
struct CupRound
{
  uint8_t stage = 0; /*!< 1-based round number. */
  std::vector<Match> ties;
  bool complete = false;
};

struct CupStatus
{
  LeagueID cup_id = 0;
  std::string name;
  uint8_t total_rounds = 0;
  std::vector<CupRound> rounds;
  std::vector<TeamID> remaining; /*!< Teams not yet eliminated. */
  std::optional<TeamID> winner;
  std::optional<TeamID> runner_up;
};

/** Every club of the country's pyramid. */
std::vector<TeamID> cupEntrants(const GameData& gamedata, LeagueID root);
/**
 * Cup name in the current language: the CUP_NAME_<LEAGUE NAME> key of the
 * root league (e.g. CUP_NAME_ITALIAN_LEAGUE) when the language has one,
 * otherwise built from the data pack's league name ("X League" -> "X Cup").
 */
std::string cupName(const GameData& gamedata, LeagueID root);

/**
 * League name in the current language: the LEAGUE_NAME_<LEAGUE NAME> key
 * (e.g. LEAGUE_NAME_ITALIAN_LEAGUE) when the language has one, otherwise the
 * data pack's name, so custom packs keep their own names.
 */
std::string leagueName(const League& league);

/** Inbox argument naming @p league ("@LEAGUE_NAME_..." when translated). */
std::string leagueNameArg(const League& league);
/** Knockout rounds needed so that exactly one team remains. */
uint8_t cupRoundCount(size_t entrants);
/** Language key naming a cup round (final, semi-final, ...). */
const char* cupRoundLabelKey(uint8_t stage, uint8_t total_rounds);

CupStatus cupStatus(const Calendar& calendar, const GameData& gamedata,
                    LeagueID root);

/**
 * @brief Schedules round 1 of every domestic cup.
 *
 * When the entrant count is not a power of two only the lowest-tier clubs
 * play a preliminary round; everybody else receives a bye to round 2.
 * Midweek rounds are spread from Tuesday to Thursday: each tie takes the
 * day that best rests both clubs, then the quietest.
 */
void scheduleCupFirstRounds(Calendar& calendar, const GameData& gamedata,
                            uint16_t season_year);

/**
 * @brief Draws the next round of every cup whose latest round is complete.
 * @return Number of fixtures added to the calendar.
 */
size_t drawPendingCupRounds(Calendar& calendar, const GameData& gamedata,
                            uint16_t season_year, const GameDateValue& today);

/** @brief A cup round that has been drawn, as the calendar holds it. */
struct CupDraw
{
  LeagueID cup_id = 0;
  uint8_t stage = 0; /*!< 1-based round number. */
  uint8_t total_rounds = 0;
  /**
   * Day of the draw: the day the previous round was complete (the date of
   * its last tie); round 1 is drawn with the season's fixtures on 1 July.
   */
  GameDateValue drawn_on;
  std::vector<Match> ties; /*!< By date, then home and away IDs. */
};

/**
 * @brief Read-only view of a drawn round of the cup of @p root (nullopt
 * when that round has not been drawn). Never changes the calendar.
 */
std::optional<CupDraw> cupDraw(const Calendar& calendar,
                               const GameData& gamedata, LeagueID root,
                               uint8_t stage);

// ---------------- Knockout resolution ----------------
struct KnockoutResolution
{
  uint8_t home_extra_goals = 0;
  uint8_t away_extra_goals = 0;
  bool penalties = false;
  uint8_t home_penalties = 0;
  uint8_t away_penalties = 0;
};

struct ShootoutResult
{
  uint8_t home = 0;
  uint8_t away = 0;
};

/** Conversion chance of one kick (~75% for average taker and keeper). */
float penaltyConversionProbability(float shooting, float goalkeeping);

/**
 * @brief Best-of-five shootout with early finish, then sudden death.
 * @param home_takers Conversion probabilities of the home kickers in order.
 * @param away_takers Conversion probabilities of the away kickers in order.
 */
ShootoutResult simulateShootout(const std::vector<float>& home_takers,
                                const std::vector<float>& away_takers,
                                std::mt19937& rng);

/** Extra time (30 minutes) and, if still level, penalties. */
KnockoutResolution resolveDrawnKnockout(const Team& home, const Team& away,
                                        const StatsConfig& stats_config,
                                        uint32_t seed);

// ---------------- Promotion / relegation ----------------
struct LeagueMovement
{
  TeamID team_id = 0;
  LeagueID from = 0;
  LeagueID to = 0;
};

/**
 * @brief Swaps the bottom clubs of each parent league with the top clubs of
 * its child division(s), using final standings.
 */
std::vector<LeagueMovement> computeLeagueMovements(
    const GameData& gamedata,
    const std::unordered_map<LeagueID, std::vector<StandingRow>>& standings);

/** Moves the teams between leagues (team league IDs and league rosters). */
void applyLeagueMovements(GameData& gamedata,
                          const std::vector<LeagueMovement>& movements);
}  // namespace Competitions
