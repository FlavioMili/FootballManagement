#include "league.h"

League::League(std::string name) {
   this->name = name;
   teams.emplace_back("team 1 ");
   teams.emplace_back("team 2 ");
}

League::~League() = default;

std::string League::getName() const {
   return name;
}

std::vector<Team> League::getTeams() const {
   return teams;
}

void League::overrideTeams(const std::vector<Team>& newTeams) {
   teams = newTeams;
}
