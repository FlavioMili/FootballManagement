#include "player.h"

Player::Player(int id, std::string name, int age, std::string role)
: id(id), name(std::move(name)), role(std::move(role)), overall(0), age(age) {}

int Player::getId() const {
   return id;
}

std::string Player::getName() const {
   return name;
}

int Player::getAge() const {
   return age;
}

std::string Player::getRole() const {
   return role;
}

int Player::getOverall() const {
   return overall;
}

const std::map<std::string, int>& Player::getStats() const {
   return stats;
}

void Player::setStats(const std::map<std::string, int>& new_stats) {
   stats = new_stats;
}

void Player::calculateOverall(const std::map<std::string, double>& weights) {
   double total_value = 0;

   for (const auto& stat : stats) {
      auto it = weights.find(stat.first);
      if (it != weights.end()) {
         total_value += stat.second * it->second;
      }
   }
   overall = total_value/100;
}
