#include <iostream>
#include "controller/game_controller.h"
#include "settings_manager.h"
#include "view/gui/gui_view.h"
#include "view/gui/scenes/main_menu_scene.h"
#include "database/database.h"

int main() {
  srand(time(0));
  try {
    Database db("football_management.db");
    db.initialize();
    Game game(db);
    GameController controller(game);

    // CliView view(controller);
    GUIView view(controller);
    view.run();
    controller.saveGame();
    db.close();
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
  return 0;
}
