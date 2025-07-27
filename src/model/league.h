#pragma once

#include <string>
#include <vector>
#include <map>
#include "team.h"

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
