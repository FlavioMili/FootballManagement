// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include <filesystem>
#include <iostream>
#include <memory>

#include "controller/game_controller.h"
#include "database/gamedata.h"
#include "global/logger.h"
#include "global/paths.h"
#include "gui/gui_view.h"

int main()
{
  Logger::init();
  srand(static_cast<uint>(time(0)));
  try
  {
    std::filesystem::remove(DATABASE_PATH);
    auto gamedata = std::make_shared<GameData>();
    auto game = std::make_unique<Game>(gamedata);
    auto controller =
        std::make_unique<GameController>(std::move(game), gamedata);

    GUIView view(*controller);
    view.run();
    controller->saveGame();
  }
  catch (const std::exception& e)
  {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
  return 0;
}
