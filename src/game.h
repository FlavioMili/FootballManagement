#pragma once

#include "database.h"
#include "league.h"
#include <vector>
#include <string>

class Game {
public:
    Game();
    void run();

private:
    // The Game class will eventually manage the game state,
    // interacting with the database.
    // For now, it's simple to allow the project to compile.
};