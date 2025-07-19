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
   std::string getName() const;
   int getStats() const;
   std::vector<Player> getPlayers() const;

   friend void to_json(nlohmann::json& j, const Team& t);
   friend void from_json(const nlohmann::json& j, Team& t);

private:
   std::string name;
   std::vector<Player> players;
   int balance;
};

void to_json(nlohmann::json& j, const Team& t);
void from_json(const nlohmann::json& j, Team& t);
