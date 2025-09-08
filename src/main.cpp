// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include <iostream>
#include <memory>
#include "controller/game_controller.h"
#include "view/gui/gui_view.h"
#include "database/database.h"
#include "logger.h"

int main() {
  Logger::init();
  srand(time(0));
  try {
    auto db = std::make_shared<Database>("football_management.db");
    db->initialize();
    auto game = std::make_unique<Game>(db);
    auto controller = std::make_unique<GameController>(std::move(game));

    GUIView view(*controller);
    view.run();
    controller->saveGame();
    db->close();
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
  return 0;
}
