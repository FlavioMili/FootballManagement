// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "model/calendar.h"
#include "database/gamedata.h"
#include "global/global.h"
#include "global/logger.h"
#include "model/league.h"
#include <algorithm>
#include <vector>

void Calendar::generate(const GameDateValue &startDate) {
  schedule.clear();
  generateSeasonFixtures(startDate+50);
  generateFriendlies(startDate);
}

void Calendar::addMatch(const Match &match) {
  schedule[match.getDate()].push_back(match);
}

const std::map<GameDateValue, std::vector<Match>> &
Calendar::getFullCalendar() const {
  return schedule;
}

const std::vector<Match> &
Calendar::getMatchesForDate(const GameDateValue &date) const {
  static const std::vector<Match> no_matches;
  auto it = schedule.find(date);
  if (it != schedule.end()) {
    return it->second;
  }
  return no_matches;
}
void Calendar::generateSeasonFixtures(const GameDateValue &startDate) {
  auto leagues = GameData::instance().getLeaguesVector();
  for (auto &l : leagues) {
    League league = l.get();
    std::vector<uint16_t> team_ids = league.getTeamIDs();

    if (team_ids.size() < 2) {
      Logger::warn("Not enough teams to generate fixtures in: " +
                   league.getName());
      continue;
    }

    if (team_ids.size() % 2 != 0) {
      team_ids.push_back(0); // Add a dummy team for round robin
    }

    size_t num_teams = team_ids.size();
    size_t num_rounds = (num_teams - 1) * 2; // Full season (home and away)
    GameDateValue currentDate = startDate;

    for (size_t round = 0; round < num_rounds; ++round) {
      for (size_t i = 0; i < num_teams / 2; ++i) {
        uint16_t home_id = team_ids[i];
        uint16_t away_id = team_ids[num_teams - 1 - i];

        if (home_id == 0 || away_id == 0) {
          continue;
        }

        if (round >= (num_rounds / 2)) {
          std::swap(home_id, away_id);
        }

        addMatch(Match(home_id, away_id, currentDate, MatchType::LEAGUE));
      }

      // Winter break after the first half of the season
      if (round == (num_rounds / 2) - 1) {
        currentDate.nextWeek(); // Advance to the date of the next round
        currentDate.nextWeek(); // Skip a week
        currentDate.nextWeek(); // Skip another week
      } else {
        currentDate.nextWeek();
      }

      uint16_t last_id = team_ids.back();
      team_ids.pop_back();
      team_ids.insert(team_ids.begin() + 1, last_id);
    }
  }
}

void Calendar::generateFriendlies(const GameDateValue &startDate,
                                  size_t numFriendlies) {
  auto teams = GameData::instance().getTeamsVector();
  std::vector<uint16_t> team_ids;
  team_ids.reserve(teams.size());

  for (const auto &team_ref : teams) {
    team_ids.push_back(team_ref.get().getId());
  }

  size_t num_teams = team_ids.size();
  if (num_teams < 2) {
    return;
  }

  bool isOdd = (num_teams % 2 != 0);
  if (isOdd) {
    team_ids.push_back(
        FREE_AGENTS_TEAM_ID); // Assuming FREE_AGENTS_TEAM_ID is available
    num_teams++;
  }

  GameDateValue currentDate = startDate;
  currentDate.nextDay(); // Start friendlies the day after season start

  for (size_t round = 0; round < numFriendlies; ++round) {
    currentDate.nextWeek(); // Schedule friendlies weekly for a few weeks.
    if (currentDate < START_SEASON_DATE) {
      for (size_t i = 0; i < num_teams / 2; ++i) {
        uint16_t home_id = team_ids[i];
        uint16_t away_id = team_ids[num_teams - 1 - i];

        if (home_id == FREE_AGENTS_TEAM_ID || away_id == FREE_AGENTS_TEAM_ID) {
          continue;
        }

        addMatch(Match(home_id, away_id, currentDate, MatchType::FRIENDLY));
      }
    }

    // Rotate teams
    uint16_t last_id = team_ids.back();
    team_ids.pop_back();
    team_ids.insert(team_ids.begin() + 1, last_id);
  }
}
