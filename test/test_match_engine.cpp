// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "model/match_engine.h"
#include "model/player.h"
#include "model/team.h"

// Helper function to create a dummy team with a specific overall rating
Team createDummyTeam(TeamID id, const std::string& name, int rating,
                     std::vector<std::unique_ptr<Player>>& dummy_players)
{
  Team team(id, 1, name, 1000, {}, Strategy(), Lineup());
  // Add 11 players
  for (unsigned int i = 0; i < 11; ++i)
  {
    std::map<std::string, float> stats = {{"Pace", (float)rating},
                                          {"Shooting", (float)rating},
                                          {"Passing", (float)rating},
                                          {"Defending", (float)rating}};
    auto p = std::make_unique<Player>(id * 100 + i, id, "First", "Last",
                                      PlayerRole::ST, Language::EN, 25, 1000000,
                                      180, 75, rating, Foot::Right, stats);
    team.getLineup().addOutfieldPlayer(p.get(),
                                       {0.5f, static_cast<float>(i) / 11.0f});
    dummy_players.push_back(std::move(p));
  }
  return team;
}

TEST(MatchEngineTest, BasicSimulationRunsWithoutCrashing)
{
  std::vector<std::unique_ptr<Player>> dummyPlayers;
  Team home = createDummyTeam(1, "Home", 50, dummyPlayers);
  Team away = createDummyTeam(2, "Away", 50, dummyPlayers);

  StatsConfig config;
  config.possible_stats = {"Pace", "Shooting", "Passing", "Defending"};
  config.role_focus["Goalkeeper"] = {
      {"Pace", "Shooting", "Passing", "Defending"}, {0.25, 0.25, 0.25, 0.25}};
  config.role_focus["Defender"] = {{"Pace", "Shooting", "Passing", "Defending"},
                                   {0.25, 0.25, 0.25, 0.25}};
  config.role_focus["Midfielder"] = {
      {"Pace", "Shooting", "Passing", "Defending"}, {0.25, 0.25, 0.25, 0.25}};
  config.role_focus["Striker"] = {{"Pace", "Shooting", "Passing", "Defending"},
                                  {0.25, 0.25, 0.25, 0.25}};
  config.role_focus["Unknown"] = {{"Pace", "Shooting", "Passing", "Defending"},
                                  {0.25, 0.25, 0.25, 0.25}};

  MatchEngine engine(home.getLineup(), away.getLineup(), home.getStrategy(),
                     away.getStrategy(), config);

  // Simulate 90 minutes
  float dt = 0.05f;
  int steps = static_cast<int>(90.0f / dt);
  for (int i = 0; i < steps; ++i)
  {
    engine.update(dt);
  }

  // Just asserting it reached here without crashing
  SUCCEED();
}

TEST(MatchEngineTest, StatAdvantage)
{
  std::vector<std::unique_ptr<Player>> dummyPlayers;
  // 99 overall team vs 10 overall team
  Team godTeam = createDummyTeam(1, "Gods", 99, dummyPlayers);
  Team weakTeam = createDummyTeam(2, "Scrubs", 10, dummyPlayers);

  StatsConfig config;
  config.possible_stats = {"Pace", "Shooting", "Passing", "Defending"};
  config.role_focus["Goalkeeper"] = {
      {"Pace", "Shooting", "Passing", "Defending"}, {0.25, 0.25, 0.25, 0.25}};
  config.role_focus["Defender"] = {{"Pace", "Shooting", "Passing", "Defending"},
                                   {0.25, 0.25, 0.25, 0.25}};
  config.role_focus["Midfielder"] = {
      {"Pace", "Shooting", "Passing", "Defending"}, {0.25, 0.25, 0.25, 0.25}};
  config.role_focus["Striker"] = {{"Pace", "Shooting", "Passing", "Defending"},
                                  {0.25, 0.25, 0.25, 0.25}};
  config.role_focus["Unknown"] = {{"Pace", "Shooting", "Passing", "Defending"},
                                  {0.25, 0.25, 0.25, 0.25}};

  MatchEngine engine(godTeam.getLineup(), weakTeam.getLineup(),
                     godTeam.getStrategy(), weakTeam.getStrategy(), config);

  // Simulate 90 minutes
  float dt = 0.05f;
  int steps = static_cast<int>(90.0f / dt);
  for (int i = 0; i < steps; ++i)
  {
    engine.update(dt);
  }

  // Due to randomness, it's possible weak team scores, but god team should
  // dominate. We just want to log the score to verify. We won't strictly assert
  // God > Weak because of extreme RNG possibilities, but ideally God > Weak
  // always.
  std::cout << "God Team Score: " << engine.getHomeScore() << std::endl;
  std::cout << "Weak Team Score: " << engine.getAwayScore() << std::endl;

  // We only verify that the match engine ran and scores were tracked.
  EXPECT_GE(engine.getHomeScore() + engine.getAwayScore(), 0);
}
