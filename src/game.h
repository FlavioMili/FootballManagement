#pragma once

#include "league.h"
#include <vector>

class Game {
public:
   Game();
   ~Game();
   void run();

   std::vector<League> getLeagues() const;

   void save(const std::string& filename) const;
   void load(const std::string& filename);


private:
   std::vector<League> leagues;
};
