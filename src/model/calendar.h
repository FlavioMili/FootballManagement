// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <map>
#include <vector>

#include "gamedate.h"
#include "match.h"

/**
 * @brief Season date rules shared by the calendar and the competitions.
 *
 * A season runs from July (friendlies) to May. League rounds are played on
 * weekends (Saturday/Sunday) with a few midweek (Wednesday) rounds, cups on
 * Wednesdays with the final on the Saturday after the last league weekend.
 * There is a winter break and four international breaks (September,
 * October, November, March) during which no competitive matches are played.
 */
namespace SeasonCalendar
{
constexpr uint8_t WEDNESDAY = 2;
constexpr uint8_t SATURDAY = 5;
constexpr uint8_t SUNDAY = 6;
constexpr uint8_t SEASON_START_MONTH = 7;
constexpr size_t MAX_CUP_ROUNDS = 8;
constexpr size_t PRESEASON_FRIENDLIES = 4;

/** @brief Weekday of a date, 0 = Monday ... 6 = Sunday. */
uint8_t dayOfWeek(const GameDateValue& date);

/** @brief Adds (or subtracts) days in O(1). */
GameDateValue addDays(const GameDateValue& date, int days);

/** @brief Calendar year in which the season containing @p date started. */
uint16_t seasonStartYear(const GameDateValue& date);

/** @brief First league Saturday (first Saturday on or after 15 August). */
GameDateValue leagueStart(uint16_t season_year);

/** @brief Last regular league Saturday (on or before 24 May). */
GameDateValue leagueEnd(uint16_t season_year);

bool isInternationalBreak(const GameDateValue& date);
bool isWinterBreak(const GameDateValue& date);

/** @brief No competitive fixtures may be scheduled on this date. */
bool isBlackout(const GameDateValue& date);

/**
 * @brief Anchor date of each league round: a Saturday for weekend rounds
 * (matches split over Saturday and Sunday) or a Wednesday for midweek rounds.
 */
std::vector<GameDateValue> leagueRoundDates(uint16_t season_year,
                                            size_t rounds);

/** @brief Date of each cup round; the last one (the final) is a Saturday. */
std::vector<GameDateValue> cupRoundDates(uint16_t season_year, size_t rounds);

/** @brief Preseason friendly Saturdays after @p from and before the league. */
std::vector<GameDateValue> friendlyDates(const GameDateValue& from,
                                         size_t count);

/** @brief First Wednesday after @p date that is not in a break. */
GameDateValue nextFreeMidweek(const GameDateValue& date);
}  // namespace SeasonCalendar

/**
 * @class Calendar
 * @brief Manages the match calendar for a season.
 *
 * This class is responsible for generating and storing the match schedule,
 * including season fixtures, cup ties and friendlies.
 */
class Calendar
{
 public:
  /**
   * @brief Default constructor.
   */
  Calendar() = default;

  /**
   * @brief Generates the full calendar of the season starting on a date.
   * @param startDate First day of the season (early July).
   */
  void generate(const class GameData& gamedata, const GameDateValue& startDate);

  /**
   * @brief Adds a single match to the calendar.
   * @param match The match to add.
   */
  void addMatch(const Match& match);

  /**
   * @brief Gets the full calendar schedule.
   * @return A map containing all scheduled matches grouped by date.
   */
  const std::map<GameDateValue, std::vector<Match>>& getFullCalendar() const;

  /**
   * @brief Gets the matches scheduled for a specific date.
   * @param date The date to query.
   * @return A vector of matches scheduled for the given date.
   */
  const std::vector<Match>& getMatchesForDate(const GameDateValue& date) const;
  std::vector<Match>& getMatchesForDateMutable(const GameDateValue& date);

  /** @brief Finds a fixture by its natural key (date, home, away). */
  const Match* findMatch(const GameDateValue& date, TeamID home_id,
                         TeamID away_id) const;
  Match* findMatch(const GameDateValue& date, TeamID home_id, TeamID away_id);

  /** @brief All fixtures of a team in date order. */
  std::vector<Match> getTeamFixtures(TeamID team_id) const;

 private:
  void generateSeasonFixtures(const class GameData& gamedata,
                              uint16_t season_year);

  void generateFriendlies(const class GameData& gamedata,
                          const GameDateValue& startDate,
                          size_t numFriendlies);

  std::map<GameDateValue, std::vector<Match>> schedule;
};
