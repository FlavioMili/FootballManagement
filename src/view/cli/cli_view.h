// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

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
