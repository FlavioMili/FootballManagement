#pragma once

#include <string>
#include <map>

class Player {
 public:
  Player(int id, std::string name, int age, std::string role);

  int getId() const;
  std::string getName() const;
  int getAge() const;
  void setAge(int age);
  std::string getRole() const;
  double getOverall() const;
  void setOverall(double overall);
  const std::map<std::string, int>& getStats() const;

  void setStats(const std::map<std::string, int>& stats);
  void calculateOverall(const std::map<std::string, double>& weights);
  void agePlayer(const std::map<std::string, double>& statWeights);
  bool checkRetirement() const;

 private:
  int id;
  std::map<std::string, int> stats;
  std::string name;
  std::string role;
  double overall;
  int age;
};
