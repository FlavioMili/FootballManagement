#pragma once

#include "player.h"
#include <string>
#include <vector>

class Team {
public:
   Team(std::string name = "testTeam");
   ~Team();
   void generateTeam();
   std::string getName() const;
   int getBalance() const;
   void setBalance(int balance);
   std::vector<Player> getPlayers() const;
   void setPlayers(const std::vector<Player>& players);

private:
   std::string name;
   std::vector<Player> players;
   int balance;
};
