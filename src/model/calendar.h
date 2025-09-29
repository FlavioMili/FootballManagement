// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include "gamedate.h"
#include "match.h"
#include <map>
#include <vector>

class Calendar {
public:
  Calendar() = default;

  void generate(const GameDateValue &startDate);
  void addMatch(const Match &match);
  const std::map<GameDateValue, std::vector<Match>> &getFullCalendar() const;
  const std::vector<Match> &getMatchesForDate(const GameDateValue &date) const;
  std::vector<Match> &getMatchesForDate(const GameDateValue &date);

private:
  void generateSeasonFixtures(const GameDateValue &startDate);
  void generateFriendlies(const GameDateValue &startDate,
                          size_t numFriendlies = 4);

  std::map<GameDateValue, std::vector<Match>> schedule;
};
