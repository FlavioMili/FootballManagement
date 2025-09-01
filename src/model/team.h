// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <vector>
#include <string>
#include "lineup.h"
#include "player.h"

// TODO change balance-related things with a Finances class
// TODO add stadium
// TODO add lineup and in controller lineupManager class
class Team {
 public:
  Team(int id, int league_id, std::string name, int balance = 0);

  int getId() const;
  int getLeagueId() const;
  std::string getName() const;
  int getBalance() const;
  const std::vector<Player>& getPlayers() const;
  std::vector<Player>& getPlayers();

  void setBalance(int balance);
  void addBalance(int to_add);
  void subtractBalance(int to_subtract);
  void setPlayers(const std::vector<Player>& players);

  // Lineup access
  Lineup& getLineup();
  const Lineup& getLineup() const;

  // Generate best starting XI automatically
  void generateStartingXI(const StatsConfig& stats_config);

 private:
  int id;
  int league_id;
  std::string name;
  int balance;
  std::vector<Player> players;
  Lineup lineup;
};
