#pragma once

#include "player.h"
#include <string>
#include <vector>

class Team {
public:
   Team(int id, int league_id, std::string name, long long balance = 0);

   int getId() const;
   int getLeagueId() const;
   std::string getName() const;
   long long getBalance() const;
   int getPoints() const;
   const std::vector<Player>& getPlayers() const;

   void setBalance(long long balance);
   void setPlayers(const std::vector<Player>& players);
   void addPoints(int points);
   void resetPoints();

private:
   int id;
   int league_id;
   std::string name;
   long long balance;
   int points;
   std::vector<Player> players;
};
