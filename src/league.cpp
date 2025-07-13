#include "league.h"

League::League() {
   teams.emplace_back("team 1 ");
   teams.emplace_back("team 2 ");
}

League::~League() = default;

std::vector<Team> League::getTeams() {
   return teams;
}
