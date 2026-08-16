// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include <SDL3/SDL.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>

#if defined(__clang__) || defined(__GNUC__)
extern "C" const char* __lsan_default_suppressions()
{
  return "leak:libSDL3.so\n";
}
#endif

#include "controller/game_controller.h"
#include "database/gamedata.h"
#include "global/logger.h"
#include "gui/gui_view.h"
#include "gui/scenes/main_game_scene.h"
#include "gui/scenes/main_menu_scene.h"
#include "gui/scenes/match_scene.h"
#include "gui/scenes/roster_scene.h"
#include "gui/scenes/settings_scene.h"
#include "gui/scenes/strategy_scene.h"
#include "gui/scenes/team_selection_scene.h"
#include "model/game.h"
#include "model/player.h"
#include "model/team.h"

namespace
{
constexpr std::chrono::milliseconds MAX_MATCH_SCENE_ENTRY_TIME{100};
constexpr std::chrono::milliseconds MAX_FIRST_MATCH_RENDER_TIME{100};
constexpr int LIVE_MATCH_WARMUP_FRAMES = 40;
#ifdef DEBUG
constexpr int AI_DEBUG_WARMUP_FRAMES = 160;
#endif
constexpr float LIVE_MATCH_TEST_FRAME_SECONDS = 0.05f;
constexpr std::string_view MATCH_SCREENSHOT_PATH =
    "/tmp/football_management_screenshot.bmp";
#ifdef DEBUG
constexpr std::string_view AI_DEBUG_SCREENSHOT_PATH =
    "/tmp/football_management_ai_debug.bmp";
constexpr std::string_view MATCH_DEBUG_SNAPSHOT_PATH =
    "/tmp/football_management_match.json";
#endif
}  // namespace

class GameFlowTest : public ::testing::Test
{
 protected:
  void SetUp() override
  {
    // Initialize Logger to prevent segfaults when Game or Database try to log
    Logger::init();

    std::string test_db_path = "test_game_flow.db";
    std::filesystem::remove(test_db_path);
    controller = std::make_unique<GameController>();
    // We cannot easily inject a path into newGame unless we modify it, so for
    // testing we can just call newGame(99) which maps to slot 99
    controller->newGame(99);
  }

  void TearDown() override
  {
    // cleanup
  }

  std::unique_ptr<GameController> controller;
};

TEST_F(GameFlowTest, FullLifecycle)
{
  // 1. Start a New Game
  // We assume there's a valid team ID, e.g., team ID 1.
  // Normally we'd fetch an actual team from the game's team list.
  auto teams = controller->getTeams();
  ASSERT_FALSE(teams.empty()) << "No teams loaded in the database!";

  TeamID firstTeamId = teams.front().get().getId();
  EXPECT_NO_THROW(controller->selectManagedTeam(firstTeamId));

  // 2. Data Access Check
  auto userTeamOpt = controller->getManagedTeam();
  ASSERT_TRUE(userTeamOpt.has_value())
      << "User team should be assigned after selectManagedTeam";

  auto roster = controller->getPlayersForTeam(userTeamOpt->get().getId());
  EXPECT_GT(roster.size(), 0) << "User team should have players in the roster";

  // 3. Time Advancement
  // Simulate advancing a few days
  EXPECT_NO_THROW({
    controller->advanceDay();
    controller->advanceDay();
  });

  // 4. Persistence Check
  constexpr Vector2F CUSTOM_POSITION{0.36f, 0.27f};
  constexpr float CUSTOM_PRESSING = 0.83f;
  Team& managedTeam = controller->getManagedTeam()->get();
  managedTeam.getStrategy().setPressing(CUSTOM_PRESSING);
  ASSERT_FALSE(managedTeam.getLineup().getOutfieldPlayers().empty());
  const PlayerID repositionedPlayerId =
      managedTeam.getLineup().getOutfieldPlayers().front().player->getId();
  ASSERT_TRUE(managedTeam.getLineup().moveOutfieldPlayer(repositionedPlayerId,
                                                         CUSTOM_POSITION));
  EXPECT_NO_THROW({ controller->saveGame(); })
      << "saveGame() should not throw or core dump";

  controller = std::make_unique<GameController>();
  ASSERT_TRUE(controller->loadGame(99));
  EXPECT_EQ(controller->getManagedTeam()->get().getId(), firstTeamId);
  const Team& reloadedTeam = controller->getManagedTeam()->get();
  EXPECT_FLOAT_EQ(reloadedTeam.getStrategy().getSliders().pressing,
                  CUSTOM_PRESSING);
  const auto reloadedPlayer = std::ranges::find_if(
      reloadedTeam.getLineup().getOutfieldPlayers(),
      [repositionedPlayerId](const Lineup::PositionedPlayer& positioned)
      {
        return positioned.player &&
               positioned.player->getId() == repositionedPlayerId;
      });
  ASSERT_NE(reloadedPlayer,
            reloadedTeam.getLineup().getOutfieldPlayers().end());
  EXPECT_FLOAT_EQ(reloadedPlayer->position.x, CUSTOM_POSITION.x);
  EXPECT_FLOAT_EQ(reloadedPlayer->position.y, CUSTOM_POSITION.y);
}

