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

class GameDataTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Use unique IDs for tests to not clash with global state
  }
};

TEST_F(GameDataTest, TestAddRemovePlayerO1Lookup) {
  PlayerID pid = 99999;
  TeamID tid = 999;
  
  Player p(pid, tid, "Test", "Player", "ST", Language::EN, 1000, 0, 20, 2, 180, Foot::Right, {});
  
  // Add player
  GameData::instance().addPlayer(pid, p);
  
  auto players = GameData::instance().getPlayersForTeam(tid);
  bool found = false;
  for (const auto& player_ref : players) {
    if (player_ref.get().getId() == pid) {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found) << "Player was not found in O(1) lookup map after addPlayer";
  
  // Remove player
  EXPECT_TRUE(GameData::instance().removePlayer(pid));
  
  players = GameData::instance().getPlayersForTeam(tid);
  found = false;
  for (const auto& player_ref : players) {
    if (player_ref.get().getId() == pid) {
      found = true;
      break;
    }
  }
  EXPECT_FALSE(found) << "Player was still found in O(1) lookup map after removePlayer";
}

TEST_F(GameDataTest, TestTransferPlayerO1Lookup) {
  PlayerID pid = 88888;
  TeamID old_tid = 111;
  TeamID new_tid = 222;
  
  Player p(pid, old_tid, "Test", "Transfer", "CM", Language::EN, 1000, 0, 20, 2, 180, Foot::Right, {});
  GameData::instance().addPlayer(pid, p);
  
  GameData::instance().transferPlayer(pid, new_tid);
  
  auto old_players = GameData::instance().getPlayersForTeam(old_tid);
  bool found_in_old = false;
  for (const auto& player_ref : old_players) {
    if (player_ref.get().getId() == pid) found_in_old = true;
  }
  EXPECT_FALSE(found_in_old) << "Player was still in old team's map after transfer";
  
  auto new_players = GameData::instance().getPlayersForTeam(new_tid);
  bool found_in_new = false;
  for (const auto& player_ref : new_players) {
    if (player_ref.get().getId() == pid) found_in_new = true;
  }
  EXPECT_TRUE(found_in_new) << "Player was not in new team's map after transfer";
  
  EXPECT_EQ(GameData::instance().getPlayer(pid)->get().getTeamId(), new_tid);
  GameData::instance().removePlayer(pid);
}
