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
 * A season runs from July (friendlies) to May. Each league round is spread
 * over a long weekend (Friday to Monday) or, for the few midweek rounds,
 * over Tuesday and Wednesday, following the league's own day pattern
 * (round_days in leagues.json). Cups are played on Wednesdays with the final
 * on the Saturday after the last league weekend. Continental club
 * competitions use their own Tuesday-Thursday weeks. There is a winter break
 * and the international windows (September/October, November, March and
 * June) during which no competitive club matches are played.
 */
namespace SeasonCalendar
{
constexpr uint8_t MONDAY = 0;
constexpr uint8_t TUESDAY = 1;
constexpr uint8_t WEDNESDAY = 2;
constexpr uint8_t THURSDAY = 3;
constexpr uint8_t FRIDAY = 4;
constexpr uint8_t SATURDAY = 5;
constexpr uint8_t SUNDAY = 6;
/** Fewest days between two competitive matches of a club (48 hours). */
constexpr int MIN_REST_DAYS = 2;
/** Fewest days between a league match and a midweek cup or continental tie
 * that follows it (two free days in between). */
constexpr int MIN_REST_BEFORE_TIE = 3;

/** Days a club needs between an @p earlier and a @p later competitive
 * match: MIN_REST_BEFORE_TIE from a league match to a cup or continental
 * tie, MIN_REST_DAYS otherwise. */
int restDays(MatchType earlier, MatchType later);
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

/** @brief Days in which clubs release players to their national teams. */
struct InternationalWindow
{
  GameDateValue start; /*!< Players report (a Monday). */
  GameDateValue end;   /*!< Last day of duty (a Tuesday). */
  std::vector<GameDateValue> match_days;
  bool summer = false; /*!< June window (the finals in tournament years). */
};

/** @brief Windows of the season starting in @p season_year, by date. */
std::vector<InternationalWindow> internationalWindows(uint16_t season_year);

bool isInternationalBreak(const GameDateValue& date);
bool isWinterBreak(const GameDateValue& date);

/** Continental club weeks per season: eight league-phase matchdays and two
 * legs each of the play-off, round of 16, quarter- and semi-finals. */
constexpr size_t CONTINENTAL_WEEKS = 16;

/**
 * @brief Tuesday of every continental club week (see CONTINENTAL_WEEKS).
 * Continental matches are played Tuesday to Thursday; domestic midweek
 * rounds and cup ties avoid these weeks.
 */
std::vector<GameDateValue> continentalWeeks(uint16_t season_year);

/** @brief True from Tuesday to Thursday of a continental club week. */
bool isContinentalWeek(const GameDateValue& date);

/** @brief No competitive fixtures may be scheduled on this date. */
bool isBlackout(const GameDateValue& date);

/**
 * @brief Anchor date of each league round: a Saturday for weekend rounds
 * (matches spread from Friday to Monday) or a Wednesday for midweek rounds
 * (Tuesday and Wednesday).
 * @param variant 0 for top divisions; lower divisions (1) play the midweek
 * rounds they need in other weeks (or on up to two weekends after the top
 * flight has finished), so the midweek days of the two tiers do not pile up.
 */
std::vector<GameDateValue> leagueRoundDates(uint16_t season_year, size_t rounds,
                                            uint8_t variant = 0);

/** @brief Date of each cup round; the last one (the final) is a Saturday. */
std::vector<GameDateValue> cupRoundDates(uint16_t season_year, size_t rounds);

/**
 * @brief Saturdays of the pre-season friendly weeks after @p from and before
 * the league; each week's friendlies are spread from Tuesday to Sunday.
 */
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

  /**
   * @brief Keeps clubs rested around cup and continental ties.
   *
   * When cup or continental fixtures were added since the last call, every
   * unplayed league fixture after @p after that leaves a club less than
   * MIN_REST_BEFORE_TIE days before such a tie (or less than MIN_REST_DAYS
   * from any other competitive match) moves to the nearest day of its round
   * (Friday-Monday, or Tuesday-Thursday for midweek rounds) that suits both
   * clubs. Deterministic; a no-op when nothing was added.
   * @return Number of league fixtures moved.
   */
  size_t protectRest(const GameDateValue& after);

  /** @brief Treats the current fixtures as already checked by protectRest()
   * (fixtures loaded from a save keep their dates). */
  void markRestChecked() { rest_check_pending = false; }

 private:
  void generateSeasonFixtures(const class GameData& gamedata,
                              uint16_t season_year);

  void generateFriendlies(const class GameData& gamedata,
                          const GameDateValue& startDate, size_t numFriendlies);

  std::map<GameDateValue, std::vector<Match>> schedule;
  bool rest_check_pending = false;
};
