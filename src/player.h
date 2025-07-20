#pragma once

#include <string>
#include <map>
#include <vector>

class Player {
public:
   Player(std::string name = "testPlayer", int age = 18);
   ~Player();

   void setName(std::string name);
   std::string getName() const;

   void setAge(int age);
   int getAge() const;

   void setStats(const std::map<std::string, int>& stats);
   const std::map<std::string, int>& getStats() const;
   int getStat(const std::string& stat_name) const;
   void setStat(const std::string& stat_name, int value);

   // Overall rating can be calculated on the fly
   // based on a specific configuration (e.g. from stats_config.json)
   // double calculateOverallRating(const std::vector<std::string>& focus_stats) const;

private:
   std::string name;
   int age;
   std::map<std::string, int> stats;
};
