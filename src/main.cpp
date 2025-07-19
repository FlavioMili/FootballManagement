#include "game.h"
#include <iostream>

int main() {
    Game game;

    // Run game logic once (creates leagues and teams)
    game.run();

    // Save game state to file "game_state.json" in project root
    std::cout << "Game saved to game_state.json\n";

    // Create a new game object and load saved state
    Game loadedGame;
    loadedGame.load("game_state.json");
    std::cout << "Game loaded from game_state.json\n";

    // Print all player names from loaded game
    const auto& leagues = loadedGame.getLeagues(); // You need to add getLeagues() const to Game

    for (size_t li = 0; li < leagues.size(); ++li) {
        std::cout << "League " << li + 1 << ":\n";
        const auto& teams = leagues[li].getTeams();
        for (size_t ti = 0; ti < teams.size(); ++ti) {
            std::cout << " Team " << ti + 1 << " (" << teams[ti].getName() << "):\n";

            // Assuming Team class has a getPlayers() method returning vector<Player>
            const auto& players = teams[ti].getPlayers();
            for (const auto& p : players) {
                std::cout << "  - " << p.getName() << "\n";
            }
        }
    }

   game.save("game_state.json");
    return 0;
}
