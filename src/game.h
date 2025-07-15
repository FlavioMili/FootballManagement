#pragma once

#include <vector>
#include <iostream>
#include "league.h"
#include "match.h"

class Game {
public:
   Game();
   ~Game();
   void run();

private:
   std::vector<League> leagues;

};
