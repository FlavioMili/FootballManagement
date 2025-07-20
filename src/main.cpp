#include "database.h"
#include "datagenerator.h"
#include "league.h"
#include <iostream>
#include <filesystem>

const std::string DB_FILE = "football_management.db";

int main() {
    bool db_exists = std::filesystem::exists(DB_FILE);
    Database db(DB_FILE);

    if (!db_exists || !db.isInitialized()) {
        std::cout << "First run detected. Generating game data..." << std::endl;
        db.initialize();
        DataGenerator::generate(db);
        std::cout << "Data generation complete." << std::endl;
    } else {
        std::cout << "Welcome back!" << std::endl;
    }

    std::cout << "\nLoading league with ID 1..." << std::endl;
    League league = db.loadLeague(1);
    std::cout << "Loaded league: " << league.getName() << std::endl;
    std::cout << "Number of teams: " << league.getTeams().size() << std::endl;

    if (!league.getTeams().empty()) {
        const Team& first_team = league.getTeams().front();
        std::cout << "First team: " << first_team.getName() << " (Balance: $" << first_team.getBalance() << ")" << std::endl;
        if(!first_team.getPlayers().empty()){
            const Player& first_player = first_team.getPlayers().front();
            std::cout << "  - Star Player: " << first_player.getName() 
                      << " (Age: " << first_player.getAge() 
                      << ", Value: $" << first_player.calculateMarketValue() << ")" << std::endl;
        }
    }

    league.simulateSeason();

    return 0;
}
