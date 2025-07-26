#pragma once

#include <vector>
#include "team.h"
#include "match.h"

class Week {
 public:
  Week(int week_number) : week_number(week_number) {}
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
