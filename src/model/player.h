#pragma once

#include <string>
#include <map>
#include <vector>
#include "stats_config.h"

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

