#include "player.h"

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