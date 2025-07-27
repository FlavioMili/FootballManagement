#include "player.h"
#include "../../global.h"
#include <map>
#include <string>
#include <utility>
#include <random>

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

void Player::setAge(int age) {
  this->age = age;
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

void Player::agePlayer(const std::map<std::string, double>& statWeights) {
  age++;

  for (auto& stat : stats) {
    float ageFactor = 1.0f;
    if (age >= PLAYER_AGE_FACTOR_DECLINE_AGE) {
      ageFactor -= (age - PLAYER_AGE_FACTOR_DECLINE_AGE + 1) * PLAYER_AGE_FACTOR_DECAY_RATE;
    }

    float statChange = PLAYER_STAT_INCREASE_BASE * ageFactor;
    stat.second = static_cast<int>(stat.second + statChange);

    // Ensure stats stay within MIN_STAT_VAL and MAX_STAT_VAL
    if (stat.second < MIN_STAT_VAL) {
      stat.second = MIN_STAT_VAL;
    } else if (stat.second > MAX_STAT_VAL) {
      stat.second = MAX_STAT_VAL;
    }
  }
  calculateOverall(statWeights);
}

bool Player::checkRetirement() const {
  if (age < PLAYER_RETIREMENT_AGE_THRESHOLD) {
    return false;
  }

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<> dis(0.0, 1.0);

  float retirementChance = PLAYER_RETIREMENT_BASE_CHANCE +
                           (age - PLAYER_RETIREMENT_AGE_THRESHOLD) * PLAYER_RETIREMENT_CHANCE_INCREASE_PER_YEAR;

  return dis(gen) < retirementChance;
}
