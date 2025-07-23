#include "team.h"

Team::Team(int id, int league_id, std::string name, long long balance)
: id(id), league_id(league_id), name(std::move(name)), balance(balance), points(0) {}

int Team::getId() const {
   return id;
}

int Team::getLeagueId() const {
   return league_id;
}

std::string Team::getName() const {
   return name;
}

long long Team::getBalance() const {
   return balance;
}

int Team::getPoints() const {
   return points;
}

const std::vector<Player>& Team::getPlayers() const {
   return players;
}

void Team::setBalance(long long new_balance) {
   balance = new_balance;
}

void Team::setPlayers(const std::vector<Player>& new_players) {
   players = new_players;
}

void Team::addPoints(int new_points) {
   points += new_points;
}

void Team::resetPoints() {
   points = 0;
}
