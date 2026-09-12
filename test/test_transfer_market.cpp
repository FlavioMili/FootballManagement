// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>

#include "controller/game_controller.h"
#include "database/database_connection.h"
#include "database/gamedata.h"
#include "database/repositories/competition_repository.h"
#include "global/logger.h"
#include "model/calendar.h"
#include "model/competition_manager.h"
#include "model/season_history.h"
#include "model/transfer_listing.h"
#include "model/transfer_market.h"
#include "model/transfer_tuning.h"
#include "model/world_tuning.h"
#include "model/world_rng.h"
#include "model/world_simulation.h"

class TransferMarketTest : public ::testing::Test
{
 protected:
  void SetUp() override
  {
    Logger::init();
    controller = std::make_unique<GameController>();
    controller->newGame(0);
    controller->selectManagedTeam(1);  // Select a dummy team for testing
  }

  void TearDown() override { controller.reset(); }

  std::unique_ptr<GameController> controller;
};

TEST_F(TransferMarketTest, BidOnPlayer)
{
  auto all_teams = controller->getTeams();
  ASSERT_FALSE(all_teams.empty());

  TeamID managed_team_id = all_teams.front().get().getId();
  controller->selectManagedTeam(managed_team_id);

  TeamID test_team_id = all_teams.back().get().getId();
  if (test_team_id == managed_team_id)
  {
    test_team_id = all_teams[1].get().getId();
  }

  auto team_players = controller->getPlayersForTeam(test_team_id);
  ASSERT_FALSE(team_players.empty());
  PlayerID test_player_id = team_players.front().get().getId();

  controller->listPlayerForTransfer(test_player_id, 10);

  auto listings = controller->getAllListings();
  ASSERT_FALSE(listings.empty());

  auto first_listing = listings.begin()->second;
  auto player_id = first_listing.player_id;

  int manager_team_id = controller->getManagedTeam()->get().getId();

  auto gamedata = controller->getGameData();
  gamedata->getTeams()
      .at(manager_team_id)
      .getFinances()
      .addBalance(1000000000LL);

  // Make a bid
  long long bid_amount = first_listing.asking_price + 1000;
  bool success = controller->submitBid(player_id, manager_team_id, bid_amount);

  EXPECT_TRUE(success);

  const auto otherBuyer =
      std::ranges::find_if(all_teams,
                           [manager_team_id, test_team_id](const auto& team)
                           {
                             return team.get().getId() != manager_team_id &&
                                    team.get().getId() != test_team_id;
                           });
  ASSERT_NE(otherBuyer, all_teams.end());
  gamedata->getTeams()
      .at(otherBuyer->get().getId())
      .getFinances()
      .addBalance(1000000000LL);
  EXPECT_FALSE(controller->buyPlayer(player_id, otherBuyer->get().getId(),
                                     static_cast<uint32_t>(bid_amount)))
      << "A club must not reuse another club's winning bid";

  ASSERT_TRUE(controller->counterOffer(
      player_id, static_cast<uint32_t>(bid_amount + 500)));
  const auto countered = controller->getAllListings().find(player_id);
  ASSERT_NE(countered, controller->getAllListings().end());
  EXPECT_FALSE(countered->second.highest_bidder_id.has_value());
  EXPECT_EQ(countered->second.highest_bid, 0u);
}

TEST_F(TransferMarketTest, AIEvaluatesAndAcceptsBid)
{
  auto all_teams = controller->getTeams();
  ASSERT_GE(all_teams.size(), 2u);

  TeamID buyer_team_id = all_teams[0].get().getId();
  TeamID seller_team_id = all_teams[1].get().getId();

  auto seller_players = controller->getPlayersForTeam(seller_team_id);
  ASSERT_FALSE(seller_players.empty());
  PlayerID test_player_id = seller_players.front().get().getId();

  controller->listPlayerForTransfer(test_player_id, 10000);

  auto gamedata = controller->getGameData();
  gamedata->getTeams().at(buyer_team_id).getFinances().addBalance(10000000LL);

  bool bid_submitted =
      controller->submitBid(test_player_id, buyer_team_id, 15000);
  EXPECT_TRUE(bid_submitted);

  // Advance day to trigger AI activity and bid processing
  controller->advanceDay();

  // The transfer should have been completed by the AI accepting the bid
  EXPECT_FALSE(controller->isPlayerListed(test_player_id));
}

