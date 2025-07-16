#include "player.h"

Player::Player(std::string name, int number) {
   this->stats = randomizeStats();
   this->name = name;
   this->number = number;
}

Player::~Player() = default;

std::map<Stats, int> Player::randomizeStats() {
   std::map<Stats, int> generated_stats;
   static std::default_random_engine generator(std::random_device{}());
   std::normal_distribution<double> distribution(70, 8); // mean 70, stddev 8

   for (const auto& stat_type : all_stats) {
      int value = static_cast<int>(distribution(generator));
      generated_stats[stat_type] = std::clamp(value, 1, 99);
   }

   return generated_stats;
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

int Player::getStats(Stats stat) const {
   return stats.at(stat);
}
