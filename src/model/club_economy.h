// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "global/types.h"
#include "model/world_tuning.h"

class GameData;
struct ClubProfile;

/**
 * @struct LeagueEconomy
 * @brief Aggregates needed to split a league's money between its clubs.
 */
struct LeagueEconomy
{
  const LeagueProfile* profile = &DEFAULT_LEAGUE_PROFILE;
  float mean_reputation = 50.0f;
  double revenue_normalizer = 1.0; /*!< Mean revenue index of the league. */
  std::size_t clubs = 0;
};

/** Builds the economy of one league from its clubs' reputations. */
LeagueEconomy makeLeagueEconomy(LeagueID league_id,
                                const std::vector<std::uint8_t>& reputations);

/** Builds the economy of every league that has clubs. */
std::unordered_map<LeagueID, LeagueEconomy> buildLeagueEconomies(
    const GameData& gamedata);

/**
 * @namespace ClubEconomy
 * @brief Revenue, cost and attendance model of a club.
 *
 * Revenue is split into TV, continental prizes, gate and commercial income
 * with the league's ECFIL-style mix. Richer, more reputable clubs earn more
 * commercial and gate income while TV money is shared mostly equally.
 */
namespace ClubEconomy
{
/** Expected revenue per season of a club with @p reputation. */
double expectedRevenue(const LeagueEconomy& economy, std::uint8_t reputation);

/** Share of capacity filled at neutral form and the reference price. */
double baseDemand(const LeagueEconomy& economy, std::uint8_t reputation);

/** Reference ticket yield that meets the league's gate-share target. */
double fairTicketPrice(const LeagueEconomy& economy,
                       const ClubProfile& profile);

/**
 * Attendance of a home match:
 * min(capacity, capacity * demand * exp(a * success - e * ln(price / ref))).
 * @param success League position and form signal in [-1, 1].
 */
std::uint32_t attendance(const LeagueEconomy& economy,
                         const ClubProfile& profile, double success,
                         std::uint8_t opponent_reputation, MatchType type);

/** Monthly equal share of the league's TV money. */
double monthlyBroadcasting(const LeagueEconomy& economy);

/** Monthly commercial income. */
double monthlySponsorship(const LeagueEconomy& economy,
                          std::uint8_t reputation);

/** Monthly non-player wages. */
double monthlyStaffCosts(const LeagueEconomy& economy, std::uint8_t reputation);

/** Monthly stadium, training ground and operating costs. */
double monthlyFacilityCosts(const LeagueEconomy& economy,
                            const ClubProfile& profile);

/** Share of revenue a club spends on player wages. */
double playerWageShare(const LeagueEconomy& economy);

/** Season-end merit and continental money by final position (index 0 =
 * champion). */
std::vector<std::int64_t> prizeMoney(const LeagueEconomy& economy,
                                     std::size_t league_size);

/** Board's transfer allowance for a new season. */
std::int64_t seasonTransferBudget(std::int64_t balance, double revenue);

/** Board's weekly wage allowance for a new season. */
std::int64_t seasonWageBudget(const LeagueEconomy& economy, double revenue,
                              std::int64_t weekly_payroll);

/**
 * Relative wage demand of a player: (overall / 65)^gamma times an age
 * factor. Multiply by a club's scale to obtain a weekly wage.
 */
double wageIndex(double overall, int age);
}  // namespace ClubEconomy
