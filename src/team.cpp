#include "team.h"
#include "player.h"

Team::Team(std::string name) {
   this->name = name;
   generateTeam();
}

Team::~Team() = default;

void Team::generateTeam() {
   for (int i = 0; i < 11; ++i)
      players.emplace_back();
}

std::string Team::getName() const {
   return name;
}

int Team::getStats() const {
   int res {};
   for (auto& p : players) 
      res += p.getStats();
   return res;
}
