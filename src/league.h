#pragma once

#include "team.h"
#include "calendar.h"
#include <vector>
#include <string>
#include <map>

class League {
public:
   League(std::string name = "My League");
   ~League();

   void setName(std::string name);
   std::string getName() const;
   const std::vector<Team>& getTeams() const;
   void overrideTeams(const std::vector<Team>& newTeams);

   void simulateSeason();
   void printLeaderboard() const;

private:
   std::string name;
   std::vector<Team> teams;
   Calendar calendar;
   std::map<std::string, int> leaderboard; // Team name -> points
};

