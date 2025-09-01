// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "player.h"
#include "../../global.h"
#include <map>
#include <string>
#include <utility>
#include <random>

Player::Player(int id, std::string name, int age, std::string role)
: id(id), name(std::move(name)), role(std::move(role)), age(age) {}

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

double Player::getOverall(const StatsConfig& stats_config) const {
    double overall = 0.0;
    const auto& role_config = stats_config.role_focus.at(role);
    const auto& weights = role_config.weights;
    const auto& stat_names = role_config.stats;

    for (size_t i = 0; i < stat_names.size(); ++i) {
        const std::string& stat_name = stat_names[i];
        if (stats.contains(stat_name)) {
            overall += stats.at(stat_name) * weights[i];
        }
    }
    return overall;
}

const std::map<std::string, float>& Player::getStats() const {
  return stats;
}

void Player::setStats(const std::map<std::string, float>& new_stats) {
  stats = new_stats;
}

void Player::agePlayer() {
  age++;

  for (auto& stat : stats) {
    if (age < PLAYER_AGE_FACTOR_DECLINE_AGE) continue;

    float age_factor = 1.0f - (age - PLAYER_AGE_FACTOR_DECLINE_AGE + 1) * PLAYER_AGE_FACTOR_DECAY_RATE;

    float decay = PLAYER_STAT_INCREASE_BASE * (1.0f - std::max(0.0f, age_factor));
    stat.second -= decay;

    if (stat.second < MIN_STAT_VAL)
      stat.second = MIN_STAT_VAL;
  }
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

void Player::train(const std::vector<std::string>& focus_stats) {
  if (focus_stats.empty()) return;

  static std::random_device rd;
  static std::mt19937 gen(rd());
  std::uniform_int_distribution<> stat_dis(0, focus_stats.size() - 1);
  std::uniform_real_distribution<float> rand_dist(0.0f, 1.0f);

  const std::string& random_stat = focus_stats[stat_dis(gen)];
  auto it = stats.find(random_stat);
  if (it == stats.end()) return;

  float age_factor = ((PLAYER_AGE_FACTOR_DECLINE_AGE - age) * PLAYER_AGE_FACTOR_DECAY_RATE);
  float random_factor = rand_dist(gen);

  float increment = PLAYER_STAT_INCREASE_BASE * (random_factor * age_factor);
  it->second += increment;

  if (it->second > MAX_STAT_VAL)
    it->second = MAX_STAT_VAL;
}
