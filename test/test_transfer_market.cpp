// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include <gtest/gtest.h>

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
}
