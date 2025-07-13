#pragma once

#include "team.h"
#include <vector>

class League {
public:
   League();
   ~League();

   // in future it will implement a calendar
   // void initCalendar();

   // void addTeam(Team team);
   // void removeTeam(Team team);
   std::vector<Team> getTeams();

private:
   std::vector<Team> teams;
   
   
};

