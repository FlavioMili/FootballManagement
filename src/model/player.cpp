#include "player.h"
#include <map>
#include <string>
#include <utility>

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

double Player::getOverall() const {
  return overall;
}

void Player::setOverall(double overall) {
  this->overall = overall;
}

const std::map<std::string, int>& Player::getStats() const {
  return stats;
}

void Player::setStats(const std::map<std::string, int>& new_stats) {
  stats = new_stats;
}

void Player::calculateOverall(const std::map<std::string, double>& weights) {
  overall = 0;
  for (const auto& stat : stats) {
    auto it = weights.find(stat.first);
    if (it != weights.end()) {
      overall += stat.second * it->second;
    }
  }
}
