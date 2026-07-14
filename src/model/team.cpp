// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "team.h"

#include <algorithm>
#include <cstdint>
#include <string_view>
#include <vector>

#include "finances.h"

// Constructor
Team::Team(TeamID team_id, uint8_t team_league_id, std::string_view team_name,
           int64_t initial_balance,
           const std::vector<PlayerID>& initial_player_ids,
           const Strategy& strategy, const Lineup& lineup_data)
    : id(team_id),
      league_id(team_league_id),
      name(team_name),
      player_ids(initial_player_ids),
      team_strategy(strategy),
      lineup(lineup_data),
      finances(initial_balance, *this)
{
}

// Accessors
uint16_t Team::getId() const { return id; }
uint8_t Team::getLeagueId() const { return league_id; }
const std::string Team::getName() const { return name; }

const std::vector<PlayerID>& Team::getPlayerIDs() const { return player_ids; }

void Team::addPlayerID(PlayerID player_id) { player_ids.push_back(player_id); }

bool Team::removePlayerID(PlayerID player_id)
{
  if (std::erase(player_ids, player_id) > 0)
  {
    return true;
  }
  return false;
}

// Lineup access
Lineup& Team::getLineup() { return lineup; }
const Lineup& Team::getLineup() const { return lineup; }

// Strategy access
Strategy& Team::getStrategy() { return team_strategy; }
const Strategy& Team::getStrategy() const { return team_strategy; }
void Team::setStrategy(const Strategy& strategy) { team_strategy = strategy; }

// Generate best starting XI automatically
void Team::generateStartingXI(const StatsConfig& stats_config)
{
  lineup.generateStartingXI(player_ids, stats_config);
}

// Finances access
Finances& Team::getFinances() noexcept { return finances; }
const Finances& Team::getFinances() const noexcept { return finances; }