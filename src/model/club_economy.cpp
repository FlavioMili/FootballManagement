// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "model/club_economy.h"

#include <algorithm>
#include <cmath>
#include <map>

#include "database/gamedata.h"
#include "global/global.h"
#include "model/team.h"

namespace
{
using Finance = WorldTuning::Finance;

double revenueIndex(const LeagueEconomy& economy, std::uint8_t reputation)
{
  return std::exp(static_cast<double>(Finance::REVENUE_REPUTATION_SLOPE) *
                  (static_cast<double>(reputation) -
                   static_cast<double>(economy.mean_reputation)));
}

double operatingShare(const LeagueEconomy& economy)
{
  return std::max(static_cast<double>(Finance::MIN_OPERATING_SHARE),
                  static_cast<double>(Finance::TARGET_COST_RATIO) -
                      static_cast<double>(economy.profile->wage_ratio));
}
}  // namespace

LeagueEconomy makeLeagueEconomy(LeagueID league_id,
                                const std::vector<std::uint8_t>& reputations)
{
  LeagueEconomy economy;
  economy.profile = &leagueProfile(league_id);
  economy.clubs = reputations.size();
  if (reputations.empty()) return economy;

  double total = 0.0;
  for (const std::uint8_t reputation : reputations) total += reputation;
  economy.mean_reputation =
      static_cast<float>(total / static_cast<double>(reputations.size()));

  double index_total = 0.0;
  for (const std::uint8_t reputation : reputations)
    index_total += revenueIndex(economy, reputation);
  economy.revenue_normalizer =
      index_total / static_cast<double>(reputations.size());
  return economy;
}

std::unordered_map<LeagueID, LeagueEconomy> buildLeagueEconomies(
    const GameData& gamedata)
{
  std::map<LeagueID, std::vector<std::uint8_t>> reputations;
  for (const auto& [team_id, team] : gamedata.getTeams())
  {
    if (team_id == FREE_AGENTS_TEAM_ID) continue;
    reputations[team.getLeagueId()].push_back(team.getReputation());
  }
  std::unordered_map<LeagueID, LeagueEconomy> economies;
  economies.reserve(reputations.size());
  for (const auto& [league_id, values] : reputations)
    economies.emplace(league_id, makeLeagueEconomy(league_id, values));
  return economies;
}