TEST_F(TransferMarketTest, BidSurvivesSaveAndReload)
{
  const auto teams = controller->getTeams();
  ASSERT_GE(teams.size(), 2u);
  const TeamID buyerId = teams[0].get().getId();
  const TeamID sellerId = teams[1].get().getId();

  const auto sellerPlayers = controller->getPlayersForTeam(sellerId);
  const auto target = std::ranges::find_if(
      sellerPlayers, [this](const auto& player)
      { return !controller->isPlayerListed(player.get().getId()); });
  ASSERT_NE(target, sellerPlayers.end());
  const PlayerID playerId = target->get().getId();

  controller->getGameData()->getTeams().at(buyerId).getFinances().addBalance(
      1'000'000'000LL);
  controller->listPlayerForTransfer(playerId, 2'000'000);
  ASSERT_TRUE(controller->submitBid(playerId, buyerId, 2'100'000));
  controller->saveGame();

  controller = std::make_unique<GameController>();
  ASSERT_TRUE(controller->loadGame(0));
  const auto listing = controller->getAllListings().find(playerId);
  ASSERT_NE(listing, controller->getAllListings().end());
  ASSERT_TRUE(listing->second.highest_bidder_id.has_value());
  EXPECT_EQ(*listing->second.highest_bidder_id, buyerId);
  EXPECT_EQ(listing->second.highest_bid, 2'100'000u);
}

TEST_F(TransferMarketTest, AcceptedTransferAndBalancesSurviveReload)
{
  const auto teams = controller->getTeams();
  ASSERT_GE(teams.size(), 2u);
  const TeamID buyerId = teams[0].get().getId();
  const TeamID sellerId = teams[1].get().getId();

  const auto sellerPlayers = controller->getPlayersForTeam(sellerId);
  const auto target = std::ranges::find_if(
      sellerPlayers, [this](const auto& player)
      { return !controller->isPlayerListed(player.get().getId()); });
  ASSERT_NE(target, sellerPlayers.end());
  const PlayerID playerId = target->get().getId();

  auto gameData = controller->getGameData();
  gameData->getTeams().at(buyerId).getFinances().addBalance(1'000'000'000LL);
  controller->listPlayerForTransfer(playerId, 1'500'000);
  ASSERT_TRUE(controller->submitBid(playerId, buyerId, 1'600'000));
  ASSERT_TRUE(controller->acceptBid(playerId));
  const int64_t buyerBalance =
      gameData->getTeams().at(buyerId).getFinances().getBalance();
  const int64_t sellerBalance =
      gameData->getTeams().at(sellerId).getFinances().getBalance();
  controller->saveGame();

  controller = std::make_unique<GameController>();
  ASSERT_TRUE(controller->loadGame(0));
  gameData = controller->getGameData();
  const auto player = gameData->getPlayer(playerId);
  ASSERT_TRUE(player.has_value());
  EXPECT_EQ(player->get().getTeamId(), buyerId);
  EXPECT_EQ(gameData->getTeams().at(buyerId).getFinances().getBalance(),
            buyerBalance);
  EXPECT_EQ(gameData->getTeams().at(sellerId).getFinances().getBalance(),
            sellerBalance);
  EXPECT_FALSE(controller->isPlayerListed(playerId));
}

TEST_F(TransferMarketTest, ContractTermsAreNegotiatedAndPersisted)
{
  const auto teams = controller->getTeams();
  ASSERT_GE(teams.size(), 2u);
  const TeamID buyerId = teams[0].get().getId();
  const TeamID sellerId = teams[1].get().getId();
  const auto sellerPlayers = controller->getPlayersForTeam(sellerId);
  ASSERT_FALSE(sellerPlayers.empty());
  const PlayerID playerId = sellerPlayers.front().get().getId();
  constexpr uint32_t ASKING_PRICE = 1'000'000;

  auto gameData = controller->getGameData();
  gameData->getTeams().at(buyerId).getFinances().addBalance(1'000'000'000LL);
  controller->listPlayerForTransfer(playerId, ASKING_PRICE);

  const GameController::ContractTerms demand =
      controller->getContractDemand(playerId, false);
  ASSERT_GT(demand.weekly_wage, 0u);
  GameController::ContractTerms insufficient = demand;
  --insufficient.weekly_wage;
  EXPECT_FALSE(controller->buyPlayerWithContract(playerId, buyerId,
                                                 ASKING_PRICE, insufficient));
  EXPECT_EQ(gameData->getPlayer(playerId)->get().getTeamId(), sellerId);

  ASSERT_TRUE(controller->buyPlayerWithContract(playerId, buyerId, ASKING_PRICE,
                                                demand));
  EXPECT_EQ(gameData->getPlayer(playerId)->get().getWage(), demand.weekly_wage);
  EXPECT_EQ(gameData->getPlayer(playerId)->get().getContractYears(),
            demand.years);
  controller->saveGame();

  controller = std::make_unique<GameController>();
  ASSERT_TRUE(controller->loadGame(0));
  const auto reloadedPlayer = controller->getGameData()->getPlayer(playerId);
  ASSERT_TRUE(reloadedPlayer.has_value());
  EXPECT_EQ(reloadedPlayer->get().getTeamId(), buyerId);
  EXPECT_EQ(reloadedPlayer->get().getWage(), demand.weekly_wage);
  EXPECT_EQ(reloadedPlayer->get().getContractYears(), demand.years);
}

// ---------------------------------------------------------------------------
// Structured deals, loans, pre-contracts and the AI market
// ---------------------------------------------------------------------------

namespace
{
using TransferNegotiation::ClubResponse;
using TransferNegotiation::ContractKind;

const TransferMarket& marketOf(const GameController& controller)
{
  return controller.getGame()->getTransfers();
}

int64_t categoryTotal(const Finances& finances, FinanceCategory category)
{
  int64_t total = 0;
  for (const FinanceTransaction& transaction : finances.getLedger())
    if (transaction.category == category) total += transaction.amount;
  return total;
}

/** A player of another club who would join @p club and is not a key
 * player there (so the seller is willing to negotiate). */
PlayerID findWillingTarget(const GameController& controller, TeamID club,
                           SquadRole min_role = SquadRole::FirstTeam)
{
  const TransferMarket& market = marketOf(controller);
  for (const auto& team : controller.getTeams())
  {
    if (team.get().getId() == club) continue;
    for (const auto& player : controller.getPlayersForTeam(team.get().getId()))
    {
      const PlayerID id = player.get().getId();
      if (player.get().getAge() > 21 && market.canBeTraded(id) &&
          static_cast<int>(controller.getSquadRole(id)) >=
              static_cast<int>(min_role) &&
          market.wouldJoin(id, club, ContractKind::Transfer))
        return id;
    }
  }
  return 0;
}

/** Manages the first generated club (team ids depend on the leagues). */
TeamID manageFirstClub(GameController& controller)
{
  const TeamID club = controller.getTeams().front().get().getId();
  controller.selectManagedTeam(club);
  return club;
}

/** Standalone market over the controller's world (state loaded from the
 * save) so tests can run its calendar without simulating matches. */
struct MarketHarness
{
  explicit MarketHarness(GameController& controller)
      : world(controller.getGameData()),
        competitions(controller.getGameData(), controller.getDbConn()),
        market(controller.getGameData(), world, competitions)
  {
    market.load(controller.getDbConn());
  }
  WorldSimulation world;
  CompetitionManager competitions;
  TransferMarket market;
};
}  // namespace

TEST_F(TransferMarketTest, StructuredOfferContractTalksAndLedger)
{
  const TeamID managed = manageFirstClub(*controller);
  auto gamedata = controller->getGameData();
  gamedata->getTeams().at(managed).getFinances().addBalance(2'000'000'000LL);
  const PlayerID target = findWillingTarget(*controller, managed);
  ASSERT_NE(target, 0u);
  const TeamID seller = gamedata->getPlayer(target)->get().getTeamId();
  const uint32_t value = controller->getPlayerMarketValue(target);

  TransferNegotiation::OfferTerms terms;
  terms.fee = std::max(value * 3, 1'000'000u);
  terms.upfront_percent = 40;
  terms.instalment_years = 3;
  terms.appearance_bonus = 500'000;
  terms.appearance_target = 20;
  terms.sell_on_percent = 15;
  const ClubResponse club = controller->makeTransferOffer(target, terms);
  ASSERT_EQ(club.decision, ClubResponse::Decision::Accept);
  ASSERT_EQ(controller->getContractTalkKind(target), ContractKind::Transfer);

  const auto demand =
      controller->getPlayerDemand(target, ContractKind::Transfer);
  TransferNegotiation::ContractOffer lowball =
      TransferNegotiation::demandedOffer(demand);
  lowball.weekly_wage /= 3;
  const auto refused = controller->proposeContract(target, lowball);
  EXPECT_FALSE(refused.completed);
  EXPECT_FALSE(refused.response.accepted);
  EXPECT_EQ(refused.rounds_left,
            TransferTuning::Negotiation::MAX_PLAYER_ROUNDS - 1);

  const int64_t seller_before =
      gamedata->getTeams().at(seller).getFinances().getBalance();
  const auto offer = TransferNegotiation::demandedOffer(demand);
  const auto agreed = controller->proposeContract(target, offer);
  ASSERT_TRUE(agreed.completed);
  EXPECT_EQ(gamedata->getPlayer(target)->get().getTeamId(), managed);
  EXPECT_EQ(gamedata->getPlayer(target)->get().getWage(), offer.weekly_wage);

  const uint32_t upfront = TransferNegotiation::upfrontAmount(terms);
  EXPECT_EQ(gamedata->getTeams().at(seller).getFinances().getBalance() -
                seller_before,
            static_cast<int64_t>(upfront));
  const auto& ledger =
      gamedata->getTeams().at(managed).getFinances().getLedger();
  const bool paid_upfront = std::ranges::any_of(
      ledger,
      [&](const FinanceTransaction& t)
      {
        return t.category == FinanceCategory::TransferFeeOut &&
               t.amount == -static_cast<int64_t>(upfront);
      });
  const bool paid_agent = std::ranges::any_of(
      ledger,
      [&](const FinanceTransaction& t)
      {
        return t.category == FinanceCategory::TransferFeeOut &&
               t.amount == -static_cast<int64_t>(terms.fee / 10);
      });
  EXPECT_TRUE(paid_upfront);
  EXPECT_TRUE(paid_agent) << "agent fee is ~10% of the fee";

  const auto count_kind = [&](const GameController& c, ObligationKind kind)
  {
    return std::ranges::count_if(
        marketOf(c).obligations(), [&](const TransferObligation& o)
        { return o.player_id == target && o.kind == kind; });
  };
  EXPECT_EQ(count_kind(*controller, ObligationKind::Instalment), 3);
  EXPECT_EQ(count_kind(*controller, ObligationKind::AppearanceBonus), 1);
  EXPECT_EQ(count_kind(*controller, ObligationKind::SellOn), 1);
  const auto history = marketOf(*controller).historyFor(target);
  ASSERT_FALSE(history.empty());
  EXPECT_EQ(history.back().kind, TransferKind::Permanent);
  EXPECT_EQ(history.back().fee, terms.fee);

  controller->saveGame();
  controller = std::make_unique<GameController>();
  ASSERT_TRUE(controller->loadGame(0));
  EXPECT_EQ(count_kind(*controller, ObligationKind::Instalment), 3);
  EXPECT_EQ(count_kind(*controller, ObligationKind::SellOn), 1);
  EXPECT_EQ(marketOf(*controller).historyFor(target).size(), history.size());
  const auto instalments = TransferNegotiation::instalmentAmounts(terms);
  EXPECT_EQ(marketOf(*controller)
                .committedPayables(managed, GameDateValue(2026, 7, 3)),
            static_cast<int64_t>(instalments.front()))
      << "next season's instalment is reserved from next season's budget";
}

TEST_F(TransferMarketTest, InstalmentsAddOnsAndSellOnsArePaid)
{
  controller->saveGame();
  MarketHarness harness(*controller);
  TransferMarket& market = harness.market;
  auto gamedata = controller->getGameData();
  const auto& teams = controller->getTeams();
  const TeamID a = teams[2].get().getId();
  const TeamID b = teams[3].get().getId();
  const TeamID c = teams[4].get().getId();
  const PlayerID player = controller->getPlayersForTeam(a).back().get().getId();
  const GameDateValue signed_on(2025, 7, 10);

  TransferMarket::Deal deal;
  deal.player_id = player;
  deal.buyer_id = b;
  deal.terms.fee = 9'000'000;
  deal.terms.upfront_percent = 40;
  deal.terms.instalment_years = 2;
  deal.terms.goal_bonus = 300'000;
  deal.terms.goal_target = 10;
  deal.terms.sell_on_percent = 20;
  deal.contract.weekly_wage = 5'000;
  deal.contract.years = 3;
  ASSERT_TRUE(market.completeTransfer(deal, signed_on, FREE_AGENTS_TEAM_ID));

  const int64_t a_before =
      gamedata->getTeams().at(a).getFinances().getBalance();
  const int64_t b_before =
      gamedata->getTeams().at(b).getFinances().getBalance();
  market.onDayAdvanced(signed_on + 365, FREE_AGENTS_TEAM_ID);
  EXPECT_EQ(gamedata->getTeams().at(a).getFinances().getBalance() - a_before,
            2'700'000);
  EXPECT_EQ(b_before - gamedata->getTeams().at(b).getFinances().getBalance(),
            2'700'000);

  // The goal add-on lapses once the player leaves the buyer; the sell-on
  // pays 20% of the next fee to the first club.
  TransferMarket::Deal resale = deal;
  resale.buyer_id = c;
  resale.terms = TransferNegotiation::OfferTerms{};
  resale.terms.fee = 5'000'000;
  const int64_t a_mid = gamedata->getTeams().at(a).getFinances().getBalance();
  const int64_t b_mid = gamedata->getTeams().at(b).getFinances().getBalance();
  ASSERT_TRUE(market.completeTransfer(resale, GameDateValue(2026, 8, 1),
                                      FREE_AGENTS_TEAM_ID));
  EXPECT_EQ(gamedata->getTeams().at(a).getFinances().getBalance() - a_mid,
            1'000'000);
  EXPECT_EQ(gamedata->getTeams().at(b).getFinances().getBalance() - b_mid,
            4'000'000);
  EXPECT_FALSE(std::ranges::any_of(
      market.obligations(), [&](const TransferObligation& o)
      { return o.kind == ObligationKind::SellOn && o.payee == a; }));
  market.onDayAdvanced(GameDateValue(2026, 8, 2), FREE_AGENTS_TEAM_ID);
  for (const auto& team : controller->getTeams())
    EXPECT_EQ(team.get().getFinances().getBalance(),
              team.get().getFinances().ledgerTotal());
}

TEST_F(TransferMarketTest, LoanOfferLifecycleSurvivesReload)
{
  const TeamID managed = manageFirstClub(*controller);
  auto gamedata = controller->getGameData();
  gamedata->getTeams().at(managed).getFinances().addBalance(100'000'000LL);
  const TransferMarket& market = marketOf(*controller);

  PlayerID target = 0;
  for (const auto& team : controller->getTeams())
  {
    if (team.get().getId() == managed || target != 0) continue;
    for (const auto& player : controller->getPlayersForTeam(team.get().getId()))
    {
      const PlayerID id = player.get().getId();
      std::vector<TransferNegotiation::Reason> reasons;
      const SquadRole role = controller->getSquadRole(id);
      if (static_cast<int>(role) >= static_cast<int>(SquadRole::Backup) &&
          market.canBeTraded(id) &&
          TransferNegotiation::playerAcceptsLoan(
              role, market.projectedRole(id, managed),
              player.get().getTraits().ambition,
              team.get().getReputation() -
                  controller->getManagedTeam()->get().getReputation(),
              reasons))
      {
        target = id;
        break;
      }
    }
  }
  ASSERT_NE(target, 0u);
  const TeamID parent = gamedata->getPlayer(target)->get().getTeamId();
  const uint32_t full_wage = gamedata->getPlayer(target)->get().getWage();

  TransferNegotiation::LoanTerms terms;
  terms.wage_share = 20;
  const ClubResponse counter = controller->makeLoanOffer(target, terms);
  ASSERT_EQ(counter.decision, ClubResponse::Decision::Counter)
      << (counter.reasons.empty()
              ? "none"
              : TransferNegotiation::reasonKey(counter.reasons.front()));
  terms.wage_share = counter.counter_wage_share;
  terms.recall_clause = true;
  const ClubResponse accepted = controller->makeLoanOffer(target, terms);
  ASSERT_EQ(accepted.decision, ClubResponse::Decision::Accept);
  EXPECT_EQ(gamedata->getPlayer(target)->get().getTeamId(), managed);
  EXPECT_EQ(gamedata->getPlayer(target)->get().getWage(),
            full_wage * terms.wage_share / 100);
  ASSERT_NE(market.findLoan(target), nullptr);
  EXPECT_FALSE(market.canBeTraded(target)) << "no sub-loans or resale";
  controller->listPlayerForTransfer(target, 1'000'000);
  EXPECT_FALSE(controller->isPlayerListed(target));
  EXPECT_FALSE(controller->releasePlayer(target));

  controller->saveGame();
  controller = std::make_unique<GameController>();
  ASSERT_TRUE(controller->loadGame(0));
  const LoanDeal* loan = marketOf(*controller).findLoan(target);
  ASSERT_NE(loan, nullptr);
  EXPECT_EQ(loan->parent, parent);
  EXPECT_EQ(loan->end, GameDateValue(2026, 6, 30));
  EXPECT_TRUE(loan->recall_clause);

  // The parent pays its wage share weekly; the player returns on 30 June.
  MarketHarness harness(*controller);
  gamedata = controller->getGameData();
  const int64_t parent_wages = categoryTotal(
      gamedata->getTeams().at(parent).getFinances(), FinanceCategory::Wages);
  GameDateValue date(2025, 7, 3);
  while (dayOrdinal(date) % 7 != 0) date = date + 1;
  harness.market.onDayAdvanced(date, managed);
  EXPECT_EQ(parent_wages -
                categoryTotal(gamedata->getTeams().at(parent).getFinances(),
                              FinanceCategory::Wages),
            static_cast<int64_t>(full_wage) * (100 - terms.wage_share) / 100);
  harness.market.onDayAdvanced(GameDateValue(2026, 6, 30), managed);
  EXPECT_EQ(gamedata->getPlayer(target)->get().getTeamId(), parent);
  EXPECT_EQ(gamedata->getPlayer(target)->get().getWage(), full_wage);
  EXPECT_EQ(harness.market.findLoan(target), nullptr);
  EXPECT_EQ(harness.market.historyFor(target).back().kind,
            TransferKind::LoanReturn);
}

TEST_F(TransferMarketTest, LoanCapsLimitLoansBetweenClubs)
{
  controller->saveGame();
  MarketHarness harness(*controller);
  const auto& teams = controller->getTeams();
  const TeamID parent = teams[5].get().getId();
  const TeamID borrower = teams[6].get().getId();
  std::vector<PlayerID> seniors;
  for (const auto& player : controller->getPlayersForTeam(parent))
    if (player.get().getAge() > TransferTuning::Loan::EXEMPT_MAX_AGE)
      seniors.push_back(player.get().getId());
  ASSERT_GE(seniors.size(), 4u);
  TransferNegotiation::LoanTerms terms;
  const GameDateValue date(2025, 7, 20);
  for (int i = 0; i < TransferTuning::Loan::MAX_BETWEEN_CLUBS; ++i)
    ASSERT_TRUE(harness.market.startLoan(seniors[static_cast<size_t>(i)],
                                         borrower, terms, date,
                                         FREE_AGENTS_TEAM_ID));
  EXPECT_FALSE(harness.market.loanWithinLimits(seniors[3], parent, borrower));
  EXPECT_FALSE(harness.market.startLoan(seniors[3], borrower, terms, date,
                                        FREE_AGENTS_TEAM_ID));
  EXPECT_FALSE(harness.market.startLoan(seniors[0], teams[7].get().getId(),
                                        terms, date, FREE_AGENTS_TEAM_ID))
      << "a loanee cannot be loaned on";
}

TEST_F(TransferMarketTest, PreContractSignedInJanuaryCompletesOnFirstJuly)
{
  controller->saveGame();
  const TeamID managed = manageFirstClub(*controller);
  auto gamedata = controller->getGameData();
  const TeamID owner = controller->getTeams()[8].get().getId();
  const PlayerID player =
      controller->getPlayersForTeam(owner).front().get().getId();
  gamedata->getPlayers().at(player).setContractYears(2);

  TransferNegotiation::ContractOffer terms;
  terms.weekly_wage = 12'345;
  terms.years = 3;
  terms.signing_bonus = 50'000;
  terms.promised_role = SquadRole::FirstTeam;
  {
    MarketHarness harness(*controller);
    EXPECT_FALSE(harness.market.agreePreContract(
        player, managed, terms, GameDateValue(2026, 1, 5), managed))
        << "only players in their final six months";
    gamedata->getPlayers().at(player).setContractYears(1);
    EXPECT_FALSE(harness.market.agreePreContract(
        player, managed, terms, GameDateValue(2025, 12, 20), managed));
    ASSERT_TRUE(harness.market.agreePreContract(
        player, managed, terms, GameDateValue(2026, 1, 5), managed));
    EXPECT_FALSE(harness.market.canBeTraded(player));
    harness.market.save(controller->getDbConn());
  }
  MarketHarness reloaded(*controller);
  ASSERT_NE(reloaded.market.findPreContract(player), nullptr);
  reloaded.market.onDayAdvanced(GameDateValue(2026, 6, 30), managed);
  EXPECT_EQ(gamedata->getPlayer(player)->get().getTeamId(), owner);
  const int64_t balance =
      gamedata->getTeams().at(managed).getFinances().getBalance();
  reloaded.market.onDayAdvanced(GameDateValue(2026, 7, 1), managed);
  const Player& moved = gamedata->getPlayer(player)->get();
  EXPECT_EQ(moved.getTeamId(), managed);
  EXPECT_EQ(moved.getWage(), terms.weekly_wage);
  EXPECT_EQ(moved.getContractYears(), terms.years);
  EXPECT_EQ(reloaded.market.historyFor(player).back().kind,
            TransferKind::PreContract);
  // Signing bonus plus the agent's fee (weeks of the new wage), no fee.
  EXPECT_EQ(
      balance - gamedata->getTeams().at(managed).getFinances().getBalance(),
      50'000 + 12'345 * TransferTuning::Offer::FREE_AGENT_FEE_WEEKS);
  ASSERT_NE(reloaded.market.flags(player), nullptr);
  EXPECT_EQ(reloaded.market.flags(player)->promised_role, SquadRole::FirstTeam);
}

TEST_F(TransferMarketTest, ReleasedPlayerIsPaidOffAndBecomesFreeAgent)
{
  const TeamID managed = manageFirstClub(*controller);
  auto gamedata = controller->getGameData();
  const auto squad = controller->getPlayersForTeam(managed);
  const auto contracted = std::ranges::find_if(
      squad, [](const auto& p) { return p.get().getContractYears() >= 2; });
  ASSERT_NE(contracted, squad.end());
  const PlayerID player = contracted->get().getId();
  const int64_t cost = controller->getReleaseCost(player);
  EXPECT_EQ(cost, TransferNegotiation::severancePay(
                      contracted->get().getWage(),
                      contracted->get().getContractYears(),
                      controller->getCurrentDate()));
  ASSERT_GT(cost, 0);
  const int64_t balance =
      gamedata->getTeams().at(managed).getFinances().getBalance();
  ASSERT_TRUE(controller->releasePlayer(player));
  EXPECT_EQ(
      balance - gamedata->getTeams().at(managed).getFinances().getBalance(),
      cost);
  EXPECT_EQ(gamedata->getPlayer(player)->get().getTeamId(),
            FREE_AGENTS_TEAM_ID);
  EXPECT_EQ(gamedata->getPlayer(player)->get().getContractYears(), 0);
  EXPECT_EQ(marketOf(*controller).historyFor(player).back().kind,
            TransferKind::Release);

  // Free agents can be signed back; the player's patience is limited.
  ASSERT_EQ(controller->getContractTalkKind(player), ContractKind::FreeAgent);
  TransferNegotiation::ContractOffer insulting;
  insulting.weekly_wage = 1;
  insulting.years = 1;
  for (int round = 0; round < TransferTuning::Negotiation::MAX_PLAYER_ROUNDS;
       ++round)
    EXPECT_FALSE(controller->proposeContract(player, insulting).completed);
  EXPECT_EQ(controller->getContractRoundsLeft(player), 0);
  const auto ended = controller->proposeContract(
      player, TransferNegotiation::demandedOffer(controller->getPlayerDemand(
                  player, ContractKind::FreeAgent)));
  EXPECT_FALSE(ended.completed);
}

TEST_F(TransferMarketTest, IncomingOffersCanBeCounteredAndAccepted)
{
  const TeamID managed = manageFirstClub(*controller);
  auto gamedata = controller->getGameData();
  const TeamID buyer = controller->getTeams()[9].get().getId();
  gamedata->getTeams().at(buyer).getFinances().addBalance(500'000'000LL);
  const PlayerID player =
      controller->getPlayersForTeam(managed).back().get().getId();

  // The test stands in for an AI club making an approach.
  TransferMarket& market =
      const_cast<Game*>(controller->getGame())->getTransfers();
  IncomingOffer offer;
  offer.player_id = player;
  offer.buyer = buyer;
  offer.terms.fee = 1'000'000;
  offer.max_fee = 1'500'000;
  offer.created = controller->getCurrentDate();
  offer.expires = controller->getCurrentDate() + 5;
  const std::uint32_t id = market.addIncomingOffer(offer);
  ASSERT_EQ(controller->getIncomingOffers().size(), 1u);

  const ClubResponse improved = controller->counterIncomingOffer(id, 3'000'000);
  EXPECT_EQ(improved.decision, ClubResponse::Decision::Counter);
  EXPECT_EQ(improved.counter_fee, 1'500'000u) << "capped at the ceiling";

  const int64_t balance =
      gamedata->getTeams().at(managed).getFinances().getBalance();
  const ClubResponse accepted = controller->counterIncomingOffer(id, 1'400'000);
  ASSERT_EQ(accepted.decision, ClubResponse::Decision::Accept);
  EXPECT_EQ(gamedata->getPlayer(player)->get().getTeamId(), buyer);
  EXPECT_EQ(
      gamedata->getTeams().at(managed).getFinances().getBalance() - balance,
      1'400'000);
  EXPECT_TRUE(controller->getIncomingOffers().empty());
}

TEST_F(TransferMarketTest, SummerMarketIsMostlyFreeAndLoanMoves)
{
  manageFirstClub(*controller);
  const auto started = std::chrono::steady_clock::now();
  for (int day = 0; day < 40; ++day) controller->advanceDay();
  const double seconds =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - started)
          .count();

  int fee_moves = 0;
  int free_moves = 0;
  int loans = 0;
  for (const TransferRecord& record : marketOf(*controller).history())
  {
    if (record.kind == TransferKind::Permanent) ++fee_moves;
    if (record.kind == TransferKind::Free) ++free_moves;
    if (record.kind == TransferKind::Loan) ++loans;
  }
  const int moves = fee_moves + free_moves + loans;
  const double fee_share =
      moves > 0 ? static_cast<double>(fee_moves) / moves : 0.0;
  std::cout << "[transfer-calibration] moves=" << moves << " fee=" << fee_moves
            << " free=" << free_moves << " loans=" << loans
            << " fee_share=" << fee_share << " per_club="
            << static_cast<double>(moves) /
                   static_cast<double>(controller->getTeams().size())
            << " seconds=" << seconds << "\n";
  // About two moves per club in the first 40 days of the summer window.
  EXPECT_GT(moves, 800);
  EXPECT_GT(fee_moves, 0);
  EXPECT_GT(loans, 0);
  EXPECT_GT(free_moves, 0);
  // FIFA 2025: 17.7% of men's professional moves carried a fee.
  EXPECT_GT(fee_share, 0.08);
  EXPECT_LT(fee_share, 0.25);
  for (const auto& team : controller->getTeams())
  {
    EXPECT_EQ(team.get().getFinances().getBalance(),
              team.get().getFinances().ledgerTotal());
    EXPECT_GE(team.get().getPlayerIDs().size(), 18u) << team.get().getName();
  }
}

TEST_F(TransferMarketTest, FreeAgentsNeedCashNotTransferBudget)
{
  auto gamedata = controller->getGameData();
  // The club with the most wage room, so only the fee budget is at stake:
  // many lower-league clubs start at their wage cap.
  const auto wageRoom = [&](const Team& team)
  {
    return team.getFinances().getWageBudget() -
           controller->getWeeklyWageBill(team.getId());
  };
  const auto roomiest = std::ranges::max_element(
      controller->getTeams(), {},
      [&](const auto& team) { return wageRoom(team.get()); });
  ASSERT_NE(roomiest, controller->getTeams().end());
  const TeamID managed = roomiest->get().getId();
  controller->selectManagedTeam(managed);
  const auto& squad = controller->getPlayersForTeam(managed);
  const auto best = std::ranges::max_element(
      squad, {}, [&](const auto& player)
      { return player.get().getOverall(gamedata->getStatsConfig()); });
  ASSERT_NE(best, squad.end());
  const PlayerID player = best->get().getId();
  ASSERT_TRUE(controller->releasePlayer(player));

  Finances& finances = gamedata->getTeams().at(managed).getFinances();
  finances.setTransferBudget(0);
  ASSERT_EQ(controller->getContractTalkKind(player), ContractKind::FreeAgent);
  const auto offer = TransferNegotiation::demandedOffer(
      controller->getPlayerDemand(player, ContractKind::FreeAgent));
  ASSERT_GE(wageRoom(gamedata->getTeams().at(managed)),
            static_cast<int64_t>(offer.weekly_wage))
      << "the scenario needs a club with room for the wage";
  const auto result = controller->proposeContract(player, offer);
  EXPECT_TRUE(result.response.accepted);
  ASSERT_TRUE(result.completed)
      << "a free signing needs cash, not a fee budget (over budget: "
      << result.over_budget << ")";
  EXPECT_EQ(gamedata->getPlayer(player)->get().getTeamId(), managed);
  EXPECT_EQ(finances.getBalance(), finances.ledgerTotal());
}

TEST_F(TransferMarketTest, SellOnPayoutNeverLeavesANegativeBudget)
{
  controller->saveGame();
  MarketHarness harness(*controller);
  auto gamedata = controller->getGameData();
  const auto& teams = controller->getTeams();
  const TeamID first = teams[10].get().getId();
  const TeamID second = teams[11].get().getId();
  const TeamID third = teams[12].get().getId();
  const PlayerID player =
      controller->getPlayersForTeam(first).front().get().getId();

  TransferMarket::Deal deal;
  deal.player_id = player;
  deal.buyer_id = second;
  deal.terms.fee = 1'000'000;
  deal.terms.sell_on_percent = TransferTuning::Offer::MAX_SELL_ON_PERCENT;
  deal.contract.weekly_wage = 2'000;
  deal.contract.years = 2;
  ASSERT_TRUE(harness.market.completeTransfer(deal, GameDateValue(2025, 7, 5),
                                              FREE_AGENTS_TEAM_ID));
  // Resold with a small upfront part: the 30% sell-on exceeds it.
  Finances& seller = gamedata->getTeams().at(second).getFinances();
  seller.setTransferBudget(0);
  TransferMarket::Deal resale = deal;
  resale.buyer_id = third;
  resale.terms = TransferNegotiation::OfferTerms{};
  resale.terms.fee = 10'000'000;
  resale.terms.upfront_percent = TransferTuning::Offer::MIN_UPFRONT_PERCENT;
  resale.terms.instalment_years = 2;
  ASSERT_TRUE(harness.market.completeTransfer(resale, GameDateValue(2025, 8, 5),
                                              FREE_AGENTS_TEAM_ID));
  EXPECT_GE(seller.getTransferBudget(), 0);
  EXPECT_EQ(seller.getBalance(), seller.ledgerTotal());
}

TEST_F(TransferMarketTest, OpeningBidAtValueIsCounteredThenAgreed)
{
  const TeamID managed = manageFirstClub(*controller);
  auto gamedata = controller->getGameData();
  Finances& finances = gamedata->getTeams().at(managed).getFinances();
  finances.addBalance(500'000'000LL);
  finances.setTransferBudget(500'000'000LL);
  const PlayerID target = findWillingTarget(*controller, managed);
  ASSERT_NE(target, 0u);
  gamedata->getPlayers().at(target).setContractYears(3);

  TransferNegotiation::OfferTerms terms;
  terms.fee = controller->getPlayerMarketValue(target);
  const ClubResponse opening = controller->makeTransferOffer(target, terms);
  ASSERT_EQ(opening.decision, ClubResponse::Decision::Counter)
      << "sellers haggle over a bid at market value";
  EXPECT_GT(opening.counter_fee, terms.fee);
  EXPECT_FALSE(opening.reasons.empty());
  EXPECT_FALSE(controller->getContractTalkKind(target).has_value());

  terms.fee = opening.counter_fee;
  const ClubResponse agreed = controller->makeTransferOffer(target, terms);
  EXPECT_EQ(agreed.decision, ClubResponse::Decision::Accept);
  EXPECT_EQ(controller->getContractTalkKind(target), ContractKind::Transfer);

  // The agent opens above the real demand and softens after a refusal.
  const auto demand =
      controller->getPlayerDemand(target, ContractKind::Transfer);
  EXPECT_GT(demand.asking_wage, demand.weekly_wage);
  TransferNegotiation::ContractOffer lowball =
      TransferNegotiation::demandedOffer(demand);
  lowball.weekly_wage = demand.weekly_wage / 2;
  EXPECT_FALSE(controller->proposeContract(target, lowball).completed);
  EXPECT_LT(controller->getPlayerDemand(target, ContractKind::Transfer)
                .asking_wage,
            demand.asking_wage);
}

TEST_F(TransferMarketTest, TakingChargeLeavesListingsToTheManager)
{
  // The seeded market lists surplus players of every club; once a manager
  // takes charge those decisions are his, so no unsolicited loan offers.
  auto fresh = std::make_unique<GameController>();
  fresh->newGame(1);
  const TeamID club = manageFirstClub(*fresh);
  for (const auto& player : fresh->getPlayersForTeam(club))
  {
    EXPECT_FALSE(marketOf(*fresh).isLoanListed(player.get().getId()));
    EXPECT_FALSE(fresh->isPlayerListed(player.get().getId()));
  }
  for (int day = 0; day < 20; ++day) fresh->advanceDay();
  EXPECT_TRUE(std::ranges::none_of(fresh->getIncomingOffers(),
                                   [](const IncomingOffer& offer)
                                   { return offer.loan; }));
}

TEST_F(TransferMarketTest, EveryClubBudgetKeepsThePayrollReserve)
{
  const TeamID managed = manageFirstClub(*controller);
  auto gamedata = controller->getGameData();
  const TransferMarket& market = marketOf(*controller);
  const GameDateValue today = controller->getCurrentDate();
  for (const TeamID club :
       {managed, controller->getTeams()[7].get().getId()})
  {
    Team& team = gamedata->getTeams().at(club);
    Finances& finances = team.getFinances();
    const std::int64_t payroll =
        finances.getCurrentWageSpending(*gamedata, team);
    ASSERT_GT(payroll, 0);
    const std::int64_t reserve =
        WorldTuning::Finance::CASH_RESERVE_WEEKS * payroll;
    finances.setTransferBudget(500'000'000);

    // A generous board allowance buys nothing while the cash only covers
    // the payroll reserve.
    finances.record(today, FinanceCategory::Investment,
                    reserve - 1 - finances.getBalance());
    EXPECT_EQ(market.spendableBudget(club, today), 0) << team.getName();
    EXPECT_EQ(controller->transferBudgetForTeam(club), 0u) << team.getName();

    // Above the reserve, the spare cash is the limit for AI clubs and the
    // manager alike.
    finances.record(today, FinanceCategory::Investment, 5'000'001);
    const std::int64_t expected =
        5'000'000 - market.committedPayables(club, today);
    EXPECT_EQ(market.spendableBudget(club, today),
              std::max<std::int64_t>(0, expected))
        << team.getName();
    EXPECT_EQ(static_cast<std::int64_t>(controller->transferBudgetForTeam(club)),
              market.spendableBudget(club, today));
  }
}

TEST_F(TransferMarketTest, ReleasingAStarterOnlyFillsHisPlace)
{
  const TeamID managed = manageFirstClub(*controller);
  auto gamedata = controller->getGameData();
  const Lineup& lineup = gamedata->getTeams().at(managed).getLineup();
  ASSERT_GE(lineup.getOutfieldPlayers().size(), 2u);
  ASSERT_FALSE(lineup.getReserves().empty());
  const Lineup::PositionedPlayer leaving = lineup.getOutfieldPlayers()[1];
  const PlayerID released = leaving.player->getId();
  std::vector<PlayerID> others;
  for (const auto& positioned : lineup.getOutfieldPlayers())
    if (positioned.player->getId() != released)
      others.push_back(positioned.player->getId());
  const PlayerID keeper = lineup.getGoalkeeper()->getId();

  ASSERT_TRUE(controller->releasePlayer(released));
  EXPECT_FALSE(lineup.isStarter(released));
  EXPECT_EQ(lineup.getGoalkeeper()->getId(), keeper);
  ASSERT_EQ(lineup.getOutfieldPlayers().size(), others.size() + 1);
  for (const PlayerID id : others) EXPECT_TRUE(lineup.isStarter(id));
  // The replacement takes the departed player's spot on the pitch.
  const auto replacement = std::ranges::find_if(
      lineup.getOutfieldPlayers(), [&](const auto& positioned)
      { return !std::ranges::contains(others, positioned.player->getId()); });
  ASSERT_NE(replacement, lineup.getOutfieldPlayers().end());
  EXPECT_EQ(replacement->position.x, leaving.position.x);
  EXPECT_EQ(replacement->position.y, leaving.position.y);
  EXPECT_EQ(replacement->player->getTeamId(), managed);
  for (const Player* reserve : lineup.getReserves())
  {
    EXPECT_NE(reserve->getId(), released);
    EXPECT_NE(reserve->getId(), replacement->player->getId());
  }
}

TEST_F(TransferMarketTest, TakingChargeWithdrawsTheClubsUnmanagedBids)
{
  // While the market is seeded no club has a manager yet: a bid placed by
  // the club the user then picks must not complete without him.
  auto gamedata = controller->getGameData();
  const auto& teams = controller->getTeams();
  const TeamID chosen = teams[5].get().getId();
  const TeamID seller = teams[6].get().getId();
  gamedata->getTeams().at(chosen).getFinances().addBalance(1'000'000'000LL);
  const PlayerID target =
      controller->getPlayersForTeam(seller).back().get().getId();
  controller->listPlayerForTransfer(target, 1'000'000);
  ASSERT_TRUE(controller->submitBid(target, chosen, 1'200'000));

  controller->selectManagedTeam(chosen);
  const GameDateValue taken_over = controller->getCurrentDate();
  for (const auto& [player_id, listing] : controller->getAllListings())
    EXPECT_NE(listing.highest_bidder_id, std::optional<TeamID>(chosen))
        << "player " << player_id;
  controller->advanceDay();
  EXPECT_NE(gamedata->getPlayer(target)->get().getTeamId(), chosen);
  for (const TransferRecord& record : marketOf(*controller).history())
    if (taken_over < record.date)
      EXPECT_NE(record.to_team, chosen) << "player " << record.player_id;
}

TEST_F(TransferMarketTest, LoansNeverOutliveTheContract)
{
  EXPECT_EQ(TransferNegotiation::contractEndDate(GameDateValue(2026, 6, 10), 1),
            GameDateValue(2026, 6, 30));
  EXPECT_EQ(TransferNegotiation::contractEndDate(GameDateValue(2026, 6, 10), 2),
            GameDateValue(2027, 6, 30));
  EXPECT_EQ(TransferNegotiation::contractEndDate(GameDateValue(2026, 8, 1), 1),
            GameDateValue(2027, 6, 30));

  controller->saveGame();
  MarketHarness harness(*controller);
  auto gamedata = controller->getGameData();
  const auto& teams = controller->getTeams();
  const TeamID parent = teams[5].get().getId();
  const TeamID borrower = teams[6].get().getId();
  const auto& squad = controller->getPlayersForTeam(parent);
  ASSERT_GE(squad.size(), 2u);
  const PlayerID expiring = squad[0].get().getId();
  const PlayerID contracted = squad[1].get().getId();
  gamedata->getPlayers().at(expiring).setContractYears(1);
  gamedata->getPlayers().at(contracted).setContractYears(2);

  // A June loan covers next season: only a player still under contract then
  // may go.
  const TransferNegotiation::LoanTerms terms;
  const GameDateValue june(2026, 6, 10);
  EXPECT_FALSE(harness.market.startLoan(expiring, borrower, terms, june,
                                        FREE_AGENTS_TEAM_ID));
  EXPECT_EQ(gamedata->getPlayer(expiring)->get().getTeamId(), parent);
  ASSERT_TRUE(harness.market.startLoan(contracted, borrower, terms, june,
                                       FREE_AGENTS_TEAM_ID));
  const LoanDeal* loan = harness.market.findLoan(contracted);
  ASSERT_NE(loan, nullptr);
  EXPECT_FALSE(TransferNegotiation::contractEndDate(june, 2) < loan->end);
}

TEST_F(TransferMarketTest, AMoveEndsTheLoanForGood)
{
  controller->saveGame();
  MarketHarness harness(*controller);
  auto gamedata = controller->getGameData();
  const auto& teams = controller->getTeams();
  const TeamID parent = teams[5].get().getId();
  const TeamID borrower = teams[6].get().getId();
  const TeamID next_club = teams[7].get().getId();
  const auto& squad = controller->getPlayersForTeam(parent);
  ASSERT_GE(squad.size(), 2u);
  const PlayerID signed_elsewhere = squad[0].get().getId();
  const PlayerID stranded = squad[1].get().getId();
  TransferNegotiation::LoanTerms terms;
  terms.wage_share = 50;
  const GameDateValue start(2025, 8, 20);
  for (const PlayerID id : {signed_elsewhere, stranded})
  {
    gamedata->getPlayers().at(id).setContractYears(2);
    ASSERT_TRUE(harness.market.startLoan(id, borrower, terms, start,
                                         FREE_AGENTS_TEAM_ID));
  }

  // A pre-contract completing on a loanee ends the loan: its end date must
  // not take him back to the old club.
  TransferMarket::Deal deal;
  deal.player_id = signed_elsewhere;
  deal.buyer_id = next_club;
  deal.kind = TransferKind::PreContract;
  deal.contract.weekly_wage = 10'000;
  deal.contract.years = 2;
  ASSERT_TRUE(
      harness.market.completeTransfer(deal, start + 30, FREE_AGENTS_TEAM_ID));
  EXPECT_EQ(harness.market.findLoan(signed_elsewhere), nullptr);

  // A loanee who left the borrower another way (an old save released him
  // while on loan) no longer costs the parent club a wage share.
  gamedata->getTeams().at(borrower).removePlayerID(stranded);
  gamedata->getTeams().at(FREE_AGENTS_TEAM_ID).addPlayerID(stranded);
  gamedata->transferPlayer(stranded, FREE_AGENTS_TEAM_ID);
  const Finances& parent_finances = gamedata->getTeams().at(parent).getFinances();
  GameDateValue day = start + 31;
  while (dayOrdinal(day) % 7 != 0) day = day + 1;
  const std::size_t entries = parent_finances.getLedger().size();
  harness.market.onDayAdvanced(day, FREE_AGENTS_TEAM_ID);
  EXPECT_EQ(harness.market.findLoan(stranded), nullptr);
  EXPECT_TRUE(harness.market.canBeTraded(stranded));
  for (std::size_t i = entries; i < parent_finances.getLedger().size(); ++i)
    EXPECT_NE(parent_finances.getLedger()[i].category, FinanceCategory::Wages);

  // The season ends: nobody is pulled back.
  harness.market.onDayAdvanced(GameDateValue(2026, 6, 30), FREE_AGENTS_TEAM_ID);
  EXPECT_EQ(gamedata->getPlayer(signed_elsewhere)->get().getTeamId(), next_club);
  EXPECT_EQ(gamedata->getPlayer(stranded)->get().getTeamId(),
            FREE_AGENTS_TEAM_ID);
}

namespace
{
/** Wages a club is committed to next season: contracts that run beyond
 * 30 June and the pre-contracts it has agreed. */
int64_t committedNextSeason(const GameController& controller,
                            const TransferMarket& market, TeamID club)
{
  int64_t wages = 0;
  for (const auto& player : controller.getPlayersForTeam(club))
  {
    if (player.get().getContractYears() > 1 &&
        market.findLoan(player.get().getId()) == nullptr)
      wages += player.get().getWage();
  }
  for (const auto& [id, loan] : market.loans())
  {
    if (loan.parent == club &&
        controller.getGameData()->getPlayer(id)->get().getContractYears() > 1)
      wages += loan.full_wage;
  }
  for (const auto& [id, deal] : market.preContracts())
  {
    if (deal.to_team == club) wages += deal.terms.weekly_wage;
  }
  return wages;
}
}  // namespace

TEST_F(TransferMarketTest, AiPreContractsFitTheBuyersBudgets)
{
  const TeamID managed = manageFirstClub(*controller);
  controller->saveGame();
  MarketHarness harness(*controller);
  auto gamedata = controller->getGameData();
  const std::size_t clubs = controller->getTeams().size();
  const int attempts = TransferTuning::Market::perDay(
      clubs, TransferTuning::Market::DAILY_PRE_CONTRACT_SHARE);
  const GameDateValue first(2026, 1, 1);
  const GameDateValue last(2026, 6, 30);
  WorldRng rng = WorldRng::stream(gamedata->getWorldSeed(),
                                  RngDomain::Transfers, 11, 7);
  for (GameDateValue day = first; !(last < day); day = day + 1)
    harness.market.runAiPreContracts(day, managed, rng, attempts);

  const auto& deals = harness.market.preContracts();
  ASSERT_GT(deals.size(), 50u) << "the market still works";
  std::unordered_map<TeamID, int64_t> costs;
  for (const auto& [player_id, deal] : deals)
  {
    const Team& buyer = gamedata->getTeams().at(deal.to_team);
    const int64_t budget = buyer.getFinances().getWageBudget();
    EXPECT_LE(static_cast<double>(deal.terms.weekly_wage),
              static_cast<double>(budget) *
                  TransferTuning::Market::MAX_SINGLE_WAGE_SHARE)
        << buyer.getName() << " pays one player out of its league";
    costs[deal.to_team] +=
        deal.terms.signing_bonus +
        static_cast<int64_t>(deal.terms.weekly_wage) *
            TransferTuning::Offer::FREE_AGENT_FEE_WEEKS;
  }
  for (const auto& [club, cost] : costs)
  {
    const Team& buyer = gamedata->getTeams().at(club);
    EXPECT_LE(committedNextSeason(*controller, harness.market, club),
              buyer.getFinances().getWageBudget())
        << buyer.getName() << " breaks its wage budget";
    EXPECT_LE(cost, harness.market.spendableBudget(club, last))
        << buyer.getName() << " cannot fund the bonuses and agent fees";
  }
}

TEST_F(TransferMarketTest, FreeAgentsLowerTheirDemandsAndSignWithinMeans)
{
  const TeamID managed = manageFirstClub(*controller);
  controller->saveGame();
  MarketHarness harness(*controller);
  auto gamedata = controller->getGameData();

  // A big club lets its best-paid seniors go.
  const auto richest = std::ranges::max_element(
      controller->getTeams(), {}, [](const auto& team)
      { return team.get().getFinances().getWageBudget(); });
  const TeamID big_club = richest->get().getId();
  ASSERT_NE(big_club, managed);
  std::vector<PlayerID> released;
  for (const auto& player : controller->getPlayersForTeam(big_club))
  {
    if (player.get().getAge() >= 24 && released.size() < 12)
      released.push_back(player.get().getId());
  }
  const GameDateValue august(2025, 8, 20);
  for (const PlayerID id : released)
    ASSERT_TRUE(harness.market.releasePlayer(id, august, managed));

  const PlayerID star = released.front();
  const uint32_t wage = gamedata->getPlayer(star)->get().getWage();
  const TeamID other = controller->getTeams()[3].get().getId();
  const uint32_t demand =
      TransferNegotiation::contractDemand(
          harness.market.playerContext(star, other, ContractKind::FreeAgent))
          .weekly_wage;
  // Two months without a club.
  harness.market.onDayAdvanced(GameDateValue(2025, 9, 1), managed);
  harness.market.onDayAdvanced(GameDateValue(2025, 10, 1), managed);
  const uint32_t lowered = gamedata->getPlayer(star)->get().getWage();
  EXPECT_LT(lowered, wage);
  EXPECT_NEAR(static_cast<double>(lowered),
              wage * TransferTuning::Market::FREE_AGENT_MONTHLY_WAGE_FACTOR *
                  TransferTuning::Market::FREE_AGENT_MONTHLY_WAGE_FACTOR,
              2.0);
  EXPECT_LT(TransferNegotiation::contractDemand(
                harness.market.playerContext(star, other,
                                             ContractKind::FreeAgent))
                .weekly_wage,
            demand);
  EXPECT_GE(lowered, TransferTuning::Contract::MINIMUM_WEEKLY_WAGE);

  // Clubs sign the free agents they can pay, and pay them within their
  // wage budget and the single-player share of it.
  std::unordered_map<TeamID, int64_t> room;
  for (const auto& team : controller->getTeams())
    room[team.get().getId()] =
        team.get().getFinances().getWageBudget() -
        controller->getWeeklyWageBill(team.get().getId());
  WorldRng rng = WorldRng::stream(gamedata->getWorldSeed(),
                                  RngDomain::Transfers, 12, 7);
  const GameDateValue october(2025, 10, 10);
  for (int round = 0; round < 3; ++round)
  {
    for (const auto& team : controller->getTeams())
      harness.market.runAiClub(team.get().getId(), {}, october, managed, rng,
                               true);
  }
  int signed_players = 0;
  std::unordered_map<TeamID, int64_t> added;
  for (const TransferRecord& record : harness.market.history())
  {
    if (record.kind != TransferKind::Free || !(record.date == october))
      continue;
    ++signed_players;
    const Team& club = gamedata->getTeams().at(record.to_team);
    const uint32_t paid =
        gamedata->getPlayer(record.player_id)->get().getWage();
    added[record.to_team] += paid;
    EXPECT_LE(added[record.to_team], room[record.to_team]) << club.getName();
    EXPECT_LE(static_cast<double>(paid),
              static_cast<double>(club.getFinances().getWageBudget()) *
                  TransferTuning::Market::MAX_SINGLE_WAGE_SHARE)
        << club.getName();
  }
  EXPECT_GT(signed_players, 0);
}

TEST_F(TransferMarketTest, RelegationClausesLetPlayersLeaveRelegatedClubs)
{
  const TeamID managed = manageFirstClub(*controller);
  auto gamedata = controller->getGameData();
  // One whole division goes down (the managed club included).
  const LeagueID league = gamedata->getTeams().at(managed).getLeagueId();
  SeasonHistoryEntry entry;
  entry.season = 1;
  entry.start_year = 2025;
  entry.competition_type = MatchType::LEAGUE;
  entry.competition_id = league;
  entry.competition_name = "League";
  std::vector<TeamID> relegated;
  for (const auto& team : controller->getTeams())
  {
    if (team.get().getLeagueId() == league)
      relegated.push_back(team.get().getId());
  }
  entry.relegated = relegated;
  controller->saveGame();
  CompetitionRepository(controller->getDbConn()).saveSeasonHistory({entry});
  MarketHarness harness(*controller);
  Calendar calendar;
  harness.competitions.load(calendar, 2);

  std::unordered_map<TeamID, int64_t> balances;
  std::unordered_map<TeamID, std::vector<PlayerID>> first_team;
  const StatsConfig& config = gamedata->getStatsConfig();
  for (const auto& team : controller->getTeams())
  {
    const TeamID id = team.get().getId();
    balances[id] = team.get().getFinances().getBalance();
    std::vector<std::pair<double, PlayerID>> ranked;
    for (const auto& player : controller->getPlayersForTeam(id))
    {
      if (!player.get().isAcademyPlayer())
        ranked.emplace_back(player.get().getOverall(config),
                            player.get().getId());
    }
    std::ranges::sort(ranked, std::greater<>{});
    for (std::size_t i = 0; i < ranked.size() && i < 16; ++i)
      first_team[id].push_back(ranked[i].second);
  }

  const GameDateValue july(2026, 7, 1);
  harness.market.onDayAdvanced(july, managed);
  std::unordered_map<TeamID, int> exits;
  for (const TransferRecord& record : harness.market.history())
  {
    if (record.kind != TransferKind::Release || !(record.date == july))
      continue;
    ++exits[record.from_team];
    EXPECT_TRUE(std::ranges::contains(relegated, record.from_team))
        << "only relegated clubs lose players this way";
    EXPECT_TRUE(
        std::ranges::contains(first_team[record.from_team], record.player_id))
        << "the clause is for first-team players";
    EXPECT_EQ(record.fee, 0u) << "the clause frees the player at no cost";
    const Player& player = gamedata->getPlayer(record.player_id)->get();
    EXPECT_EQ(player.getTeamId(), FREE_AGENTS_TEAM_ID);
    EXPECT_LE(player.getAge(),
              TransferTuning::Market::RELEGATION_CLAUSE_MAX_AGE);
  }
  EXPECT_EQ(exits[managed], 0) << "the manager's contracts carry no clause";
  int total = 0;
  for (const auto& [club, count] : exits)
  {
    total += count;
    EXPECT_LE(count, TransferTuning::Market::RELEGATION_CLAUSE_MAX_EXITS);
    EXPECT_EQ(gamedata->getTeams().at(club).getFinances().getBalance(),
              balances[club])
        << "no severance is due";
  }
  // ~25% of 16 first-team players at 19 clubs, capped at three a club.
  EXPECT_GT(total, 19);
}
