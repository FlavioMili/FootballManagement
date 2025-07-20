#pragma once

#include "player.h"
#include <string>
#include <vector>

class Team {
public:
   Team(std::string name = "testTeam");
   ~Team();
   
   std::string getName() const;
   
   long long getBalance() const;
   void setBalance(long long balance);
   
   const std::vector<Player>& getPlayers() const;
   void setPlayers(const std::vector<Player>& players);

   double getAverageStat(const std::string& stat_name) const;

private:
   std::string name;
   std::vector<Player> players;
   long long balance;
};
