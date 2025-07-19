#pragma once

#include "team.h"
#include <vector>
#include "json.hpp"

class League {
public:
   League();
   ~League();

   // in future it will implement a calendar
   // void initCalendar();

   // void addTeam(Team team);
   // void removeTeam(Team team);
   std::vector<Team> getTeams() const;
   void overrideTeams(const std::vector<Team>& newTeams);

private:
   std::vector<Team> teams;

};

void to_json(nlohmann::json& j, const League& l);
void from_json(const nlohmann::json& j, League& l);
