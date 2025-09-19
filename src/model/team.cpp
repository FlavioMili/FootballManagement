// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "team.h"
#include "finances.h"
#include <cstdint>
#include <utility>
#include <vector>
#include <string>

Team::Team(int team_id, int team_league_id, std::string team_name, std::int64_t balance) : 
  id(team_id), 
  league_id(team_league_id),
  name(std::move(team_name)), 
  team_strategy(),
  finances(balance)
{}

int Team::getId() const { return id; }
int Team::getLeagueId() const { return league_id; }
std::string Team::getName() const { return name; }
const std::vector<Player>& Team::getPlayers() const { return players; }
std::vector<Player>& Team::getPlayers() { return players; }

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

Finances& Team::getFinances() noexcept {
  return finances;
}
const Finances& Team::getFinances() const noexcept {
  return finances;
}