namespace ClubEconomy
{
double expectedRevenue(const LeagueEconomy& economy, std::uint8_t reputation)
{
  return static_cast<double>(economy.profile->average_revenue_eur) *
         revenueIndex(economy, reputation) / economy.revenue_normalizer;
}

double baseDemand(const LeagueEconomy& economy, std::uint8_t reputation)
{
  // [P] Mid-table clubs fill ~80%, the biggest sell out.
  return std::clamp(0.80 + 0.25 *
                               (static_cast<double>(reputation) -
                                static_cast<double>(economy.mean_reputation)) /
                               20.0,
                    0.35, 1.15);
}

double fairTicketPrice(const LeagueEconomy& economy, const ClubProfile& profile)
{
  const double gate_target =
      expectedRevenue(economy, profile.reputation) *
      static_cast<double>(economy.profile->gate_share) /
      static_cast<double>(Finance::HOME_MATCHES_PER_SEASON);
  const double expected_crowd =
      static_cast<double>(
          std::max<std::uint32_t>(profile.stadium_capacity, 1)) *
      std::min(1.0, baseDemand(economy, profile.reputation));
  return std::max(1.0, gate_target / expected_crowd);
}

std::uint32_t attendance(const LeagueEconomy& economy,
                         const ClubProfile& profile, double success,
                         std::uint8_t opponent_reputation, MatchType type)
{
  const double reference = fairTicketPrice(economy, profile);
  const double price = std::max(1.0, static_cast<double>(profile.ticket_price));
  // [P] Bigger visitors draw up to 10% more, smaller ones up to 10% fewer.
  const double opponent =
      1.0 + 0.1 * std::clamp((static_cast<double>(opponent_reputation) -
                              static_cast<double>(profile.reputation)) /
                                 20.0,
                             -1.0, 1.0);
  double type_factor = 1.0;
  if (type == MatchType::FRIENDLY) type_factor = 0.3;  // [P]
  const double demand =
      baseDemand(economy, profile.reputation) *
      std::exp(static_cast<double>(Finance::ATTENDANCE_SUCCESS_WEIGHT) *
                   std::clamp(success, -1.0, 1.0) -
               static_cast<double>(Finance::TICKET_PRICE_ELASTICITY) *
                   std::log(price / reference)) *
      opponent * type_factor;
  const double capacity = static_cast<double>(profile.stadium_capacity);
  return static_cast<std::uint32_t>(
      std::lround(std::clamp(capacity * demand, 0.05 * capacity, capacity)));
}

double monthlyBroadcasting(const LeagueEconomy& economy)
{
  return static_cast<double>(economy.profile->average_revenue_eur) *
         static_cast<double>(economy.profile->tv_share) *
         static_cast<double>(Finance::TV_EQUAL_SHARE) / 12.0;
}

double monthlySponsorship(const LeagueEconomy& economy, std::uint8_t reputation)
{
  return expectedRevenue(economy, reputation) *
         static_cast<double>(economy.profile->commercial_share) / 12.0;
}

double monthlyStaffCosts(const LeagueEconomy& economy, std::uint8_t reputation)
{
  const double staff_share =
      static_cast<double>(economy.profile->wage_ratio) *
      (1.0 -
       static_cast<double>(WorldTuning::Generation::PLAYER_SHARE_OF_WAGES));
  return expectedRevenue(economy, reputation) * staff_share / 12.0;
}

double monthlyFacilityCosts(const LeagueEconomy& economy,
                            const ClubProfile& profile)
{
  // [P] Facilities above the club's stature cost more to run, and less when
  // below it (+-0.4% per point, 0.8x-1.2x).
  const double facility_gap =
      (static_cast<double>(profile.training_facilities) +
       static_cast<double>(profile.youth_facilities)) /
          2.0 -
      static_cast<double>(profile.reputation);
  const double facility_factor =
      std::clamp(1.0 + 0.004 * facility_gap, 0.8, 1.2);
  return expectedRevenue(economy, profile.reputation) *
         operatingShare(economy) * facility_factor / 12.0;
}

double playerWageShare(const LeagueEconomy& economy)
{
  return static_cast<double>(economy.profile->wage_ratio) *
         static_cast<double>(WorldTuning::Generation::PLAYER_SHARE_OF_WAGES);
}

std::vector<std::int64_t> prizeMoney(const LeagueEconomy& economy,
                                     std::size_t league_size)
{
  std::vector<std::int64_t> prizes(league_size, 0);
  if (league_size == 0) return prizes;
  const double clubs = static_cast<double>(league_size);
  const double revenue =
      static_cast<double>(economy.profile->average_revenue_eur);
  const double merit_pool =
      revenue * static_cast<double>(economy.profile->tv_share) *
      (1.0 - static_cast<double>(Finance::TV_EQUAL_SHARE)) * clubs;
  const double continental_pool =
      revenue * static_cast<double>(economy.profile->continental_share) * clubs;
  const double weight_total = clubs * (clubs + 1.0) / 2.0;
  for (std::size_t position = 0; position < league_size; ++position)
  {
    // [S] Linear merit payments by place (Premier League model).
    double prize =
        merit_pool * (clubs - static_cast<double>(position)) / weight_total;
    if (position < Finance::CONTINENTAL_WEIGHTS.size())
    {
      prize += continental_pool *
               static_cast<double>(Finance::CONTINENTAL_WEIGHTS[position]);
    }
    prizes[position] = static_cast<std::int64_t>(std::llround(prize));
  }
  return prizes;
}

std::int64_t seasonTransferBudget(std::int64_t balance, double revenue)
{
  if (balance <= 0) return 0;
  const double budget =
      static_cast<double>(Finance::TRANSFER_BUDGET_CASH_SHARE) *
          static_cast<double>(balance) +
      static_cast<double>(Finance::TRANSFER_BUDGET_REVENUE_SHARE) * revenue;
  return std::min(balance, static_cast<std::int64_t>(std::llround(budget)));
}

std::int64_t seasonWageBudget(const LeagueEconomy& economy, double revenue,
                              std::int64_t weekly_payroll)
{
  // The board funds the league's typical player wage share with a 10%
  // margin, but never below the payroll it has already committed to.
  const double affordable = revenue * playerWageShare(economy) * 1.1 / 52.0;
  return std::max(static_cast<std::int64_t>(std::llround(affordable)),
                  weekly_payroll + weekly_payroll / 20);
}

double wageIndex(double overall, int age)
{
  double age_factor = 1.0;
  if (age < 19)
    age_factor = 0.35;
  else if (age < 21)
    age_factor = 0.55;
  else if (age < 23)
    age_factor = 0.8;
  else if (age > 32)
    age_factor = 0.8;
  else if (age > 30)
    age_factor = 0.9;
  return std::pow(std::max(overall, 1.0) / 65.0,
                  static_cast<double>(
                      WorldTuning::Generation::WAGE_ABILITY_EXPONENT)) *
         age_factor;
}
}  // namespace ClubEconomy
