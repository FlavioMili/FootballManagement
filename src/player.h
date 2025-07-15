#pragma once

#include <cstdint>
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

   int getStats() const;

private:
   std::string name;
   int number;
   int stats;
   int randomizeStats();
};
