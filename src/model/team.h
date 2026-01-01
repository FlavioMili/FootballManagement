// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include "finances.h"
#include "lineup.h"
#include "strategy.h"
#include "global/types.h"
#include <cstdint>
#include <string_view>
#include <vector>

// TODO change balance-related things with a Finances class
// TODO add stadium
// TODO add lineup and in controller lineupManager class
class Team {
 public:
  Team(TeamID team_id, uint8_t team_league_id, std::string_view team_name,
       int64_t initial_balance,
       const std::vector<PlayerID> &initial_player_ids = {},
       const Strategy &strategy = Strategy{},
       const Lineup &lineup_data = Lineup{});

  // Accessors
  TeamID getId() const;
  LeagueID getLeagueId() const;
  const std::string getName() const;

  const std::vector<PlayerID> &getPlayerIDs() const;

  // Manage roster
  void addPlayerID(PlayerID id);
  bool removePlayerID(PlayerID id);

  // Lineup access
  Lineup &getLineup();
  const Lineup &getLineup() const;

  // Strategy access
  Strategy &getStrategy();
  const Strategy &getStrategy() const;
  void setStrategy(const Strategy &strategy);

  // Generate best starting XI automatically
  void generateStartingXI(const StatsConfig &stats_config);

  // Finances access
  Finances &getFinances() noexcept;
  const Finances &getFinances() const noexcept;

 private:
  TeamID id;
  LeagueID league_id;
  std::string name;

  /*
   * the main idea is to have a players map *map[player_id]-- > player;
   * and here we just iterate on the ids to get *all players.
   */
  std::vector<PlayerID> player_ids;

  Strategy team_strategy;
  Lineup lineup;
  Finances finances;
};
