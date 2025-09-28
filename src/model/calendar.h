#pragma once
#include "gamedate.h"
#include "league.h"
#include <map>
#include <vector>

struct Matchup {
  uint16_t home_team_id;
  uint16_t away_team_id;
};

class Calendar {
 public:
  Calendar() = default;

  void generate(const League league, const GameDateValue &startDate);

  // Get matches on a specific date
  const std::vector<Matchup> &getMatches(const GameDateValue &date) const;

  // modify matches
  void addMatch(const GameDateValue &date, const Matchup &matchup);

  const std::map<GameDateValue, std::vector<Matchup>> &getFixtures() const { return fixtures; };

 private:
  std::map<GameDateValue, std::vector<Matchup>> fixtures;

  GameDateValue scheduleRound(const std::vector<int> &team_ids,
                              GameDateValue date, bool reverse = false);
};
