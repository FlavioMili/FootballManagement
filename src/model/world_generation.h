// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "global/stats_config.h"
#include "global/types.h"
#include "model/club_economy.h"
#include "model/player.h"
#include "model/team.h"

class WorldRng;

/**
 * @struct NamePool
 * @brief First and last names from assets/user_made_data/names_files.
 */
struct NamePool
{
  std::vector<std::string> first_names;
  std::vector<std::string> last_names;

  /** Loads the name files once and returns the shared pool. */
  static const NamePool& instance();
};

/**
 * @namespace WorldGeneration
 * @brief Creation of clubs and players with realistic structure.
 *
 * Club reputations are tiered inside each league around the league's level;
 * squad quality, stadiums, revenue and payroll all follow from reputation,
 * so wage bills predict league strength. Players get role-dependent
 * attribute profiles, an age curve, hidden potential and personality.
 */
namespace WorldGeneration
{
/** Mean overall rating of a first-team regular at a club. */
float teamLevel(std::uint8_t reputation);

/** Overall rating of @p stats for @p role (same formula as Player). */
double overallFor(PlayerRole role, const std::map<std::string, float>& stats,
                  const StatsConfig& stats_config);

/**
 * Assigns reputation, facilities, stadium and ticket price to every club
 * (except free agents), league by league. With @p assign_opening_balance the
 * clubs also get opening cash scaled to their revenue (new worlds); legacy
 * saves keep their balance.
 */
void generateClubProfiles(std::unordered_map<TeamID, Team>& teams,
                          std::uint64_t world_seed,
                          bool assign_opening_balance);

/**
 * Generates the missing players of a 30-man squad. Wages are scaled so the
 * club's payroll (including @p existing_weekly_wages) matches the league's
 * player-wage share of the club's revenue.
 */
std::vector<Player> generateSquad(const Team& team,
                                  const LeagueEconomy& economy,
                                  std::size_t existing_players,
                                  std::int64_t existing_weekly_wages,
                                  PlayerID& next_player_id,
                                  std::uint64_t world_seed,
                                  const StatsConfig& stats_config);

/**
 * Generates an academy graduate aged 15-17 whose potential scales with the
 * club level and youth facilities. @p wage_scale converts the wage index to
 * euros for this club (weekly payroll / sum of wage indices).
 */
Player generateYouthPlayer(const Team& team, PlayerID player_id, WorldRng& rng,
                           const StatsConfig& stats_config, double wage_scale);

/** Draws potential and personality for a player that has none yet. */
void initializeHiddenAttributes(Player& player, WorldRng& rng,
                                const StatsConfig& stats_config);

/** Sets the opening transfer and wage budgets from cash and payroll. */
void applyOpeningBudgets(Team& team, const LeagueEconomy& economy,
                         std::int64_t weekly_payroll);
}  // namespace WorldGeneration
