#include "team.h"
#include "player.h"

Team::Team(std::string name) {
   this->name = name;
   this->balance = 1000000;
   generateTeam();
}

Team::~Team() = default;

void Team::generateTeam() {
   for (int i = 0; i < 11; ++i)
      players.emplace_back();
}

std::string Team::getName() const {
   return name;
}

int Team::getBalance() const {
   return balance;
}

void Team::setBalance(int balance) {
   this->balance = balance;
}

std::vector<Player> Team::getPlayers() const {
   return players;
}

void Team::setPlayers(const std::vector<Player>& players) {
   this->players = players;
}
