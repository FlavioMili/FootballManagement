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
#include "database/repositories/fixture_repository.h"
#include "database/repositories/league_repository.h"
#include "database/repositories/player_repository.h"
#include "database/repositories/team_repository.h"
#include "global/logger.h"

class DatabaseTest : public ::testing::Test
{
 protected:
  void SetUp() override
  {
    Logger::init();
    // Use an in-memory database for isolated, fast tests
    db_conn = std::make_shared<DatabaseConnection>(":memory:");
    db_conn->initialize();
  }

  void TearDown() override { db_conn.reset(); }

  std::shared_ptr<DatabaseConnection> getDbConn() const { return db_conn; }

 private:
  std::shared_ptr<DatabaseConnection> db_conn;
};

TEST_F(DatabaseTest, InsertAndLoadPlayer)
{
  PlayerRepository playerRepo(getDbConn());

  Player p(1, 10, "Test", "Player", PlayerRole::ST, Language::EN, 1000, 0, 20,
           2, 180, Foot::Right, {});
  playerRepo.insertPlayer(p);

  auto players = playerRepo.loadAllPlayers();
  ASSERT_EQ(players.size(), 1);
  EXPECT_EQ(players[0].getFirstName(), "Test");
  EXPECT_EQ(players[0].getLastName(), "Player");
  EXPECT_EQ(players[0].getTeamId(), 10);
}

TEST_F(DatabaseTest, UpdatePlayer)
{
  PlayerRepository playerRepo(getDbConn());

  Player p(1, 10, "Test", "Player", PlayerRole::ST, Language::EN, 1000, 0, 20,
           2, 180, Foot::Right, {});
  playerRepo.insertPlayerWithId(p);

  p.setAge(21);
  playerRepo.updatePlayer(p);

  auto players = playerRepo.loadAllPlayers();
  ASSERT_EQ(players.size(), 1);
  EXPECT_EQ(players[0].getAge(), 21);
}

TEST_F(DatabaseTest, DeletePlayer)
{
  PlayerRepository playerRepo(getDbConn());

  Player p(1, 10, "Test", "Player", PlayerRole::ST, Language::EN, 1000, 0, 20,
           2, 180, Foot::Right, {});
  playerRepo.insertPlayerWithId(p);

  playerRepo.deletePlayer(1);

  auto players = playerRepo.loadAllPlayers();
  EXPECT_TRUE(players.empty());
}

TEST_F(DatabaseTest, InsertAndLoadTeam)
{
  TeamRepository teamRepo(getDbConn());

  Team t(1, 1, "Test Team", 1000000);
  StrategySliders savedSliders;
  savedSliders.pressing = 0.81f;
  savedSliders.riskTaking = 0.67f;
  savedSliders.offensiveBias = 0.73f;
  savedSliders.widthUsage = 0.42f;
  savedSliders.compactness = 0.58f;
  t.getStrategy().setAllSliders(savedSliders);
  Player goalkeeper(101, 1, "Test", "Keeper", PlayerRole::GK, Language::EN,
                    1000, 0, 25, 3, 188, Foot::Right, {});
  Player striker(102, 1, "Test", "Striker", PlayerRole::ST, Language::EN, 1000,
                 0, 24, 3, 182, Foot::Right, {});
  Player reserve(103, 1, "Test", "Reserve", PlayerRole::CM, Language::EN, 1000,
                 0, 23, 3, 178, Foot::Right, {});
  constexpr Vector2F SAVED_STRIKER_POSITION{0.79f, 0.46f};
  t.getLineup().setGoalkeeper(&goalkeeper);
  t.getLineup().addOutfieldPlayer(&striker, SAVED_STRIKER_POSITION);
  t.getLineup().setReserves({&reserve});
  teamRepo.insertTeamWithId(t);

  auto teams = teamRepo.loadAllTeams();
  ASSERT_EQ(teams.size(), 2);  // Free Agents + Test Team
  bool found = false;
  for (const auto& team : teams)
  {
    if (team.getName() == "Test Team")
    {
      EXPECT_EQ(team.getFinances().getBalance(), 1000000);
      const StrategySliders loadedSliders = team.getStrategy().getSliders();
      EXPECT_FLOAT_EQ(loadedSliders.pressing, savedSliders.pressing);
      EXPECT_FLOAT_EQ(loadedSliders.riskTaking, savedSliders.riskTaking);
      EXPECT_FLOAT_EQ(loadedSliders.offensiveBias, savedSliders.offensiveBias);
      EXPECT_FLOAT_EQ(loadedSliders.widthUsage, savedSliders.widthUsage);
      EXPECT_FLOAT_EQ(loadedSliders.compactness, savedSliders.compactness);
      found = true;
    }
  }
  EXPECT_TRUE(found);

  const auto storedLineups = teamRepo.loadAllLineups();
  const auto storedLineup = storedLineups.find(t.getId());
  ASSERT_NE(storedLineup, storedLineups.end());
  EXPECT_TRUE(storedLineup->second.persisted);
  EXPECT_EQ(storedLineup->second.goalkeeperId, goalkeeper.getId());
  ASSERT_EQ(storedLineup->second.outfield.size(), 1u);
  EXPECT_EQ(storedLineup->second.outfield.front().playerId, striker.getId());
  EXPECT_FLOAT_EQ(storedLineup->second.outfield.front().position.x,
                  SAVED_STRIKER_POSITION.x);
  EXPECT_FLOAT_EQ(storedLineup->second.outfield.front().position.y,
                  SAVED_STRIKER_POSITION.y);
  ASSERT_EQ(storedLineup->second.reserves.size(), 1u);
  EXPECT_EQ(storedLineup->second.reserves.front(), reserve.getId());
}

TEST_F(DatabaseTest, Transactions)
{
  PlayerRepository playerRepo(getDbConn());

  getDbConn()->beginTransaction();
  Player p1(1, 10, "A", "B", PlayerRole::ST, Language::EN, 1000, 0, 20, 2, 180,
            Foot::Right, {});
  Player p2(2, 10, "C", "D", PlayerRole::ST, Language::EN, 1000, 0, 20, 2, 180,
            Foot::Right, {});
  playerRepo.insertPlayerWithId(p1);
  playerRepo.insertPlayerWithId(p2);
  getDbConn()->rollbackTransaction();

  auto players = playerRepo.loadAllPlayers();
  EXPECT_TRUE(players.empty());

  getDbConn()->beginTransaction();
  playerRepo.insertPlayerWithId(p1);
  playerRepo.insertPlayerWithId(p2);
  getDbConn()->commitTransaction();

  players = playerRepo.loadAllPlayers();
  EXPECT_EQ(players.size(), 2);
}

TEST_F(DatabaseTest, PlayedFixtureRoundTripsWithItsScore)
{
  FixtureRepository fixtureRepo(getDbConn());
  Calendar calendar;
  const GameDateValue matchDate(2025, 9, 14);
  Match match(11, 12, matchDate, MatchType::LEAGUE);
  match.setPlayedResult(3, 2);
  calendar.addMatch(match);

  fixtureRepo.saveCalendar(calendar);

  Calendar loaded;
  fixtureRepo.loadCalendar(loaded);
  const auto& matches = loaded.getMatchesForDate(matchDate);
  ASSERT_EQ(matches.size(), 1u);
  EXPECT_TRUE(matches.front().isPlayed());
  EXPECT_EQ(matches.front().getHomeScore(), 3);
  EXPECT_EQ(matches.front().getAwayScore(), 2);
}
