// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <string>
#include <vector>
#include <map>
#include "team.h"

/**
  * League class, seems like it has pretty much 
  * all we need to make it work, maybe it could 
  * be improved with stats or something like that.
  *
  * TODO add stats after the match engine improves and
  * makes players actually perform stuff
*/
class League {
 public:
  League(int id, std::string name);

  int getId() const;
  std::string getName() const;
  const std::vector<Team>& getTeams() const;
  void setTeams(const std::vector<Team>& teams);
  
  int getPoints(int team_id) const;
  void addPoints(int team_id, int points);
  void setPoints(int team_id, int points);
  void resetPoints();
  const std::map<int, int>& getLeaderboard() const;

 private:
  int id;
  std::string name;
  std::vector<Team> teams;
  std::map<int, int> leaderboard;  // map[team_id] = points;
};
