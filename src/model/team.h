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
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

// TODO change balance-related things with a Finances class
// TODO add stadium
// TODO add lineup and in controller lineupManager class
class Team {
 public:
  Team(uint16_t team_id, uint8_t team_league_id, std::string_view team_name,
       int64_t initial_balance,
       const std::vector<uint32_t> &initial_player_ids = {},
       const Strategy &strategy = Strategy{},
       const Lineup &lineup_data = Lineup{});

  // Accessors
  uint16_t getId() const;
  uint8_t getLeagueId() const;
  const std::string getName() const;

  const std::vector<uint32_t> &getPlayerIDs() const;

  // TODO make these managed by the datapath of either retirement or
  // database itself?
  // otherwise we need to check how to make it so that the 
  // main map id to player actually has those ids
  // void addPlayerID(const size_t id);
  // void removePlayerID(const size_t id);

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
  uint16_t id;
  uint8_t league_id;
  std::string name;

  /*
   * the main idea is to have a players map *map[player_id]-- > player;
   * and here we just iterate on the ids to get *all players.
   */
  std::vector<uint32_t> player_ids;

  Strategy team_strategy;
  Lineup lineup;
  Finances finances;
};
