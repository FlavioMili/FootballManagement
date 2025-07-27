#include <iostream>
#include "game_controller.h"
#include "cli_view.h"
#include "database.h"

int main() {
  srand(time(0));
  try {
    Database db("football_management.db");
    db.initialize();
    Game game(db);
    GameController controller(game);
    CliView view(controller);
    view.run();
    controller.saveGame();
    db.close();
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
  return 0;
}
