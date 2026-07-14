// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include <gtest/gtest.h>

#include "database/gamedata.h"
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

#include <algorithm>

TEST_F(GameDataTest, TestAddRemovePlayer)
{
  PlayerID pid = 99999;
  TeamID tid = 999;

  Player p(pid, tid, "Test", "Player", "ST", Language::EN, 1000, 0, 20, 2, 180,
           Foot::Right, {});

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

  Player p(pid, source_tid, "Test", "Transfer", "CM", Language::EN, 1000, 0, 20,
           2, 180, Foot::Right, {});
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
