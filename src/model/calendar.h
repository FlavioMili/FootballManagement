// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <vector>
#include "team.h"
#include "match.h"

/**
  * This class should be further improved to have
  * a 365 days year, and take track of it adding 
  * Transfer windows, holidays and make the Calendar
  * close to reality as per league + tournaments during
  * the year.
*/

class Week {
 public:
  explicit Week(int week_number = 0);
  void addMatch(const Matchup& matchup) {
    matches.push_back(matchup);
  }
  const std::vector<Matchup>& getMatches() const {
    return matches;
  }

 private:
  int week_number;
  std::vector<Matchup> matches;
};

class Calendar {
 public:
  Calendar() = default;
  void generate(const std::vector<Team>& teams);
  const std::vector<Week>& getWeeks() const;
  void setWeeks(const std::vector<Week>& weeks);

 private:
  std::vector<Week> weeks;
};
