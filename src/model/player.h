// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <string>
#include <map>
#include <vector>
#include "stats_config.h"

/**
  * TODO add status (enum PlayerStatus) a vector and/or a bitset
  * just so that we can check if a player is on transfer list,
  * or injured, or had a red card, and so on.
  * This way we can manage more things easily later on I guess.
  * Also these for modding reasons might come from a modifiable json?
*/

class Player {
 public:
  Player(int id, std::string name, int age, std::string role);

  int getId() const;
  std::string getName() const;
  int getAge() const;
  void setAge(int age);
  std::string getRole() const;
  double getOverall(const StatsConfig& stats_config) const;
  const std::map<std::string, float>& getStats() const;

  void setStats(const std::map<std::string, float>& stats);
  void agePlayer();
  bool checkRetirement() const;
  void train(const std::vector<std::string>& focus_stats);

 private:
  int id;
  std::map<std::string, float> stats;
  std::string name;
  std::string role;
  int age;
};

