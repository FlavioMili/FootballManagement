#pragma once

#include "player.h"

#include <string>
#include <vector>
class Team {
public:
   Team(std::string name = "testTeam");
   ~Team();
   void generateTeam();
   // void addPlayer(Player player);
   // void removePlayer(Player player);
   // void addBalance();
   // void removeBalance();
   std::string getName();
   int getStats();

private:
   std::string name;
   std::vector<Player> players;
   int balance;
};
