// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "model/calendar.h"

#include <algorithm>
#include <array>
#include <random>
#include <vector>

#include "database/gamedata.h"
#include "global/global.h"
#include "global/logger.h"
#include "model/competition.h"
#include "model/league.h"

namespace
{
// Proleptic Gregorian day numbers (days since 1970-01-01), after H. Hinnant.
int toDayNumber(int year, int month, int day)
{
  year -= month <= 2 ? 1 : 0;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const int year_of_era = year - era * 400;
  const int day_of_year = (153 * (month > 2 ? month - 3 : month + 9) + 2) / 5 +
                          day - 1;
  const int day_of_era = year_of_era * 365 + year_of_era / 4 -
                         year_of_era / 100 + day_of_year;
  return era * 146'097 + day_of_era - 719'468;
}

int toDayNumber(const GameDateValue& date)
{
  return toDayNumber(date.year, date.month, date.day);
}

GameDateValue fromDayNumber(int day_number)
{
  day_number += 719'468;
  const int era =
      (day_number >= 0 ? day_number : day_number - 146'096) / 146'097;
  const int day_of_era = day_number - era * 146'097;
  const int year_of_era = (day_of_era - day_of_era / 1'460 +
                           day_of_era / 36'524 - day_of_era / 146'096) /
                          365;
  const int day_of_year =
      day_of_era - (365 * year_of_era + year_of_era / 4 - year_of_era / 100);
  const int month_index = (5 * day_of_year + 2) / 153;
  const int day = day_of_year - (153 * month_index + 2) / 5 + 1;
  const int month = month_index < 10 ? month_index + 3 : month_index - 9;
  const int year = year_of_era + era * 400 + (month <= 2 ? 1 : 0);
  return GameDateValue(static_cast<uint16_t>(year),
                       static_cast<uint8_t>(month), static_cast<uint8_t>(day));
}

int weekdayOf(int day_number)
{
  // 1970-01-01 was a Thursday (3 when Monday is 0).
  return ((day_number + 3) % 7 + 7) % 7;
}

int firstWeekdayOnOrAfter(int day_number, int weekday)
{
  return day_number + (weekday - weekdayOf(day_number) + 7) % 7;
}

int lastWeekdayOnOrBefore(int day_number, int weekday)
{
  return day_number - (weekdayOf(day_number) - weekday + 7) % 7;
}

int nthSaturday(int year, int month, int nth)
{
  return firstWeekdayOnOrAfter(toDayNumber(year, month, 1),
                               SeasonCalendar::SATURDAY) +
         7 * (nth - 1);
}

struct MonthDay
{
  int month;
  int day;
  int year_offset;
};

// Preferred cup midweeks, spread from late August to late April.
constexpr std::array<MonthDay, 8> CUP_MIDWEEK_TARGETS = {{{8, 27, 0},
                                                          {9, 24, 0},
                                                          {10, 29, 0},
                                                          {12, 3, 0},
                                                          {1, 14, 1},
                                                          {2, 11, 1},
                                                          {3, 4, 1},
                                                          {4, 22, 1}}};

// International windows: nth Saturday of the month is skipped by leagues.
constexpr std::array<MonthDay, 4> INTERNATIONAL_BREAKS = {
    {{9, 2, 0}, {10, 2, 0}, {11, 3, 0}, {3, 4, 1}}};
constexpr int BREAK_DAYS_BEFORE_SATURDAY = 5;  // From the Monday.
constexpr int BREAK_DAYS_AFTER_SATURDAY = 3;   // To the Tuesday.

std::vector<int> cupMidweekCandidates(uint16_t season_year)
{
  std::vector<int> days;
  days.reserve(CUP_MIDWEEK_TARGETS.size());
  for (const MonthDay& target : CUP_MIDWEEK_TARGETS)
  {
    int day = firstWeekdayOnOrAfter(
        toDayNumber(season_year + target.year_offset, target.month, target.day),
        SeasonCalendar::WEDNESDAY);
    while (SeasonCalendar::isBlackout(fromDayNumber(day))) day += 7;
    days.push_back(day);
  }
  return days;
}

/** Picks @p count evenly spread elements of @p candidates (count <= size). */
std::vector<int> spreadPick(const std::vector<int>& candidates, size_t count)
{
  std::vector<int> picked;
  if (count == 0 || candidates.empty()) return picked;
  picked.reserve(count);
  for (size_t i = 0; i < count; ++i)
  {
    const size_t index = ((2 * i + 1) * candidates.size()) / (2 * count);
    picked.push_back(candidates[std::min(index, candidates.size() - 1)]);
  }
  return picked;
}
}  // namespace

// ---------------- SeasonCalendar ----------------

uint8_t SeasonCalendar::dayOfWeek(const GameDateValue& date)
{
  return static_cast<uint8_t>(weekdayOf(toDayNumber(date)));
}

GameDateValue SeasonCalendar::addDays(const GameDateValue& date, int days)
{
  return fromDayNumber(toDayNumber(date) + days);
}

uint16_t SeasonCalendar::seasonStartYear(const GameDateValue& date)
{
  return date.month >= SEASON_START_MONTH ? date.year
                                          : static_cast<uint16_t>(date.year - 1);
}

GameDateValue SeasonCalendar::leagueStart(uint16_t season_year)
{
  return fromDayNumber(
      firstWeekdayOnOrAfter(toDayNumber(season_year, 8, 15), SATURDAY));
}

GameDateValue SeasonCalendar::leagueEnd(uint16_t season_year)
{
  return fromDayNumber(
      lastWeekdayOnOrBefore(toDayNumber(season_year + 1, 5, 24), SATURDAY));
}

bool SeasonCalendar::isInternationalBreak(const GameDateValue& date)
{
  const int season_year = seasonStartYear(date);
  const int day = toDayNumber(date);
  return std::ranges::any_of(
      INTERNATIONAL_BREAKS,
      [&](const MonthDay& window)
      {
        const int saturday = nthSaturday(season_year + window.year_offset,
                                         window.month, window.day);
        return day >= saturday - BREAK_DAYS_BEFORE_SATURDAY &&
               day <= saturday + BREAK_DAYS_AFTER_SATURDAY;
      });
}

bool SeasonCalendar::isWinterBreak(const GameDateValue& date)
{
  constexpr uint8_t BREAK_START_DAY = 22;  // 22 December
  constexpr uint8_t BREAK_END_DAY = 8;     // 8 January
  return (date.month == 12 && date.day >= BREAK_START_DAY) ||
         (date.month == 1 && date.day <= BREAK_END_DAY);
}

bool SeasonCalendar::isBlackout(const GameDateValue& date)
{
  return isWinterBreak(date) || isInternationalBreak(date);
}

std::vector<GameDateValue> SeasonCalendar::cupRoundDates(uint16_t season_year,
                                                         size_t rounds)
{
  std::vector<GameDateValue> dates;
  if (rounds == 0) return dates;
  rounds = std::min(rounds, MAX_CUP_ROUNDS + 1);
  const std::vector<int> midweeks = cupMidweekCandidates(season_year);
  const size_t early_rounds = rounds - 1;
  dates.reserve(rounds);
  if (early_rounds == 1)
  {
    dates.push_back(fromDayNumber(midweeks.back()));
  }
  else if (early_rounds > 1)
  {
    const size_t last = midweeks.size() - 1;
    for (size_t i = 0; i < early_rounds; ++i)
    {
      const size_t index =
          (i * last + (early_rounds - 1) / 2) / (early_rounds - 1);
      dates.push_back(fromDayNumber(midweeks[std::min(index, last)]));
    }
  }
  dates.push_back(addDays(leagueEnd(season_year), 7));
  return dates;
}

std::vector<GameDateValue> SeasonCalendar::leagueRoundDates(
    uint16_t season_year, size_t rounds)
{
  const int first = toDayNumber(leagueStart(season_year));
  const int last = toDayNumber(leagueEnd(season_year));

  std::vector<int> weekends;
  for (int day = first; day <= last; day += 7)
  {
    if (!isBlackout(fromDayNumber(day)) && !isBlackout(fromDayNumber(day + 1)))
      weekends.push_back(day);
  }

  std::vector<int> picked;
  if (rounds <= weekends.size())
  {
    picked = spreadPick(weekends, rounds);
  }
  else
  {
    const std::vector<int> cup_days = cupMidweekCandidates(season_year);
    std::vector<int> midweeks;
    for (int day = first + 4; day < last; day += 7)
    {
      if (!isBlackout(fromDayNumber(day)) && !std::ranges::contains(cup_days, day))
        midweeks.push_back(day);
    }
    const size_t needed = rounds - weekends.size();
    picked = weekends;
    const std::vector<int> extra =
        spreadPick(midweeks, std::min(needed, midweeks.size()));
    picked.insert(picked.end(), extra.begin(), extra.end());
    // Oversized leagues spill into June, skipping the cup final weekend.
    for (int day = last + 14; picked.size() < rounds; day += 7)
      picked.push_back(day);
    std::ranges::sort(picked);
  }

  std::vector<GameDateValue> dates;
  dates.reserve(picked.size());
  for (int day : picked) dates.push_back(fromDayNumber(day));
  return dates;
}

std::vector<GameDateValue> SeasonCalendar::friendlyDates(
    const GameDateValue& from, size_t count)
{
  std::vector<GameDateValue> dates;
  const int latest = toDayNumber(leagueStart(seasonStartYear(from))) - 7;
  for (int day = firstWeekdayOnOrAfter(toDayNumber(from) + 7, SATURDAY);
       day <= latest && dates.size() < count; day += 7)
  {
    dates.push_back(fromDayNumber(day));
  }
  return dates;
}

GameDateValue SeasonCalendar::nextFreeMidweek(const GameDateValue& date)
{
  int day = firstWeekdayOnOrAfter(toDayNumber(date) + 1, WEDNESDAY);
  while (isBlackout(fromDayNumber(day))) day += 7;
  return fromDayNumber(day);
}

// ---------------- Calendar ----------------

void Calendar::generate(const class GameData& gamedata,
                        const GameDateValue& startDate)
{
  schedule.clear();
  const uint16_t season_year = SeasonCalendar::seasonStartYear(startDate);
  generateFriendlies(gamedata, startDate, SeasonCalendar::PRESEASON_FRIENDLIES);
  generateSeasonFixtures(gamedata, season_year);
  Competitions::scheduleCupFirstRounds(*this, gamedata, season_year);
}

void Calendar::addMatch(const Match& match)
{
  schedule[match.getDate()].push_back(match);
}

const std::map<GameDateValue, std::vector<Match>>& Calendar::getFullCalendar()
    const
{
  return schedule;
}

const std::vector<Match>& Calendar::getMatchesForDate(
    const GameDateValue& date) const
{
  static const std::vector<Match> no_matches;
  auto it = schedule.find(date);
  if (it != schedule.end())
  {
    return it->second;
  }
  return no_matches;
}

std::vector<Match>& Calendar::getMatchesForDateMutable(
    const GameDateValue& date)
{
  return schedule[date];
}

const Match* Calendar::findMatch(const GameDateValue& date, TeamID home_id,
                                 TeamID away_id) const
{
  const auto it = schedule.find(date);
  if (it == schedule.end()) return nullptr;
  const auto match = std::ranges::find_if(
      it->second, [&](const Match& candidate)
      {
        return candidate.getHomeTeamId() == home_id &&
               candidate.getAwayTeamId() == away_id;
      });
  return match == it->second.end() ? nullptr : &*match;
}

Match* Calendar::findMatch(const GameDateValue& date, TeamID home_id,
                           TeamID away_id)
{
  return const_cast<Match*>(
      static_cast<const Calendar&>(*this).findMatch(date, home_id, away_id));
}

std::vector<Match> Calendar::getTeamFixtures(TeamID team_id) const
{
  std::vector<Match> fixtures;
  for (const auto& [date, matches] : schedule)
  {
    for (const Match& match : matches)
    {
      if (match.getHomeTeamId() == team_id || match.getAwayTeamId() == team_id)
        fixtures.push_back(match);
    }
  }
  return fixtures;
}

void Calendar::generateSeasonFixtures(const class GameData& gamedata,
                                      uint16_t season_year)
{
  std::vector<LeagueID> league_ids;
  league_ids.reserve(gamedata.getLeagues().size());
  for (const auto& [id, league] : gamedata.getLeagues()) league_ids.push_back(id);
  std::ranges::sort(league_ids);

  for (const LeagueID league_id : league_ids)
  {
    const League& league = gamedata.getLeagues().at(league_id);
    std::vector<TeamID> team_ids = league.getTeamIDs();

    if (team_ids.size() < 2)
    {
      Logger::warn("Not enough teams to generate fixtures in: " +
                   league.getName());
      continue;
    }

    std::ranges::sort(team_ids);
    std::mt19937 rng(Competitions::mixSeed(season_year, league_id, 1));
    std::ranges::shuffle(team_ids, rng);
    if (team_ids.size() % 2 != 0)
    {
      team_ids.push_back(FREE_AGENTS_TEAM_ID);  // Bye for round robin
    }

    const size_t num_teams = team_ids.size();
    const size_t half_rounds = num_teams - 1;
    const std::vector<GameDateValue> round_dates =
        SeasonCalendar::leagueRoundDates(season_year, half_rounds * 2);

    // Circle method; venues flip every round so teams alternate home/away.
    std::vector<std::vector<std::pair<TeamID, TeamID>>> first_half(half_rounds);
    for (size_t round = 0; round < half_rounds; ++round)
    {
      for (size_t i = 0; i < num_teams / 2; ++i)
      {
        TeamID home_id = team_ids[i];
        TeamID away_id = team_ids[num_teams - 1 - i];
        if (home_id == FREE_AGENTS_TEAM_ID || away_id == FREE_AGENTS_TEAM_ID)
          continue;
        if (round % 2 == 1) std::swap(home_id, away_id);
        first_half[round].emplace_back(home_id, away_id);
      }
      const TeamID last_id = team_ids.back();
      team_ids.pop_back();
      team_ids.insert(team_ids.begin() + 1, last_id);
    }

    for (size_t round = 0; round < half_rounds * 2 && round < round_dates.size();
         ++round)
    {
      const bool second_half = round >= half_rounds;
      const auto& pairs = first_half[second_half ? round - half_rounds : round];
      const GameDateValue anchor = round_dates[round];
      const bool weekend =
          SeasonCalendar::dayOfWeek(anchor) == SeasonCalendar::SATURDAY;
      const auto stage = static_cast<uint8_t>(round + 1);
      for (size_t i = 0; i < pairs.size(); ++i)
      {
        const auto [home_id, away_id] = pairs[i];
        const GameDateValue date =
            weekend && (i + round) % 2 == 1 ? SeasonCalendar::addDays(anchor, 1)
                                            : anchor;
        if (second_half)
          addMatch(Match(away_id, home_id, date, MatchType::LEAGUE, league_id,
                         stage));
        else
          addMatch(Match(home_id, away_id, date, MatchType::LEAGUE, league_id,
                         stage));
      }
    }
  }
}

void Calendar::generateFriendlies(const class GameData& gamedata,
                                  const GameDateValue& startDate,
                                  size_t numFriendlies)
{
  std::vector<TeamID> team_ids;
  team_ids.reserve(gamedata.getTeams().size());
  for (const auto& [id, team] : gamedata.getTeams())
  {
    if (id != FREE_AGENTS_TEAM_ID) team_ids.push_back(id);
  }
  if (team_ids.size() < 2) return;
  std::ranges::sort(team_ids);

  const uint16_t season_year = SeasonCalendar::seasonStartYear(startDate);
  const std::vector<GameDateValue> dates =
      SeasonCalendar::friendlyDates(startDate, numFriendlies);
  for (size_t round = 0; round < dates.size(); ++round)
  {
    std::mt19937 rng(Competitions::mixSeed(
        season_year, static_cast<uint32_t>(round), 2));
    std::ranges::shuffle(team_ids, rng);
    for (size_t i = 0; i + 1 < team_ids.size(); i += 2)
    {
      addMatch(
          Match(team_ids[i], team_ids[i + 1], dates[round], MatchType::FRIENDLY));
    }
  }
}
