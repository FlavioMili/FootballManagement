// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include <gtest/gtest.h>
#include <unistd.h>

#include <algorithm>
#include <memory>
#include <numeric>

#include "controller/game_controller.h"
#include "database/database_connection.h"
#include "database/gamedata.h"
#include "database/repositories/finance_repository.h"
#include "database/repositories/team_repository.h"
#include "global/logger.h"
#include "global/runtime_paths.h"
#include "model/club_economy.h"
#include "model/finances.h"
#include "model/team.h"

namespace
{
constexpr std::uint64_t WORLD_SEED = 424242;

/** Save slot unique to this process (tests run in parallel processes). */
int financeSlot()
{
  return 100'000 + static_cast<int>(getpid() % 100'000) * 10 + 5;
}

const GameDateValue DAY_ONE(2025, 7, 2);
const GameDateValue DAY_TWO(2025, 7, 9);
const GameDateValue DAY_THREE(2025, 8, 1);

std::int64_t categoryTotal(const Finances& finances, FinanceCategory category)
{
  std::int64_t total = 0;
  for (const FinanceTransaction& transaction : finances.getLedger())
  {
    if (transaction.category == category) total += transaction.amount;
  }
  return total;
}
}  // namespace

TEST(FinancesTest, BalanceAlwaysEqualsTheLedger)
{
  Finances finances(1'000'000, DAY_ONE);
  finances.record(DAY_TWO, FinanceCategory::Wages, -250'000);
  finances.record(DAY_TWO, FinanceCategory::Matchday, 400'000);
  finances.record(DAY_THREE, FinanceCategory::Broadcasting, 90'000);

  EXPECT_EQ(finances.getBalance(), 1'240'000);
  EXPECT_EQ(finances.ledgerTotal(), finances.getBalance());
  ASSERT_EQ(finances.getLedger().size(), 4u);
  EXPECT_EQ(finances.getLedger().front().category,
            FinanceCategory::OpeningBalance);

  EXPECT_TRUE(finances.revertLastTransaction());
  EXPECT_EQ(finances.getBalance(), 1'150'000);
  EXPECT_EQ(finances.ledgerTotal(), finances.getBalance());
}

TEST(FinancesTest, SummaryTotalsOnlyTheRequestedDates)
{
  Finances finances(0, DAY_ONE);
  finances.record(DAY_TWO, FinanceCategory::Wages, -100);
  finances.record(DAY_TWO, FinanceCategory::Sponsorship, 300);
  finances.record(DAY_THREE, FinanceCategory::Staff, -50);

  const FinanceSummary july =
      finances.summarize(GameDateValue(2025, 7, 3), GameDateValue(2025, 7, 31));
  EXPECT_EQ(july.income, 300);
  EXPECT_EQ(july.expenses, 100);
  EXPECT_EQ(july.net(), 200);
  EXPECT_EQ(july.by_category[static_cast<std::size_t>(FinanceCategory::Wages)],
            -100);
  EXPECT_EQ(july.by_category[static_cast<std::size_t>(FinanceCategory::Staff)],
            0);
}

TEST(FinancesTest, TransferAndWageBudgetsAreSeparate)
{
  Finances finances(50'000'000, DAY_ONE);
  finances.setTransferBudget(10'000'000);
  finances.setWageBudget(200'000);

  finances.record(DAY_TWO, FinanceCategory::TransferFeeOut, -4'000'000);
  EXPECT_EQ(finances.getTransferBudget(), 6'000'000);
  EXPECT_EQ(finances.getWageBudget(), 200'000);

  // Half of a sale is reinvested in the transfer budget.
  finances.record(DAY_TWO, FinanceCategory::TransferFeeIn, 2'000'000);
  EXPECT_EQ(finances.getTransferBudget(), 7'000'000);

  // Wages do not touch the transfer budget.
  finances.record(DAY_TWO, FinanceCategory::Wages, -150'000);
  EXPECT_EQ(finances.getTransferBudget(), 7'000'000);

  // 5.2M of transfer budget buys 100k of weekly wage room.
  EXPECT_TRUE(finances.moveTransferToWageBudget(5'200'000, 150'000));
  EXPECT_EQ(finances.getTransferBudget(), 1'800'000);
  EXPECT_EQ(finances.getWageBudget(), 300'000);
  // Cannot overdraw the transfer budget or undercut the payroll.
  EXPECT_FALSE(finances.moveTransferToWageBudget(5'200'000, 150'000));
  EXPECT_FALSE(finances.moveTransferToWageBudget(-10'400'000, 150'000));
  EXPECT_EQ(finances.ledgerTotal(), finances.getBalance());
}

TEST(FinancesTest, LedgerRoundTripsThroughTheDatabase)
{
  Logger::init();
  auto db = std::make_shared<DatabaseConnection>(":memory:");
  db->initialize();

  Team team(7, 1, "Ledger FC", 5'000'000);
  team.getFinances().record(DAY_TWO, FinanceCategory::Wages, -120'000);
  team.getFinances().record(DAY_THREE, FinanceCategory::PrizeMoney, 800'000);
  TeamRepository team_repo(db);
  team_repo.insertTeamWithId(team);
  team_repo.updateTeamState(team);
  // Flushing twice before markPersisted() must not duplicate rows.
  team_repo.updateTeamState(team);

  const auto ledgers = FinanceRepository(db).loadAll();
  const auto stored = ledgers.find(team.getId());
  ASSERT_NE(stored, ledgers.end());
  ASSERT_EQ(stored->second.size(), team.getFinances().getLedger().size());
  for (std::size_t i = 0; i < stored->second.size(); ++i)
  {
    EXPECT_EQ(stored->second[i].amount,
              team.getFinances().getLedger()[i].amount);
    EXPECT_EQ(stored->second[i].category,
              team.getFinances().getLedger()[i].category);
    EXPECT_EQ(stored->second[i].date, team.getFinances().getLedger()[i].date);
  }

  Finances restored(0);
  restored.restoreLedger(stored->second);
  EXPECT_EQ(restored.getBalance(), team.getFinances().getBalance());
  EXPECT_TRUE(restored.pendingTransactions().empty());
}

TEST(ClubEconomyTest, PrizeMoneyRewardsHigherPlaces)
{
  const LeagueEconomy economy =
      makeLeagueEconomy(3, std::vector<std::uint8_t>(20, 90));
  const auto prizes = ClubEconomy::prizeMoney(economy, 20);
  ASSERT_EQ(prizes.size(), 20u);
  EXPECT_TRUE(std::ranges::is_sorted(prizes, std::greater<>{}));
  EXPECT_GT(prizes.back(), 0);

  const double revenue =
      static_cast<double>(economy.profile->average_revenue_eur);
  const double expected_pool =
      revenue * 20.0 *
      (static_cast<double>(economy.profile->tv_share) *
           (1.0 - static_cast<double>(WorldTuning::Finance::TV_EQUAL_SHARE)) +
       static_cast<double>(economy.profile->continental_share));
  const auto total = static_cast<double>(
      std::accumulate(prizes.begin(), prizes.end(), std::int64_t{0}));
  EXPECT_NEAR(total, expected_pool, expected_pool * 0.001);
}

TEST(ClubEconomyTest, AttendanceFollowsSuccessPriceAndCapacity)
{
  const LeagueEconomy economy = makeLeagueEconomy(1, {70, 75, 80, 85, 90, 95});
  ClubProfile profile;
  profile.reputation = 80;
  profile.stadium_capacity = 40'000;
  profile.ticket_price = static_cast<std::uint32_t>(
      ClubEconomy::fairTicketPrice(economy, profile));

  const auto neutral =
      ClubEconomy::attendance(economy, profile, 0.0, 80, MatchType::LEAGUE);
  const auto winning =
      ClubEconomy::attendance(economy, profile, 1.0, 80, MatchType::LEAGUE);
  const auto losing =
      ClubEconomy::attendance(economy, profile, -1.0, 80, MatchType::LEAGUE);
  EXPECT_GE(winning, neutral);
  EXPECT_LT(losing, neutral);
  EXPECT_LE(winning, profile.stadium_capacity);

  ClubProfile expensive = profile;
  expensive.ticket_price *= 3;
  EXPECT_LT(
      ClubEconomy::attendance(economy, expensive, -1.0, 80, MatchType::LEAGUE),
      losing);
  EXPECT_LT(
      ClubEconomy::attendance(economy, profile, 0.0, 80, MatchType::FRIENDLY),
      neutral);

  // At the reference price a season of home games meets the gate target.
  const double season_gate = static_cast<double>(neutral) *
                             static_cast<double>(profile.ticket_price) *
                             WorldTuning::Finance::HOME_MATCHES_PER_SEASON;
  const double target =
      ClubEconomy::expectedRevenue(economy, profile.reputation) *
      static_cast<double>(economy.profile->gate_share);
  EXPECT_NEAR(season_gate, target, target * 0.05);
}

class FinanceWorldTest : public ::testing::Test
{
 protected:
  void SetUp() override
  {
    Logger::init();
    controller = std::make_unique<GameController>();
    controller->newGame(financeSlot(), WORLD_SEED);
  }

  void TearDown() override
  {
    controller.reset();
    RuntimePaths::removeSave(financeSlot());
  }

  std::unique_ptr<GameController> controller;
};

TEST_F(FinanceWorldTest, LedgersReconcileThroughMonthsOfPlayAndReload)
{
  const TeamID managed = controller->getTeams().front().get().getId();
  controller->selectManagedTeam(managed);
  for (int day = 0; day < 40; ++day) controller->advanceDay();

  auto gamedata = controller->getGameData();
  std::size_t clubs_with_gate = 0;
  for (const auto& team_ref : controller->getTeams())
  {
    const Finances& finances = team_ref.get().getFinances();
    ASSERT_EQ(finances.getBalance(), finances.ledgerTotal())
        << team_ref.get().getName();
    EXPECT_LT(categoryTotal(finances, FinanceCategory::Wages), 0);
    EXPECT_GT(categoryTotal(finances, FinanceCategory::Broadcasting), 0);
    EXPECT_GT(categoryTotal(finances, FinanceCategory::Sponsorship), 0);
    EXPECT_LT(categoryTotal(finances, FinanceCategory::Staff), 0);
    if (categoryTotal(finances, FinanceCategory::Matchday) > 0)
      ++clubs_with_gate;
  }
  EXPECT_GT(clubs_with_gate, 0u) << "home friendlies must earn gate receipts";

  const Finances& managed_finances =
      controller->getManagedTeam()->get().getFinances();
  const auto ledger_size = managed_finances.getLedger().size();
  const auto balance = managed_finances.getBalance();
  const auto transfer_budget = managed_finances.getTransferBudget();
  const auto wage_budget = managed_finances.getWageBudget();
  controller->saveGame();

  controller = std::make_unique<GameController>();
  ASSERT_TRUE(controller->loadGame(financeSlot()));
  const Finances& reloaded = controller->getManagedTeam()->get().getFinances();
  EXPECT_EQ(reloaded.getLedger().size(), ledger_size);
  EXPECT_EQ(reloaded.getBalance(), balance);
  EXPECT_EQ(reloaded.ledgerTotal(), balance);
  EXPECT_EQ(reloaded.getTransferBudget(), transfer_budget);
  EXPECT_EQ(reloaded.getWageBudget(), wage_budget);
}

TEST_F(FinanceWorldTest, TransferFeesAreRecordedForBothClubs)
{
  const auto& teams = controller->getTeams();
  ASSERT_GE(teams.size(), 2u);
  const TeamID buyer_id = teams[0].get().getId();
  const TeamID seller_id = teams[1].get().getId();
  const PlayerID player_id =
      controller->getPlayersForTeam(seller_id).front().get().getId();
  auto gamedata = controller->getGameData();
  gamedata->getTeams().at(buyer_id).getFinances().addBalance(100'000'000);

  const std::int64_t buyer_budget =
      gamedata->getTeams().at(buyer_id).getFinances().getTransferBudget();
  controller->listPlayerForTransfer(player_id, 1'000'000);
  ASSERT_TRUE(controller->buyPlayer(player_id, buyer_id, 1'000'000));

  const Finances& buyer = gamedata->getTeams().at(buyer_id).getFinances();
  const Finances& seller = gamedata->getTeams().at(seller_id).getFinances();
  EXPECT_EQ(buyer.getLedger().back().category, FinanceCategory::TransferFeeOut);
  EXPECT_EQ(buyer.getLedger().back().amount, -1'000'000);
  EXPECT_EQ(seller.getLedger().back().category, FinanceCategory::TransferFeeIn);
  EXPECT_EQ(seller.getLedger().back().amount, 1'000'000);
  EXPECT_EQ(buyer.getTransferBudget(), buyer_budget - 1'000'000);
  EXPECT_EQ(buyer.getBalance(), buyer.ledgerTotal());
  EXPECT_EQ(seller.getBalance(), seller.ledgerTotal());
}
