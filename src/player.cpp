#include "player.h"

Player::Player(std::string name, int number) {
   stats = randomizeStats();
   this->name = name;
   this->number = number;
}

Player::~Player() = default;

int Player::randomizeStats() {
    static std::default_random_engine generator(std::random_device{}());
    std::normal_distribution<double> distribution(70, 8); // mean 70, stddev 8

    int value = static_cast<int>(distribution(generator));
    return std::clamp(value, 1, 99);
}

void Player::setName(std::string name) {
   this->name = name;
}

std::string Player::getName() const {
   return name;
}

void Player::setNumber(int number) {
   this->number = number;
}

int Player::getNumber() const {
   return number;
}

int Player::getStats() const {
   return stats;
}
