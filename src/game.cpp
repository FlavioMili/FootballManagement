#include "game.h"

Game::Game() {
   leagues.emplace_back();
}

Game::~Game() = default;


void Game::run() {
   std::cout << "Starting Game::run()\n";

   for (size_t lIndex = 0; lIndex < leagues.size(); ++lIndex) {
      std::cout << "Processing League #" << lIndex + 1 << "\n";

      std::vector<Team> teams = leagues[lIndex].getTeams();
      std::cout << "Number of teams in this league: " << teams.size() << "\n";

      for (size_t i = 0; i < teams.size(); ++i) {
         std::cout << "Team " << i << ": " << teams[i].getName() << " (Stats: " << teams[i].getStats() << ")\n";
      }

      // magic number just to test a  match
      Match::simulateMatch(teams[0], teams[1]);

      std::cout << "Finished processing League #" << lIndex + 1 << "\n\n";
   }

   std::cout << "Finished Game::run()\n";
}
