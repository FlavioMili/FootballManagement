// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "team.h"
#include <utility>
#include <vector>
#include <string>

Team::Team(int id, int league_id, std::string name, int balance)
  : id(id), league_id(league_id), name(std::move(name)), balance(balance), team_strategy() {}

int Team::getId() const { return id; }
int Team::getLeagueId() const { return league_id; }
std::string Team::getName() const { return name; }
int Team::getBalance() const { return balance; }
const std::vector<Player>& Team::getPlayers() const { return players; }
std::vector<Player>& Team::getPlayers() { return players; }

void Team::setBalance(int new_balance) { balance = new_balance; }
void Team::addBalance(int to_add) { balance += to_add; }
void Team::subtractBalance(int to_subtract) { balance -= to_subtract; }

void Team::setPlayers(const std::vector<Player>& new_players) {
  players = new_players;
  lineup.gridClear();
}

Lineup& Team::getLineup() { return lineup; }
const Lineup& Team::getLineup() const { return lineup; }

Strategy& Team::getStrategy() { return team_strategy; }
const Strategy& Team::getStrategy() const { return team_strategy; }
void Team::setStrategy(const Strategy& strategy) { team_strategy = strategy; }

// Generate best starting XI automatically
void Team::generateStartingXI(const StatsConfig& stats_config) {
  lineup.generateStartingXI(players, stats_config);
}
