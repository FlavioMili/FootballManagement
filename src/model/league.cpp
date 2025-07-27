#include "league.h"
#include <exception>
#include <utility>
#include <vector>
#include <string>

League::League(int id, std::string name) : id(id), name(std::move(name)) {}

int League::getId() const {
  return id;
}

std::string League::getName() const {
  return name;
}

const std::vector<Team>& League::getTeams() const {
  return teams;
}

void League::setTeams(const std::vector<Team>& new_teams) {
  teams = new_teams;
  resetPoints();
}

int League::getPoints(int team_id) const {
    try {
        return leaderboard.at(team_id);
    } catch (const std::exception& e) {
        return -1; 
    }
}

void League::addPoints(int team_id, int points) {
    leaderboard[team_id] += points;
}

void League::setPoints(int team_id, int points) {
    leaderboard[team_id] = points;
}

void League::resetPoints() {
    for (auto& pair : leaderboard) {
        pair.second = 0;
    }
}

const std::map<int, int>& League::getLeaderboard() const {
    return leaderboard;
}
