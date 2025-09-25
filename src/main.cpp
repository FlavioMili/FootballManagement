// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include <iostream>
#include <memory>

#include "global/logger.h"

#include "controller/game_controller.h"
#include "gui/gui_view.h"

int main() {
  Logger::init();
  srand(static_cast<uint>(time(0)));
  try {
    auto game = std::make_unique<Game>();
    auto controller = std::make_unique<GameController>(std::move(game));

    GUIView view(*controller);
    view.run();
    controller->saveGame();
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
  return 0;
}
