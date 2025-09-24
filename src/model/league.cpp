// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "league.h"
#include <algorithm>
#include <string_view>
#include <utility>
#include <vector>

League::League(size_t league_id, std::string_view league_name,
               const std::vector<size_t> &initial_team_ids)
    : id(league_id), name(league_name), team_ids(initial_team_ids) {
  for (auto tid : team_ids) {
    leaderboard[tid] = 0;
  }
}

size_t League::getId() const { return id; }

std::string_view League::getName() const { return name; }

const std::vector<size_t> &League::getTeamIDs() const { return team_ids; }

void League::addTeamID(size_t team_id) {
  if (std::find(team_ids.begin(), team_ids.end(), team_id) == team_ids.end()) {
    team_ids.push_back(team_id);
    leaderboard[team_id] = 0;
  }
}

void League::removeTeamID(size_t team_id) {
  auto it = std::remove(team_ids.begin(), team_ids.end(), team_id);
  if (it != team_ids.end()) {
    team_ids.erase(it, team_ids.end());
    leaderboard.erase(team_id);
  }
}

// Points management
uint8_t League::getPoints(size_t team_id) const {
  auto it = leaderboard.find(team_id);
  return it != leaderboard.end() ? it->second : 0;
}

void League::addPoints(size_t team_id, uint8_t points) {
  leaderboard[team_id] += points;
}

void League::setPoints(size_t team_id, uint8_t points) {
  leaderboard[team_id] = points;
}

void League::resetPoints() {
  for (auto &[_, pts] : leaderboard) {
    pts = 0;
  }
}

const std::map<size_t, uint8_t> &League::getLeaderboard() const {
  return leaderboard;
}
