#include "team.h"
#include <numeric>

Team::Team(std::string name) : name(name), balance(0) {}

Team::~Team() {}

std::string Team::getName() const {
    return name;
}

long long Team::getBalance() const {
    return balance;
}

void Team::setBalance(long long new_balance) {
    this->balance = new_balance;
}

const std::vector<Player>& Team::getPlayers() const {
    return players;
}

void Team::setPlayers(const std::vector<Player>& new_players) {
    this->players = new_players;
}

double Team::getAverageStat(const std::string& stat_name) const {
    if (players.empty()) {
        return 0.0;
    }
    double total = 0.0;
    for (const auto& player : players) {
        total += player.getStat(stat_name);
    }
    return total / players.size();
}