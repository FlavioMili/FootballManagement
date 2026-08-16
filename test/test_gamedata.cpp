// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include <gtest/gtest.h>

#include <algorithm>

#include "database/gamedata.h"
#include "global/global.h"
#include "model/player.h"

class GameDataTest : public ::testing::Test
{
 protected:
  void SetUp() override
  {
    // Use unique IDs for tests to not clash with global state
  }

  GameData gamedata;
};

TEST_F(GameDataTest, TestAddRemovePlayer)
{
  PlayerID pid = 99999;
  TeamID tid = 999;

  Player p(pid, tid, "Test", "Player", PlayerRole::ST, Language::EN, 1000, 0,
           20, 2, 180, Foot::Right, {});

  // Add player
  gamedata.addPlayer(pid, p);

  auto players = gamedata.getPlayersForTeam(tid);
  bool found = std::ranges::any_of(players, [pid](const auto& player_ref)
                                   { return player_ref.get().getId() == pid; });
  EXPECT_TRUE(found) << "Player was not found after addPlayer";

  // Remove player
  EXPECT_TRUE(gamedata.removePlayer(pid));

  players = gamedata.getPlayersForTeam(tid);
  found = std::ranges::any_of(players, [pid](const auto& player_ref)
                              { return player_ref.get().getId() == pid; });
  EXPECT_FALSE(found) << "Player was still found after removePlayer";
}

TEST_F(GameDataTest, TestTransferPlayer)
{
  PlayerID pid = 88888;
  TeamID source_tid = 111;
  TeamID target_tid = 222;

  Player p(pid, source_tid, "Test", "Transfer", PlayerRole::CM, Language::EN,
           1000, 0, 20, 2, 180, Foot::Right, {});
  gamedata.addPlayer(pid, p);

  gamedata.transferPlayer(pid, target_tid);

  auto source_players = gamedata.getPlayersForTeam(source_tid);
  bool found_in_source =
      std::ranges::any_of(source_players, [pid](const auto& player_ref)
                          { return player_ref.get().getId() == pid; });
  EXPECT_FALSE(found_in_source)
      << "Player was still in source team after transfer";

  auto target_players = gamedata.getPlayersForTeam(target_tid);
  bool found_in_target =
      std::ranges::any_of(target_players, [pid](const auto& player_ref)
                          { return player_ref.get().getId() == pid; });
  EXPECT_TRUE(found_in_target)
      << "Player was not in target team after transfer";

  EXPECT_EQ(gamedata.getPlayer(pid)->get().getTeamId(), target_tid);
  gamedata.removePlayer(pid);
}

TEST_F(GameDataTest, ExpiredContractReleasesPlayerToFreeAgents)
{
  constexpr PlayerID PLAYER_ID = 77777;
  constexpr TeamID SOURCE_TEAM_ID = 333;
  gamedata.addTeam(FREE_AGENTS_TEAM_ID,
                   Team(FREE_AGENTS_TEAM_ID, 0, "Free agents", 0));
  gamedata.addTeam(SOURCE_TEAM_ID,
                   Team(SOURCE_TEAM_ID, 1, "Source", 1'000'000));
  Player player(PLAYER_ID, SOURCE_TEAM_ID, "End", "Contract", PlayerRole::CM,
                Language::EN, 1'000, 0, 27, 1, 180, Foot::Right, {});
  gamedata.addPlayer(PLAYER_ID, player);
  gamedata.getTeams().at(SOURCE_TEAM_ID).addPlayerID(PLAYER_ID);

  const std::vector<PlayerID> released =
      gamedata.advanceContractsAndReleasePlayers();

  EXPECT_TRUE(std::ranges::contains(released, PLAYER_ID));
  ASSERT_TRUE(gamedata.getPlayer(PLAYER_ID).has_value());
  EXPECT_EQ(gamedata.getPlayer(PLAYER_ID)->get().getTeamId(),
            FREE_AGENTS_TEAM_ID);
  EXPECT_EQ(gamedata.getPlayer(PLAYER_ID)->get().getContractYears(), 0);
  EXPECT_FALSE(std::ranges::contains(
      gamedata.getTeams().at(SOURCE_TEAM_ID).getPlayerIDs(), PLAYER_ID));
  EXPECT_TRUE(std::ranges::contains(
      gamedata.getTeams().at(FREE_AGENTS_TEAM_ID).getPlayerIDs(), PLAYER_ID));
}
