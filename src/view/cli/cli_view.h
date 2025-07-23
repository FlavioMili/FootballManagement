#pragma once

#include "game_controller.h"
#include <string>
#include <vector>

class CliView {
public:
    CliView(GameController& controller);
    void run();

private:
    void displayMenu();
    void processInput();
    void viewRoster();
    void viewLeaderboard(int league_id);

    GameController& controller;
};