TEST_F(GameFlowTest, GUIFlowLifecycle)
{
  // Enable headless SDL for testing
  SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");

  GUIView view(*controller);
  EXPECT_TRUE(view.initialize());

  auto step_frame = [&view]()
  {
    view.applyPendingSceneChanges();
    view.handleEvents();
    view.update(0.16f);
    view.render();
  };

  // 1. Initial frame (Main Menu)
  EXPECT_NO_THROW(step_frame());

  // 2. Change to Settings
  view.changeScene(std::make_unique<SettingsScene>(&view));
  EXPECT_NO_THROW(step_frame());

  // 3. Change back to Main Menu
  view.changeScene(std::make_unique<MainMenuScene>(&view));
  EXPECT_NO_THROW(step_frame());

  // 4. Change to Main Game Scene
  view.changeScene(std::make_unique<MainGameScene>(&view));
  EXPECT_NO_THROW(step_frame());

  // Pop the implicit TeamSelectionScene overlay
  view.popScene();
  EXPECT_NO_THROW(step_frame());

  // 5. Roster Scene Overlay
  view.overlayScene(std::make_unique<RosterScene>(&view));
  EXPECT_NO_THROW(step_frame());

  // Pop Roster Scene
  view.popScene();
  EXPECT_NO_THROW(step_frame());

  // 6. Strategy Scene Overlay
  view.overlayScene(std::make_unique<StrategyScene>(&view));
  EXPECT_NO_THROW(step_frame());

  // Pop Strategy Scene
  view.popScene();
  EXPECT_NO_THROW(step_frame());

  // 7. Render a live match and export a frame for visual/headless debugging.
  const auto teams = controller->getTeams();
  ASSERT_GE(teams.size(), 2u);
  controller->selectManagedTeam(teams[0].get().getId());
  view.overlayScene(std::make_unique<MatchScene>(&view, teams[0].get().getId(),
                                                 teams[1].get().getId()));
  const auto matchSceneEntryStart = std::chrono::steady_clock::now();
  view.applyPendingSceneChanges();
  const auto matchSceneEntryDuration =
      std::chrono::steady_clock::now() - matchSceneEntryStart;
  const auto matchSceneEntryMicroseconds =
      std::chrono::duration_cast<std::chrono::microseconds>(
          matchSceneEntryDuration)
          .count();
  RecordProperty("match_scene_entry_microseconds", matchSceneEntryMicroseconds);
  EXPECT_LT(matchSceneEntryDuration, MAX_MATCH_SCENE_ENTRY_TIME)
      << "Match scene initialization blocked the UI thread";
  const auto firstMatchRenderStart = std::chrono::steady_clock::now();
  view.render();
  const auto firstMatchRenderDuration =
      std::chrono::steady_clock::now() - firstMatchRenderStart;
  const auto firstMatchRenderMicroseconds =
      std::chrono::duration_cast<std::chrono::microseconds>(
          firstMatchRenderDuration)
          .count();
  RecordProperty("first_match_render_microseconds",
                 firstMatchRenderMicroseconds);
  EXPECT_LT(firstMatchRenderDuration, MAX_FIRST_MATCH_RENDER_TIME)
      << "The first match render blocked the UI thread";
  for (int frame = 0; frame < LIVE_MATCH_WARMUP_FRAMES; ++frame)
  {
    view.update(LIVE_MATCH_TEST_FRAME_SECONDS);
  }
  const std::filesystem::path screenshotPath = MATCH_SCREENSHOT_PATH;
  std::filesystem::remove(screenshotPath);
  view.screenshotPending = true;
  EXPECT_NO_THROW(step_frame());
  ASSERT_TRUE(std::filesystem::exists(screenshotPath));
  EXPECT_GT(std::filesystem::file_size(screenshotPath), 1'000u);

  // Capture the tactical overlay later in the match. This artifact makes AI
  // target churn, duplicated runs and broken defensive spacing inspectable in
  // headless CI as well as during local development. The AI overlay and the
  // machine-readable snapshot are debug-only features, so they are exercised
  // only in Debug builds.
#ifdef DEBUG
  SDL_Event debugEvent{};
  debugEvent.type = SDL_EVENT_KEY_DOWN;
  debugEvent.key.key = SDLK_F10;
  ASSERT_NE(view.getActiveScene(), nullptr);
  view.getActiveScene()->handleEvent(debugEvent);
  for (int frame = 0; frame < AI_DEBUG_WARMUP_FRAMES; ++frame)
  {
    view.update(LIVE_MATCH_TEST_FRAME_SECONDS);
  }
  const std::filesystem::path debugSnapshotPath = MATCH_DEBUG_SNAPSHOT_PATH;
  std::filesystem::remove(debugSnapshotPath);
  SDL_Event exportEvent{};
  exportEvent.type = SDL_EVENT_KEY_DOWN;
  exportEvent.key.key = SDLK_F11;
  view.getActiveScene()->handleEvent(exportEvent);
  ASSERT_TRUE(std::filesystem::exists(debugSnapshotPath));
  std::ifstream snapshotInput(debugSnapshotPath);
  const nlohmann::json debugSnapshot = nlohmann::json::parse(snapshotInput);
  EXPECT_TRUE(debugSnapshot.contains("team_phase"));
  EXPECT_TRUE(
      debugSnapshot["team_phase"].contains("transition_seconds_remaining"));
  EXPECT_TRUE(debugSnapshot.contains("players"));
  EXPECT_TRUE(debugSnapshot.contains("decision"));
  EXPECT_TRUE(debugSnapshot["decision"].contains("reason"));
  EXPECT_FALSE(debugSnapshot["decision"]["reason"].get<std::string>().empty());
  EXPECT_TRUE(debugSnapshot["decision"].contains("analysis"));
  const auto& analysis = debugSnapshot["decision"]["analysis"];
  EXPECT_TRUE(analysis.contains("pass"));
  EXPECT_TRUE(analysis.contains("shot"));
  EXPECT_TRUE(analysis.contains("carry"));
  EXPECT_TRUE(analysis.contains("shield"));
  const auto chosenUtility = [&debugSnapshot, &analysis]() -> float
  {
    const std::string action =
        debugSnapshot["decision"]["action"].get<std::string>();
    if (action == "shot") return analysis["shot"].get<float>();
    if (action == "carry") return analysis["carry"].get<float>();
    if (action == "shield") return analysis["shield"].get<float>();
    return analysis["pass"].get<float>();
  }();
  for (const std::string& key : {"pass", "shot", "carry", "shield"})
  {
    if (analysis[key].is_null()) continue;
    EXPECT_GE(chosenUtility + 1e-4f, analysis[key].get<float>());
  }
  view.render();
  const std::filesystem::path debugScreenshotPath = AI_DEBUG_SCREENSHOT_PATH;
  std::filesystem::remove(debugScreenshotPath);
  EXPECT_TRUE(view.captureScreenshot(debugScreenshotPath.string()));
  ASSERT_TRUE(std::filesystem::exists(debugScreenshotPath));
  EXPECT_GT(std::filesystem::file_size(debugScreenshotPath), 1'000u);
#endif
}

TEST_F(GameFlowTest, SaveSlotMetadata)
{
  // Clean up any potential leftover from a previous run on slot 99
  controller.reset();

  // Re-create controller to ensure clean state
  controller = std::make_unique<GameController>();

  // Check metadata for empty/non-existent slot (e.g. slot 100)
  auto metadata_nonexistent = controller->getSaveSlotMetadata(100);
  EXPECT_FALSE(metadata_nonexistent.exists);

  // Re-create slot 99
  controller->newGame(99);

  // Check metadata for slot 99 (newly created game, no team selected yet)
  auto metadata_new = controller->getSaveSlotMetadata(99);
  EXPECT_TRUE(metadata_new.exists);
  EXPECT_TRUE(metadata_new.team_name.empty());

  // Select team, save, check metadata
  auto teams = controller->getTeams();
  ASSERT_FALSE(teams.empty());
  TeamID testTeamId = teams.front().get().getId();
  std::string testTeamName = teams.front().get().getName();

  controller->selectManagedTeam(testTeamId);
  controller->saveGame();

  auto metadata_saved = controller->getSaveSlotMetadata(99);
  EXPECT_TRUE(metadata_saved.exists);
  EXPECT_EQ(metadata_saved.team_name, testTeamName);
  EXPECT_FALSE(metadata_saved.game_date.empty());
  EXPECT_FALSE(metadata_saved.real_date.empty());
}
