// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <memory>

#include "controller/game_controller.h"
#include "database/database_connection.h"
#include "database/gamedata.h"
#include "global/logger.h"
#include "global/paths.h"
#include "model/transfer_listing.h"

class TransferMarketTest : public ::testing::Test
{
 protected:
  void SetUp() override
  {
    Logger::init();
    if (std::filesystem::exists(DATABASE_PATH))
    {
      std::filesystem::remove(DATABASE_PATH);
    }

    // Create controller (it will initialize the DB itself using DATABASE_PATH)
    controller = std::make_unique<GameController>();
    controller->newGame(0);
    controller->selectManagedTeam(1);  // Select a dummy team for testing
  }

  void TearDown() override
  {
    controller.reset();
    if (std::filesystem::exists(DATABASE_PATH))
    {
      std::filesystem::remove(DATABASE_PATH);
    }
  }

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
