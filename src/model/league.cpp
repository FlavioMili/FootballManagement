// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "league.h"
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

League::League(uint8_t league_id, const std::string &league_name,
               const std::vector<uint16_t> &initial_team_ids, std::optional<uint8_t> parent)
    : id(league_id), name(league_name), parent_league_id(parent) {
  for (auto tid : initial_team_ids) {
    team_ids.push_back(tid);
    leaderboard[tid] = 0;
  }
}

uint8_t League::getId() const { return id; }

std::string League::getName() const { return name; }

const std::vector<uint16_t> &League::getTeamIDs() const { return team_ids; }

void League::addTeamID(uint16_t team_id) {
  if (std::find(team_ids.begin(), team_ids.end(), team_id) == team_ids.end()) {
    team_ids.push_back(team_id);
    leaderboard[team_id] = 0;
  }
}

void League::removeTeamID(uint16_t team_id) {
  auto it = std::remove(team_ids.begin(), team_ids.end(), team_id);
  if (it != team_ids.end()) {
    team_ids.erase(it, team_ids.end());
    leaderboard.erase(team_id);
  }
}

// Points management
uint8_t League::getPoints(uint16_t team_id) const {
  auto it = leaderboard.find(team_id);
  return it != leaderboard.end() ? it->second : 0;
}

void League::addPoints(uint16_t team_id, uint8_t points) {
  leaderboard[team_id] += points;
}

void League::setPoints(uint16_t team_id, uint8_t points) {
  leaderboard[team_id] = points;
}

void League::resetPoints() {
  for (auto &[_, pts] : leaderboard) {
    pts = 0;
  }
}

const std::map<uint16_t, uint8_t> &League::getLeaderboard() const {
  return leaderboard;
}

const std::optional<uint8_t> League::getParentLeagueID() const {
    return parent_league_id;
}
