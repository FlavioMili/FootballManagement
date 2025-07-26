#pragma once

#include <string>
#include <map>

class Player {
 public:
  Player(int id, std::string name, int age, std::string role);

  int getId() const;
  std::string getName() const;
  int getAge() const;
  std::string getRole() const;
  int getOverall() const;
  const std::map<std::string, int>& getStats() const;

  void setStats(const std::map<std::string, int>& stats);
  void calculateOverall(const std::map<std::string, double>& weights);

 private:
  int id;
  std::map<std::string, int> stats;
  std::string name;
  std::string role;
  int overall;
  int age;
};
