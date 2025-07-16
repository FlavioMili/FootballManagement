#pragma once

#include "global.h"
#include <cstdint>
#include <map>
#include <string>
#include <random>
#include <algorithm>

class Player {
public:
   Player(std::string name = "testPlayer", 
          int number = 99);
   ~Player();

   void setName(std::string name);
   std::string getName() const;

   void setNumber(int num);
   int getNumber() const;

   int getStats(Stats stat) const;

private:
   std::string name;
   int number;
   std::map<Stats, int> stats;
   std::map<Stats, int> randomizeStats();
};
