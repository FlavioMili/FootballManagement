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
#include "global/logger.h"
#include "model/competition_manager.h"
#include "model/transfer_listing.h"
#include "model/transfer_market.h"
#include "model/transfer_tuning.h"
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
            << " fee_share=" << fee_share << " seconds=" << seconds << "\n";
  EXPECT_GT(moves, 150);
  EXPECT_GT(fee_moves, 0);
  EXPECT_GT(loans, 0);
  EXPECT_GT(free_moves, 0);
  // FIFA 2025: 17.7% of men's professional moves carried a fee.
  EXPECT_GT(fee_share, 0.08);
  EXPECT_LT(fee_share, 0.35);
  for (const auto& team : controller->getTeams())
  {
    EXPECT_EQ(team.get().getFinances().getBalance(),
              team.get().getFinances().ledgerTotal());
    EXPECT_GE(team.get().getPlayerIDs().size(), 18u) << team.get().getName();
  }
}

TEST_F(TransferMarketTest, FreeAgentsNeedCashNotTransferBudget)
{
  const TeamID managed = manageFirstClub(*controller);
  auto gamedata = controller->getGameData();
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
  const auto result = controller->proposeContract(player, offer);
  EXPECT_TRUE(result.response.accepted);
  ASSERT_TRUE(result.completed)
      << "a free signing needs cash, not a fee budget";
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
