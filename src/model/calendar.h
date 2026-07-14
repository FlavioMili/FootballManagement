// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <map>
#include <vector>

#include "gamedate.h"
#include "match.h"

/**
 * @class Calendar
 * @brief Manages the match calendar for a season.
 *
 * This class is responsible for generating and storing the match schedule,
 * including season fixtures and friendlies.
 */
class Calendar
{
 public:
  /**
   * @brief Default constructor.
   */
  Calendar() = default;

  /**
   * @brief Generates the full calendar starting from a given date.
   * @param startDate The date to start generating the calendar from.
   */
  void generate(class GameData& gamedata, const GameDateValue& startDate);

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

 private:
  /**
   * @brief Generates the season fixtures.
   * @param startDate The date to start the season from.
   */
  void generateSeasonFixtures(class GameData& gamedata,
                              const GameDateValue& startDate);

  /**
   * @brief Generates friendly matches.
   * @param startDate The date to start scheduling friendlies from.
   * @param numFriendlies The number of friendlies to schedule. Default is 4.
   */
  void generateFriendlies(class GameData& gamedata,
                          const GameDateValue& startDate,
                          size_t numFriendlies = 4);

  std::map<GameDateValue, std::vector<Match>> schedule;
};
