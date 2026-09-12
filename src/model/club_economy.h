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
  /** Club reputations, highest first (gives a club's expected finish). */
  std::vector<std::uint8_t> reputations_desc;
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
/**
 * Revenue potential per season of a club with @p reputation: the league's
 * average revenue scaled by reputation. Gate and commercial income are
 * shares of it.
 */
double expectedRevenue(const LeagueEconomy& economy, std::uint8_t reputation);

/** Expected league finish (0 = champion) of a club with @p reputation. */
std::size_t expectedPosition(const LeagueEconomy& economy,
                             std::uint8_t reputation);

/**
 * Income the club can budget on per season: equal TV share, merit money at
 * its expected finish, gate and commercial income, plus half of the
 * continental money of its expected place (boards do not spend prize money
 * they may miss). Every cost line and budget is a share of it, so a club
 * that finishes where expected roughly breaks even; beating expectations
 * or reaching continental places makes a profit.
 */
double expectedIncome(const LeagueEconomy& economy, std::uint8_t reputation);

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

/** Share of expected income a club spends on player wages. */
double playerWageShare(const LeagueEconomy& economy);

/**
 * Season-end merit (broadcasting) money by final position (index 0 =
 * champion). Continental prize money is paid by the continental
 * competitions themselves, as the club earns it.
 */
std::vector<std::int64_t> prizeMoney(const LeagueEconomy& economy,
                                     std::size_t league_size);

/** Board's transfer allowance for a new season. */
std::int64_t seasonTransferBudget(std::int64_t balance, double revenue);

/**
 * Transfer money a club can commit today: the board's allowance, but never
 * more than the cash left after a reserve of Finance::CASH_RESERVE_WEEKS of
 * payroll and the instalments already owed; never negative.
 */
std::int64_t availableTransferBudget(std::int64_t board_budget,
                                     std::int64_t balance,
                                     std::int64_t weekly_payroll,
                                     std::int64_t committed);

/**
 * Owner money that rescues a club whose cash has fallen more than
 * Finance::OWNER_RESCUE_TRIGGER_WEEKS of @p weekly_payroll into the red:
 * enough to restore a cushion of OWNER_RESCUE_CUSHION_WEEKS; 0 otherwise.
 */
std::int64_t ownerRescue(std::int64_t balance, std::int64_t weekly_payroll);

/** Board's weekly wage allowance for a new season. */
std::int64_t seasonWageBudget(const LeagueEconomy& economy, double revenue,
                              std::int64_t weekly_payroll);

/**
 * Relative wage demand of a player: (overall / 65)^gamma times an age
 * factor. Multiply by a club's scale to obtain a weekly wage.
 */
double wageIndex(double overall, int age);
}  // namespace ClubEconomy
