#pragma once

#include "team.h"
#include <string>
#include <vector>

class League {
public:
   League(int id, std::string name);

   int getId() const;
   std::string getName() const;
   const std::vector<Team>& getTeams() const;
   void setTeams(const std::vector<Team>& teams);

private:
   int id;
   std::string name;
   std::vector<Team> teams;
};
