#include "player.h"
#include <numeric>
#include <algorithm>

Player::Player(std::string name, int age)
    : name(name), age(age) {}

Player::~Player() {}

void Player::setName(std::string new_name) {
    this->name = new_name;
}

std::string Player::getName() const {
    return name;
}

void Player::setAge(int new_age) {
    this->age = new_age;
}

int Player::getAge() const {
    return age;
}

void Player::setStats(const std::map<std::string, int>& new_stats) {
    this->stats = new_stats;
}

const std::map<std::string, int>& Player::getStats() const {
    return stats;
}

int Player::getStat(const std::string& stat_name) const {
    auto it = stats.find(stat_name);
    if (it != stats.end()) {
        return it->second;
    }
    return 0; // Return a default value if stat not found
}

void Player::setStat(const std::string& stat_name, int value) {
    stats[stat_name] = value;
}

long long Player::calculateMarketValue() const {
    if (stats.empty()) {
        return 0;
    }

    double total_stats = 0;
    for (const auto& pair : stats) {
        total_stats += pair.second;
    }
    double avg_stat = total_stats / stats.size();

    // Age factor: younger players are more valuable
    double age_factor = 1.0;
    if (age < 24) {
        age_factor = 1.5;
    } else if (age > 30) {
        age_factor = 0.7;
    }

    long long value = static_cast<long long>((avg_stat * avg_stat) * 100 * age_factor);
    return std::max(1000LL, value); // Minimum value of 1000
}
