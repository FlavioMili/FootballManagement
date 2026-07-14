// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "finances.h"
#include "global/types.h"
#include "lineup.h"
#include "strategy.h"

// TODO change balance-related things with a Finances class
// TODO add stadium
// TODO add lineup and in controller lineupManager class

/**
 * @class Team
 * @brief Represents a football team.
 */
class Team
{
 public:
  /**
   * @brief Constructs a Team object.
   * @param team_id The unique ID for the team.
   * @param team_league_id The ID of the league the team belongs to.
   * @param team_name The name of the team.
   * @param initial_balance The initial financial balance.
   * @param initial_player_ids Optional list of initial player IDs.
   * @param strategy Optional strategy for the team.
   * @param lineup_data Optional lineup data for the team.
   */
  Team(TeamID team_id, uint8_t team_league_id, std::string_view team_name,
       int64_t initial_balance,
       const std::vector<PlayerID>& initial_player_ids = {},
       const Strategy& strategy = Strategy{},
       const Lineup& lineup_data = Lineup{});

  // Accessors

  /** @brief Gets the team's ID. */
  TeamID getId() const;

  /** @brief Gets the ID of the team's league. */
  LeagueID getLeagueId() const;

  /** @brief Gets the name of the team. */
  const std::string getName() const;

  /** @brief Gets the list of player IDs in the team. */
  const std::vector<PlayerID>& getPlayerIDs() const;

  // Manage roster

  /**
   * @brief Adds a player to the team.
   * @param id The player's ID.
   */
  void addPlayerID(PlayerID id);

  /**
   * @brief Removes a player from the team.
   * @param id The player's ID.
   * @return True if successful, false otherwise.
   */
  bool removePlayerID(PlayerID id);

  // Lineup access

  /** @brief Gets the team's lineup (non-const). */
  Lineup& getLineup();

  /** @brief Gets the team's lineup (const). */
  const Lineup& getLineup() const;

  // Strategy access

  /** @brief Gets the team's strategy (non-const). */
  Strategy& getStrategy();

  /** @brief Gets the team's strategy (const). */
  const Strategy& getStrategy() const;

  /** @brief Sets the team's strategy. */
  void setStrategy(const Strategy& strategy);

  /**
   * @brief Generates the best starting XI automatically based on stats.
   * @param stats_config The configuration used to evaluate players.
   */
  void generateStartingXI(const class GameData& gamedata,
                          const StatsConfig& stats_config);

  // Finances access

  /** @brief Gets the team's finances (non-const). */
  Finances& getFinances() noexcept;

  /** @brief Gets the team's finances (const). */
  const Finances& getFinances() const noexcept;

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
