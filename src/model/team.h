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
#include "strategy.h"
#include "finances.h"

// TODO change balance-related things with a Finances class
// TODO add stadium
// TODO add lineup and in controller lineupManager class
class Team {
 public:
  Team(int id, int league_id, std::string name, int64_t balance);

  int getId() const;
  int getLeagueId() const;
  std::string getName() const;
  const std::vector<Player>& getPlayers() const;
  std::vector<Player>& getPlayers();

  void setPlayers(const std::vector<Player>& players);

  // Lineup access
  Lineup& getLineup();
  const Lineup& getLineup() const;

  // Strategy access
  Strategy& getStrategy();
  const Strategy& getStrategy() const;
  void setStrategy(const Strategy& strategy);

  // Generate best starting XI automatically
  void generateStartingXI(const StatsConfig& stats_config);
  
  Finances& getFinances() noexcept;
  const Finances& getFinances() const noexcept;


 private:
  int id;
  int league_id;
  std::string name;
  std::vector<Player> players;
  Lineup lineup;
  Strategy team_strategy;
  Finances finances;
};
