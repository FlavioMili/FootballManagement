#include "player.h"

Player::Player(std::string name) {
   stats = randomizeStats();
   this->name = name;
}

Player::~Player() = default;

int Player::randomizeStats() {
   std::default_random_engine generator;
   std::normal_distribution<double> distribution(50, 3); 
   return distribution(generator);
}

int Player::getStats() {
   return stats;
}
