#pragma once

#include "global.h"
#include <string>
#include <map>

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
   friend void to_json(nlohmann::json& j, const Player& p);
   friend void from_json(const nlohmann::json& j, Player& p);

private:
   std::string name;
   int number;
   std::map<Stats, int> stats;
   std::map<Stats, int> randomizeStats();
};
