#include "controller/game_controller.h"
#include "view/cli/cli_view.h"
#include "model/database.h"
#include <iostream>

int main() {
    srand(time(0));
    try {
        Database db("football_management.db");
        db.initialize();
        Game game(db);
        GameController controller(game);
        CliView view(controller);
        view.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}