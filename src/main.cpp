#include "database.h"
#include "datagenerator.h"
#include <iostream>
#include <filesystem>

const std::string DB_FILE = "football_manager.db";

int main() {
    // Check if the database file exists to determine if it's a first run.
    // This is a simple check. The db.isInitialized() is a more robust check
    // against the database content itself.
    bool db_exists = std::filesystem::exists(DB_FILE);

    Database db(DB_FILE);

    if (!db_exists || !db.isInitialized()) {
        std::cout << "First run detected or database is not initialized." << std::endl;
        // 1. Create the tables
        db.initialize();
        // 2. Populate with initial data
        DataGenerator::generate(db);
    } else {
        std::cout << "Welcome back! Loading game from " << DB_FILE << std::endl;
        // Here, you would add the logic to load game state from the database
        // and start the actual game loop.
    }

    std::cout << "\nSetup complete. You can now inspect the '" << DB_FILE << "' file." << std::endl;

    return 0;
}