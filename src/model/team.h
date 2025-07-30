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
  const std::vector<Player>& getPlayers() const;
  std::vector<Player>& getPlayers();

  void setBalance(uint64_t balance);
  void addBalance(uint64_t to_add);
  void subtractBalance(uint64_t to_subtract);
  void setPlayers(const std::vector<Player>& players);

 private:
  int id;
  int league_id;
  std::string name;
  uint64_t balance;
  std::vector<Player> players;
};
