#pragma once

#include <cstdint>
#include <string>
#include <random>

class Player {
public:
   Player(std::string name = "testPlayer");
   ~Player();
   // void setName();
   // void getName();
   int getStats();

private:
   std::string name;
   int stats;
   int randomizeStats();
};
