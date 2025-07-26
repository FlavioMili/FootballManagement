#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include "player.h"

class Team {
 public:
  Team(int id, int league_id, std::string name, uint64_t balance = 0);

  int getId() const;
  int getLeagueId() const;
  std::string getName() const;
  uint64_t getBalance() const;
  int getPoints() const;
  const std::vector<Player>& getPlayers() const;

  void setBalance(uint64_t balance);
  void addBalance(uint64_t to_add);
  void subtractBalance(uint64_t to_subtract);
  void setPlayers(const std::vector<Player>& players);
  void addPoints(int points);
  void resetPoints();

 private:
  int id;
  int league_id;
  std::string name;
  uint64_t balance;
  int points;
  std::vector<Player> players;
};
