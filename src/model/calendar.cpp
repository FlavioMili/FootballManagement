#include "calendar.h"
#include "gamedate.h" // for GameDateValue
#include "global/logger.h"
#include "league.h"
#include "team.h"
#include <algorithm>
#include <vector>

// Add a match to a date
void Calendar::addMatch(const GameDateValue &date, const Matchup &matchup) {
  fixtures[date].push_back(matchup);
}

// Get matches for a date (empty vector if none)
const std::vector<Matchup> &
Calendar::getMatches(const GameDateValue &date) const {
  static const std::vector<Matchup> empty;
  auto it = fixtures.find(date);
  return it != fixtures.end() ? it->second : empty;
}

// Generate fixtures starting from startDate
void Calendar::generate(const League league, const GameDateValue &startDate) {
  fixtures.clear();
  std::vector<uint16_t> team_ids = league.getTeamIDs();

  if (team_ids.size() < 2 || team_ids.size() % 2) return;

  size_t num_teams = team_ids.size();
  size_t num_rounds = (num_teams - 1) * 2;
  GameDateValue currentDate = startDate;

  for (size_t round = 0; round < num_rounds; ++round) {
    std::vector<Matchup> roundMatches;

    for (size_t i = 0; i < num_teams / 2; ++i) {
      uint16_t home_id = team_ids[i];
      uint16_t away_id = team_ids[num_teams - 1 - i];

      // swap home/away according to round
      if (round < (num_teams - 1)) { // first half
        if (round % 2 == 1)
          std::swap(home_id, away_id);
      } else { // second half
        if (round % 2 == 0)
          std::swap(home_id, away_id);
      }

      // Create the matchup and add to round matches
      Matchup match(home_id, away_id);
      roundMatches.push_back(match);

      // Debug logging
      Logger::debug("Team " + std::to_string(home_id) + " vs " +
                    std::to_string(away_id) + " will be on " +
                    currentDate.toString());
    }

    // assign all matches of this round to currentDate
    fixtures[currentDate] = roundMatches;

    // move to next week
    currentDate.nextWeek();

    // rotate teams (except first team)
    uint16_t last_id = team_ids.back();
    team_ids.pop_back();
    team_ids.insert(team_ids.begin() + 1, last_id);
  }
}
