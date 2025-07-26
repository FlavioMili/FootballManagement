#include "league.h"
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
}
