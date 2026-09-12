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
#include <optional>
#include <set>
#include <string>

#include "controller/game_controller.h"
#include "database/datagenerator.h"
#include "database/gamedata.h"
#include "global/global.h"
#include "global/logger.h"
#include "global/runtime_paths.h"
#include "model/competition.h"

namespace
{
constexpr std::uint64_t WORLD_SEED = 20250702;
constexpr LeagueID ITALIAN_LOWER_LEAGUE = 6;

int uniqueSlot(int offset)
{
  return 400'000 + static_cast<int>(getpid() % 100'000) * 10 + offset;
}

struct SlotCleanup
{
  int slot;
  ~SlotCleanup() { RuntimePaths::removeSave(slot); }
};
}  // namespace

TEST(WorldDataTest, LeaguesKeepTheirStructure)
{
  const auto leagues = DataGenerator::generateLeagues();
  ASSERT_EQ(leagues.size(), 12u);
  const auto lower =
      std::ranges::find(leagues, ITALIAN_LOWER_LEAGUE, &League::getId);
  ASSERT_NE(lower, leagues.end());
  EXPECT_EQ(lower->getParentLeagueID(), std::optional<LeagueID>{1});
  EXPECT_EQ(lower->getTieBreakRule(), TieBreakRule::HEAD_TO_HEAD);
}

TEST(WorldDataTest, EveryClubHasACompleteIdentity)
{
  const auto teams = DataGenerator::generateTeams();
  const auto identities = DataGenerator::loadClubIdentities();
  ASSERT_EQ(teams.size(), 240u);
  ASSERT_EQ(identities.size(), teams.size());

  std::set<std::string> names;
  std::set<std::string> codes;
  for (const Team& team : teams)
  {
    EXPECT_TRUE(names.insert(team.getName()).second) << team.getName();
    const auto found = identities.find(team.getId());
    ASSERT_NE(found, identities.end()) << team.getName();
    const ClubIdentity& identity = found->second;
    EXPECT_EQ(identity.short_name.size(), 3u) << team.getName();
    EXPECT_TRUE(std::ranges::all_of(identity.short_name, [](char c)
                                    { return c >= 'A' && c <= 'Z'; }))
        << identity.short_name;
    EXPECT_TRUE(codes.insert(identity.short_name).second)
        << identity.short_name;
    EXPECT_NE(identity.primary_colour, identity.secondary_colour);
    EXPECT_LE(identity.primary_colour, 0xFFFFFFu);
    EXPECT_FALSE(identity.stadium_name.empty()) << team.getName();
    EXPECT_FALSE(identity.nickname.empty()) << team.getName();
    EXPECT_GE(identity.stadium_capacity, 2'500u);
    EXPECT_LE(identity.stadium_capacity, 99'000u);
    EXPECT_GE(identity.founded, 1850);
    EXPECT_LE(identity.founded, 2025);
  }
}

TEST(WorldDataTest, NewGameLoadsNamesAndIdentities)
{
  Logger::init();
  const SlotCleanup cleanup{uniqueSlot(0)};
  auto controller = std::make_unique<GameController>();
  controller->newGame(cleanup.slot, WORLD_SEED);
  const GameData& data = *controller->getGameData();

  const auto league = data.getLeague(1);
  ASSERT_TRUE(league.has_value());
  EXPECT_EQ(league->get().getName(), "Italian League");
  EXPECT_EQ(Competitions::cupName(data, 1), "Italian Cup");

  const auto lecce = data.getTeam(103);
  ASSERT_TRUE(lecce.has_value());
  EXPECT_EQ(lecce->get().getName(), "Lecce");
  const auto predefined = data.getPlayer(1001);
  ASSERT_TRUE(predefined.has_value());
  EXPECT_EQ(predefined->get().getTeamId(), 103);

  // The Apulian second division, including Acaya.
  std::set<std::string> lower_names;
  for (const auto& [id, team] : data.getTeams())
  {
    if (team.getLeagueId() == ITALIAN_LOWER_LEAGUE)
      lower_names.insert(team.getName());
  }
  EXPECT_EQ(lower_names.size(), 20u);
  for (const char* town : {"Acaya", "Foggia", "Cerignola", "Taranto", "Otranto"})
    EXPECT_TRUE(lower_names.contains(town)) << town;

  const auto identities = DataGenerator::loadClubIdentities();
  const auto identity = identities.find(103);
  ASSERT_NE(identity, identities.end());
  EXPECT_EQ(identity->second.short_name, "LEC");
  EXPECT_EQ(identity->second.stadium_name, "Stadio Comunale di Lecce");
  EXPECT_NE(identity->second.primary_colour, identity->second.secondary_colour);
  for (const auto& [id, club] : data.getTeams())
  {
    if (id == FREE_AGENTS_TEAM_ID) continue;
    EXPECT_TRUE(identities.contains(id)) << club.getName();
  }
}
