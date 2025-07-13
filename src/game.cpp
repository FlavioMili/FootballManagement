#include "game.h"

Game::Game() {
   leagues.emplace_back();
}

void Game::run() {
   for (auto& l : leagues) {
      std::vector<Team> teams = l.getTeams();
      for(size_t i = 0; i < teams.size(); ++i) {
         for (size_t j = 0; j < teams.size(); ++j) {
            if (i == j) continue;
            if (teams[i].getStats() > teams[j].getStats())
               std::cout << teams[i].getName() << "Wins\n";
            else 
               std::cout << teams[j].getName() << "Wins\n";
         }
      }

   }
}
