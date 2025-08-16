#pragma once

#include "game_controller.h"

class CliView {
 public:
  CliView(GameController& controller);
  void run();

 private:
  void displayMenu();
  bool processInput();
  void viewRoster();
  void viewLeaderboard(int league_id);
  void chooseManagedTeamMenu();

  GameController& controller;
};
