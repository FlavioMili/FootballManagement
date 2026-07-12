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

#include "database/database_connection.h"
#include "database/repositories/league_repository.h"
#include "database/repositories/player_repository.h"
#include "database/repositories/team_repository.h"

class DatabaseTest : public ::testing::Test
{
 protected:
  void SetUp() override
  {
    // Use an in-memory database for isolated, fast tests
    db_conn = std::make_shared<DatabaseConnection>(":memory:");
    db_conn->initialize();
  }

  void TearDown() override { db_conn.reset(); }

  std::shared_ptr<DatabaseConnection> db_conn;
};

TEST_F(DatabaseTest, InsertAndLoadPlayer)
{
  PlayerRepository playerRepo(db_conn);

  Player p(1, 10, "Test", "Player", "ST", Language::EN, 1000, 0, 20, 2, 180,
           Foot::Right, {});
  playerRepo.insertPlayer(p);

  auto players = playerRepo.loadAllPlayers();
  ASSERT_EQ(players.size(), 1);
  EXPECT_EQ(players[0].getFirstName(), "Test");
  EXPECT_EQ(players[0].getLastName(), "Player");
  EXPECT_EQ(players[0].getTeamId(), 10);
}

TEST_F(DatabaseTest, UpdatePlayer)
{
  PlayerRepository playerRepo(db_conn);

  Player p(1, 10, "Test", "Player", "ST", Language::EN, 1000, 0, 20, 2, 180,
           Foot::Right, {});
  playerRepo.insertPlayerWithId(p);

  p.setAge(21);
  playerRepo.updatePlayer(p);

  auto players = playerRepo.loadAllPlayers();
  ASSERT_EQ(players.size(), 1);
  EXPECT_EQ(players[0].getAge(), 21);
}

TEST_F(DatabaseTest, DeletePlayer)
{
  PlayerRepository playerRepo(db_conn);

  Player p(1, 10, "Test", "Player", "ST", Language::EN, 1000, 0, 20, 2, 180,
           Foot::Right, {});
  playerRepo.insertPlayerWithId(p);

  playerRepo.deletePlayer(1);

  auto players = playerRepo.loadAllPlayers();
  EXPECT_TRUE(players.empty());
}

TEST_F(DatabaseTest, InsertAndLoadTeam)
{
  TeamRepository teamRepo(db_conn);

  Team t(1, 1, "Test Team", 1000000);
  teamRepo.insertTeamWithId(t);

  auto teams = teamRepo.loadAllTeams();
  ASSERT_EQ(teams.size(), 2);  // Free Agents + Test Team
  bool found = false;
  for (const auto& team : teams)
  {
    if (team.getName() == "Test Team")
    {
      EXPECT_EQ(team.getFinances().getBalance(), 1000000);
      found = true;
    }
  }
  EXPECT_TRUE(found);
}

TEST_F(DatabaseTest, Transactions)
{
  PlayerRepository playerRepo(db_conn);

  db_conn->beginTransaction();
  Player p1(1, 10, "A", "B", "ST", Language::EN, 1000, 0, 20, 2, 180,
            Foot::Right, {});
  Player p2(2, 10, "C", "D", "ST", Language::EN, 1000, 0, 20, 2, 180,
            Foot::Right, {});
  playerRepo.insertPlayerWithId(p1);
  playerRepo.insertPlayerWithId(p2);
  db_conn->rollbackTransaction();

  auto players = playerRepo.loadAllPlayers();
  EXPECT_TRUE(players.empty());

  db_conn->beginTransaction();
  playerRepo.insertPlayerWithId(p1);
  playerRepo.insertPlayerWithId(p2);
  db_conn->commitTransaction();

  players = playerRepo.loadAllPlayers();
  EXPECT_EQ(players.size(), 2);
}
