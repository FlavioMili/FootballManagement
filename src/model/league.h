#pragma once

#include <string>
#include <vector>
#include "team.h"

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
