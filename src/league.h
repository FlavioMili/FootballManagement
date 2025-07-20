#pragma once

#include "team.h"
#include <vector>
#include <string>

class League {
public:
   League(std::string name = "My League");
   ~League();

   std::string getName() const;
   std::vector<Team> getTeams() const;
   void overrideTeams(const std::vector<Team>& newTeams);

private:
   std::string name;
   std::vector<Team> teams;

};

