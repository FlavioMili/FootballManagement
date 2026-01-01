// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "player.h"
#include "global/global.h"
#include <cstdint>
#include <map>
#include <random>
#include <string>
#include <utility>
#include <cmath>

Player::Player(PlayerID new_id, TeamID new_team_id,
               std::string_view new_first_name, std::string_view new_last_name,
               std::string_view new_role, Language new_nationality,
               uint32_t new_wage, uint32_t new_status, uint8_t new_age,
               uint8_t new_contract_years, uint8_t new_height, Foot new_foot,
               const std::map<std::string, float> &new_stats)
    : _id(new_id), _team_id(new_team_id), _wage(new_wage), _status(new_status),
      _first_name(new_first_name), _last_name(new_last_name), _role(new_role),
      _nationality(new_nationality), _age(new_age),
      _contract_years(new_contract_years), _height(new_height), _foot(new_foot),
      _stats(new_stats) {}

uint32_t Player::getId() const { return _id; }

TeamID Player::getTeamId() const { return _team_id; }

void Player::setTeamId(TeamID id) { _team_id = id; }

std::string Player::getName() const { return _first_name + " " + _last_name; }

std::string Player::getFirstName() const { return _first_name; }

std::string Player::getLastName() const { return _last_name; }

int Player::getAge() const { return _age; }

void Player::setAge(uint8_t new_age) { _age = new_age; }

std::string Player::getRole() const { return _role; }

Language Player::getNationality() const { return _nationality; }

uint32_t Player::getWage() const { return _wage; }

uint8_t Player::getContractYears() const { return _contract_years; }

uint8_t Player::getHeight() const { return _height; }

Foot Player::getFoot() const { return _foot; }

uint32_t Player::getStatus() const { return _status; }

const std::map<std::string, float> &Player::getStats() const { return _stats; }

void Player::setStats(const std::map<std::string, float> &new_stats) {
  _stats = new_stats;
}

double Player::getOverall(const StatsConfig &stats_config) const {
  double overall = 0.0;
  const auto &role_config = stats_config.role_focus.at(std::string(_role));
  const auto &weights = role_config.weights;
  const auto &stat_names = role_config.stats;

  for (size_t i = 0; i < stat_names.size(); ++i) {
    const std::string &stat_name = stat_names[i];
    auto it = _stats.find(stat_name);
    if (it != _stats.end()) {
      overall += static_cast<double>(it->second) * weights[i];
    }
  }
  return overall;
}

void Player::agePlayer() {
  ++_age;

  if (_age < PLAYER_AGE_FACTOR_DECLINE_AGE) return;

  float age_factor = 1.0f - (static_cast<float>(_age) - PLAYER_AGE_FACTOR_DECLINE_AGE + 1.0f) *
                                PLAYER_AGE_FACTOR_DECAY_RATE;

  // Ensure age_factor doesn't make decay negative (growth) unexpectedly here
  // though formula suggests it decreases.
  float decay = PLAYER_STAT_INCREASE_BASE * (1.0f - std::max(0.0f, age_factor));

  for (auto &stat : _stats) {
    stat.second -= decay;

    if (stat.second < MIN_STAT_VAL) stat.second = MIN_STAT_VAL;
  }
}

bool Player::checkRetirement() const {
  if (_age < PLAYER_RETIREMENT_AGE_THRESHOLD) return false;

  static std::random_device rd;
  static std::mt19937 gen(rd());
  std::uniform_real_distribution<float> dis(0.0f, 1.0f);

  float retirementChance = PLAYER_RETIREMENT_BASE_CHANCE +
                           (static_cast<float>(_age) - PLAYER_RETIREMENT_AGE_THRESHOLD) *
                               PLAYER_RETIREMENT_CHANCE_INCREASE_PER_YEAR;

  return dis(gen) < retirementChance;
}

void Player::train(const std::vector<std::string> &focus_stats) {
  if (focus_stats.empty())
    return;

  static std::random_device rd;
  static std::mt19937 gen(rd());
  std::uniform_int_distribution<> stat_dis(
      0, static_cast<int>(focus_stats.size() - 1));
  std::uniform_real_distribution<float> rand_dist(0.0f, 1.0f);

  const std::string &random_stat =
      focus_stats[static_cast<size_t>(stat_dis(gen))];

  auto it = _stats.find(random_stat);
  if (it == _stats.end()) return;

  float age_factor =
      ((PLAYER_AGE_FACTOR_DECLINE_AGE - static_cast<float>(_age)) * PLAYER_AGE_FACTOR_DECAY_RATE);
  float random_factor = rand_dist(gen);

  float increment = PLAYER_STAT_INCREASE_BASE * (random_factor * age_factor);
  it->second += increment;

  if (it->second > MAX_STAT_VAL) it->second = MAX_STAT_VAL;
}

// ---------------- Market Logic ----------------

uint32_t Player::getMarketValue() const {
  return _cached_market_value;
}

void Player::updateMarketValue(const StatsConfig &stats_config) {
  // Simple algorithm for market value
  // Value = (Overall^2 * 1000) * AgeFactor
  // AgeFactor: Younger players (18-25) have higher potential value. 
  // This is a placeholder algorithm.
  
  double overall = getOverall(stats_config);
  
  // Base value calculation
  // E.g. Overall 50 -> 2500 * 1000 = 2.5M
  // E.g. Overall 80 -> 6400 * 1000 = 6.4M (Need steeper curve)
  // Let's try Overall^3
  
  // Normalized overall (0-100) -> (0-1)
  // But our overall is sum of weighted stats. Weights sum to 1? 
  // Let's assume overall is roughly 0-100.
  
  double base_value = std::pow(overall, 3) * 10.0; 
  // 50^3 * 10 = 125,000 * 10 = 1,250,000
  // 80^3 * 10 = 512,000 * 10 = 5,120,000
  // 90^3 * 10 = 729,000 * 10 = 7,290,000
  
  // Age multiplier
  // Peak value at ~24-27
  double age_multiplier = 1.0;
  if (_age < 20) age_multiplier = 1.5;
  else if (_age < 24) age_multiplier = 1.3;
  else if (_age < 29) age_multiplier = 1.1;
  else if (_age < 32) age_multiplier = 0.9;
  else age_multiplier = 0.6; // Older players lose value
  
  _cached_market_value = static_cast<uint32_t>(base_value * age_multiplier);
}

void Player::setTransferStatus(TransferStatus status) {
  _transfer_status = status;
}

TransferStatus Player::getTransferStatus() const {
  return _transfer_status;
}