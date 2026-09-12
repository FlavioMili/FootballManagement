// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include <SDL3/SDL.h>
#include <gtest/gtest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
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
#include "global/language_manager.h"
#include "global/logger.h"
#include "global/paths.h"
#include "global/runtime_paths.h"
#include "gui/gui_view.h"
#include "gui/scenes/lineup_scene.h"
#include "gui/scenes/main_game_scene.h"
#include "gui/scenes/main_menu_scene.h"
#include "gui/scenes/match_scene.h"
#include "gui/scenes/roster_scene.h"
#include "gui/scenes/settings_scene.h"
#include "gui/scenes/strategy_scene.h"
#include "gui/scenes/team_selection_scene.h"
#include "gui/scenes/fixtures_scene.h"
#include "gui/scenes/management_scene.h"
#include "gui/scenes/player_profile_scene.h"
#include "gui/scenes/staff_scene.h"
#include "gui/scenes/standings_scene.h"
#include "gui/scenes/training_scene.h"
#include "gui/scenes/transfer_market_scene.h"
#include "gui/widgets/theme.h"
#include "model/settings_manager.h"
#include "model/game.h"
#include "model/player.h"
#include "model/team.h"

namespace
{
constexpr std::chrono::milliseconds MAX_MATCH_SCENE_ENTRY_TIME{100};
constexpr std::chrono::milliseconds MAX_FIRST_HARDWARE_MATCH_RENDER_TIME{100};
constexpr int LIVE_MATCH_WARMUP_FRAMES = 40;
#ifdef DEBUG
constexpr int AI_DEBUG_WARMUP_FRAMES = 160;
#endif
constexpr float LIVE_MATCH_TEST_FRAME_SECONDS = 0.05f;
#ifdef DEBUG
#endif
}  // namespace

class GameFlowTest : public ::testing::Test
{
 protected:
  void SetUp() override
  {
    // Initialize Logger to prevent segfaults when Game or Database try to log
    Logger::init();

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

TEST_F(GameFlowTest, ContinueStopsOnNextManagedMatchday)
{
  const auto teams = controller->getTeams();
  ASSERT_FALSE(teams.empty());
  const TeamID managedTeamId = teams.front().get().getId();
  controller->selectManagedTeam(managedTeamId);

  std::optional<GameDateValue> expectedDate;
  for (const auto& [date, matches] :
       controller->getGame()->getCalendar().getFullCalendar())
  {
    if (date < controller->getCurrentDate()) continue;
    if (std::ranges::any_of(matches,
                            [managedTeamId](const Match& match)
                            {
                              return !match.isPlayed() &&
                                     (match.getHomeTeamId() == managedTeamId ||
                                      match.getAwayTeamId() == managedTeamId);
                            }))
    {
      expectedDate = date;
      break;
    }
  }

  ASSERT_TRUE(expectedDate.has_value());
  const GameDateValue startingDate = controller->getCurrentDate();
  const int advancedDays = controller->advanceToNextManagedFixture();
  EXPECT_GT(advancedDays, 0);
  EXPECT_EQ(controller->getCurrentDate(), *expectedDate);
  EXPECT_FALSE(controller->getCurrentDate() == startingDate);
  EXPECT_EQ(controller->advanceToNextManagedFixture(), 0)
      << "Continue must not skip an unplayed managed fixture";
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
    EXPECT_EQ(ImGui::GetCurrentContext()->ErrorCountCurrentFrame, 0)
        << "ImGui reported a usage error (unbalanced push/pop, bad layout)";
  };

  // 1. Initial frame (Main Menu)
  EXPECT_NO_THROW(step_frame());
  EXPECT_NO_THROW(step_frame());
  const auto mainMenuScreenshotPath = RuntimePaths::capturePath("main_menu.bmp");
  std::filesystem::remove(mainMenuScreenshotPath);
  EXPECT_TRUE(view.captureScreenshot(mainMenuScreenshotPath.string()));

  // 2. Change to Settings
  view.changeScene(std::make_unique<SettingsScene>(&view));
  EXPECT_NO_THROW(step_frame());

  // 3. Change back to Main Menu
  view.changeScene(std::make_unique<MainMenuScene>(&view));
  EXPECT_NO_THROW(step_frame());

  // 4. Change to Main Game Scene
  view.changeScene(std::make_unique<MainGameScene>(&view));
  EXPECT_NO_THROW(step_frame());

  const auto teamSelectionScreenshotPath =
      RuntimePaths::capturePath("team_selection.bmp");
  std::filesystem::remove(teamSelectionScreenshotPath);
  EXPECT_TRUE(view.captureScreenshot(teamSelectionScreenshotPath.string()));
  ASSERT_TRUE(std::filesystem::exists(teamSelectionScreenshotPath));
  EXPECT_GT(std::filesystem::file_size(teamSelectionScreenshotPath), 1'000u);

  // Pop the implicit TeamSelectionScene overlay
  view.popScene();
  EXPECT_NO_THROW(step_frame());

  const auto uiTeams = controller->getTeams();
  ASSERT_FALSE(uiTeams.empty());
  controller->selectManagedTeam(uiTeams.front().get().getId());
  EXPECT_NO_THROW(step_frame());
  ASSERT_NE(view.getActiveScene(), nullptr);
  EXPECT_EQ(view.getActiveScene()->getID(), SceneID::GAME_MENU);
  const auto dashboardScreenshotPath =
      RuntimePaths::capturePath("dashboard.bmp");
  std::filesystem::remove(dashboardScreenshotPath);
  EXPECT_TRUE(view.captureScreenshot(dashboardScreenshotPath.string()));
  ASSERT_TRUE(std::filesystem::exists(dashboardScreenshotPath));
  EXPECT_GT(std::filesystem::file_size(dashboardScreenshotPath), 1'000u);

  auto* dashboard = dynamic_cast<MainGameScene*>(view.getActiveScene());
  ASSERT_NE(dashboard, nullptr);
  dashboard->active_page = MainGameScene::Page::FINANCES;
  view.render();
  const auto financeScreenshotPath = RuntimePaths::capturePath("finances.bmp");
  std::filesystem::remove(financeScreenshotPath);
  EXPECT_TRUE(view.captureScreenshot(financeScreenshotPath.string()));
  ASSERT_TRUE(std::filesystem::exists(financeScreenshotPath));
  EXPECT_GT(std::filesystem::file_size(financeScreenshotPath), 1'000u);
  dashboard->active_page = MainGameScene::Page::OVERVIEW;

  // 5. Roster Scene Overlay
  view.overlayScene(std::make_unique<RosterScene>(&view));
  EXPECT_NO_THROW(step_frame());
  ASSERT_NE(view.getActiveScene(), nullptr);
  EXPECT_EQ(view.getActiveScene()->getID(), SceneID::ROSTER);
  EXPECT_EQ(view.matchFramesTimed, -1);
  auto* rosterScene = dynamic_cast<RosterScene*>(view.getActiveScene());
  ASSERT_NE(rosterScene, nullptr);
  ASSERT_FALSE(rosterScene->roster_players.empty());
  rosterScene->selected_player_id =
      rosterScene->roster_players.front().get().getId();
  view.render();
  const auto rosterScreenshotPath = RuntimePaths::capturePath("roster.bmp");
  std::filesystem::remove(rosterScreenshotPath);
  EXPECT_TRUE(view.captureScreenshot(rosterScreenshotPath.string()));
  ASSERT_TRUE(std::filesystem::exists(rosterScreenshotPath));
  EXPECT_GT(std::filesystem::file_size(rosterScreenshotPath), 1'000u);

  ASSERT_TRUE(LanguageManager::instance().loadLanguage(Language::IT));
  view.render();
  const std::filesystem::path italianRosterScreenshotPath =
      RuntimePaths::capturePath("roster_italian.bmp");
  std::filesystem::remove(italianRosterScreenshotPath);
  EXPECT_TRUE(view.captureScreenshot(italianRosterScreenshotPath.string()));
  ASSERT_TRUE(std::filesystem::exists(italianRosterScreenshotPath));
  EXPECT_GT(std::filesystem::file_size(italianRosterScreenshotPath), 1'000u);
  ASSERT_TRUE(LanguageManager::instance().loadLanguage(Language::EN));

  // Pop Roster Scene
  view.popScene();
  EXPECT_NO_THROW(step_frame());

  // 6. Lineup Scene Overlay
  view.overlayScene(std::make_unique<LineupScene>(&view));
  EXPECT_NO_THROW(step_frame());
  ASSERT_NE(view.getActiveScene(), nullptr);
  EXPECT_EQ(view.getActiveScene()->getID(), SceneID::LINEUP);
  auto* lineupScene = dynamic_cast<LineupScene*>(view.getActiveScene());
  ASSERT_NE(lineupScene, nullptr);
  ASSERT_NE(lineupScene->current_lineup, nullptr);
  ASSERT_FALSE(lineupScene->current_lineup->getOutfieldPlayers().empty());
  ASSERT_FALSE(lineupScene->current_lineup->getReserves().empty());
  lineupScene->selected_pitch_player_id =
      lineupScene->current_lineup->getOutfieldPlayers().front().player->getId();
  lineupScene->selected_bench_player_id =
      lineupScene->current_lineup->getReserves().front()->getId();
  view.render();
  const auto lineupScreenshotPath = RuntimePaths::capturePath("lineup.bmp");
  std::filesystem::remove(lineupScreenshotPath);
  EXPECT_TRUE(view.captureScreenshot(lineupScreenshotPath.string()));
  ASSERT_TRUE(std::filesystem::exists(lineupScreenshotPath));
  EXPECT_GT(std::filesystem::file_size(lineupScreenshotPath), 1'000u);
  view.popScene();
  EXPECT_NO_THROW(step_frame());

  // 7. Strategy Scene Overlay
  view.overlayScene(std::make_unique<StrategyScene>(&view));
  EXPECT_NO_THROW(step_frame());
  const auto tacticsScreenshotPath = RuntimePaths::capturePath("tactics.bmp");
  std::filesystem::remove(tacticsScreenshotPath);
  EXPECT_TRUE(view.captureScreenshot(tacticsScreenshotPath.string()));
  ASSERT_TRUE(std::filesystem::exists(tacticsScreenshotPath));
  EXPECT_GT(std::filesystem::file_size(tacticsScreenshotPath), 1'000u);

  // Pop Strategy Scene
  view.popScene();
  EXPECT_NO_THROW(step_frame());

  // 7b. Management shell: every sidebar section, the player profile, the
  // command palette and in-career settings, captured for visual review.
  const auto captureScreen = [&view](const char* fileName)
  {
    const auto path = RuntimePaths::capturePath(fileName);
    std::filesystem::remove(path);
    EXPECT_TRUE(view.captureScreenshot(path.string()));
    ASSERT_TRUE(std::filesystem::exists(path));
    EXPECT_GT(std::filesystem::file_size(path), 1'000u);
  };
  const auto openSection = [&](NavSection section, SceneID expected)
  {
    Navigation::open(&view, section);
    step_frame();
    step_frame();
    ASSERT_NE(view.getActiveScene(), nullptr);
    EXPECT_EQ(view.getActiveScene()->getID(), expected);
    EXPECT_LE(view.getOverlayDepth(), 1u)
        << "Section navigation must not stack overlays";
  };
  openSection(NavSection::SQUAD, SceneID::ROSTER);
  captureScreen("shell_squad.bmp");
  openSection(NavSection::LINEUP, SceneID::LINEUP);
  captureScreen("shell_lineup.bmp");
  openSection(NavSection::TACTICS, SceneID::STRATEGY);
  captureScreen("shell_tactics.bmp");
  openSection(NavSection::FIXTURES, SceneID::FIXTURES);
  captureScreen("shell_fixtures.bmp");
  auto* fixturesScene = dynamic_cast<FixturesScene*>(view.getActiveScene());
  ASSERT_NE(fixturesScene, nullptr);
  EXPECT_FALSE(fixturesScene->club_fixtures.empty());
  EXPECT_GT(fixturesScene->round_count, 0);
  fixturesScene->view = FixturesScene::View::LEAGUE;
  step_frame();
  captureScreen("shell_fixtures_rounds.bmp");
  openSection(NavSection::STANDINGS, SceneID::STANDINGS);
  auto* standingsScene = dynamic_cast<StandingsScene*>(view.getActiveScene());
  ASSERT_NE(standingsScene, nullptr);
  EXPECT_EQ(standingsScene->table.size(),
            controller->getLeagueById(
                          controller->getManagedTeam()->get().getLeagueId())
                ->get()
                .getTeamIDs()
                .size());
  captureScreen("shell_standings.bmp");
  openSection(NavSection::TRANSFERS, SceneID::TRANSFER_MARKET);
  captureScreen("shell_transfers.bmp");
  openSection(NavSection::INBOX, SceneID::INBOX);
  captureScreen("shell_inbox.bmp");
  openSection(NavSection::CLUB, SceneID::CLUB);
  captureScreen("shell_club.bmp");
  openSection(NavSection::TRAINING, SceneID::TRAINING);
  auto* trainingScene = dynamic_cast<TrainingScene*>(view.getActiveScene());
  ASSERT_NE(trainingScene, nullptr);
  EXPECT_FALSE(trainingScene->rows.empty());
  EXPECT_EQ(trainingScene->week.size(), 7u);
  EXPECT_FALSE(trainingScene->advice.empty());
  captureScreen("shell_training.bmp");
  openSection(NavSection::STAFF, SceneID::STAFF);
  auto* staffScene = dynamic_cast<StaffScene*>(view.getActiveScene());
  ASSERT_NE(staffScene, nullptr);
  EXPECT_FALSE(staffScene->staff_rows.empty());
  EXPECT_FALSE(staffScene->market_rows.empty());
  captureScreen("shell_staff.bmp");
  openSection(NavSection::FINANCES, SceneID::GAME_MENU);
  EXPECT_EQ(view.getOverlayDepth(), 0u);
  captureScreen("shell_finances.bmp");
  openSection(NavSection::HOME, SceneID::GAME_MENU);
  captureScreen("shell_home.bmp");

  // Player profile stacks above the current section and Back returns to it.
  openSection(NavSection::SQUAD, SceneID::ROSTER);
  const PlayerID profiledPlayer =
      controller->getPlayersForTeam(controller->getManagedTeam()->get().getId())
          .front()
          .get()
          .getId();
  Navigation::openPlayer(&view, profiledPlayer);
  step_frame();
  step_frame();
  ASSERT_NE(view.getActiveScene(), nullptr);
  EXPECT_EQ(view.getActiveScene()->getID(), SceneID::PLAYER_PROFILE);
  EXPECT_EQ(view.getOverlayDepth(), 2u);
  captureScreen("shell_profile.bmp");
  Navigation::back(&view);
  step_frame();
  EXPECT_EQ(view.getActiveScene()->getID(), SceneID::ROSTER);

  // A real Escape key press goes back as well (routed shortcut).
  Navigation::openPlayer(&view, profiledPlayer);
  step_frame();
  step_frame();
  ASSERT_EQ(view.getActiveScene()->getID(), SceneID::PLAYER_PROFILE);
  ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, true);
  step_frame();
  ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, false);
  step_frame();
  step_frame();
  EXPECT_EQ(view.getActiveScene()->getID(), SceneID::ROSTER);

  // Command palette with a query, then in-career settings.
  auto* shellScene = dynamic_cast<ManagementScene*>(view.getActiveScene());
  ASSERT_NE(shellScene, nullptr);
  shellScene->openPalette();
  step_frame();
  step_frame();
  for (const char character : std::string_view("ar"))
    ImGui::GetIO().AddInputCharacter(static_cast<unsigned int>(character));
  step_frame();
  step_frame();
  EXPECT_EQ(shellScene->palette_filtered_query, "ar");
  EXPECT_FALSE(shellScene->palette_matches.empty());
  captureScreen("shell_palette.bmp");
  ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, true);
  step_frame();
  ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, false);
  ImGui::GetIO().AddMousePosEvent(640.0f, 400.0f);
  step_frame();
  // Moving the mouse hides the keyboard-navigation frame, as for a user.
  ImGui::GetIO().AddMousePosEvent(660.0f, 420.0f);
  step_frame();
  EXPECT_FALSE(ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId));
  view.navigateTo(std::make_unique<SettingsScene>(&view, true));
  step_frame();
  step_frame();
  EXPECT_EQ(view.getActiveScene()->getID(), SceneID::SETTINGS);
  captureScreen("shell_settings.bmp");
  view.popScene();
  step_frame();
  EXPECT_EQ(view.getActiveScene()->getID(), SceneID::GAME_MENU);

  // Responsive layouts: full HD and a narrow window (collapsed sidebar).
  const auto resizeTo = [&](int width, int height)
  {
    SDL_SetWindowSize(view.getWindow(), width, height);
    step_frame();
    step_frame();
  };
  resizeTo(1920, 1080);
  captureScreen("shell_home_1080p.bmp");
  openSection(NavSection::SQUAD, SceneID::ROSTER);
  captureScreen("shell_squad_1080p.bmp");
  openSection(NavSection::STANDINGS, SceneID::STANDINGS);
  captureScreen("shell_standings_1080p.bmp");
  Navigation::openPlayer(&view, profiledPlayer);
  step_frame();
  step_frame();
  captureScreen("shell_profile_1080p.bmp");
  resizeTo(1024, 700);
  openSection(NavSection::HOME, SceneID::GAME_MENU);
  captureScreen("shell_home_narrow.bmp");
  openSection(NavSection::TRANSFERS, SceneID::TRANSFER_MARKET);
  captureScreen("shell_transfers_narrow.bmp");
  openSection(NavSection::LINEUP, SceneID::LINEUP);
  captureScreen("shell_lineup_narrow.bmp");
  resizeTo(1280, 720);
  openSection(NavSection::HOME, SceneID::GAME_MENU);

  // 8. Render a live match and export a frame for visual/headless debugging.
  const auto teams = controller->getTeams();
  ASSERT_GE(teams.size(), 2u);
  controller->selectManagedTeam(teams[0].get().getId());
  view.overlayScene(std::make_unique<MatchScene>(&view, teams[0].get().getId(),
                                                 teams[1].get().getId()));
  EXPECT_EQ(view.matchFramesTimed, -1);
  const auto matchSceneEntryStart = std::chrono::steady_clock::now();
  view.applyPendingSceneChanges();
  EXPECT_EQ(view.matchFramesTimed, 0);
  const auto matchSceneEntryDuration =
      std::chrono::steady_clock::now() - matchSceneEntryStart;
  const auto matchSceneEntryMicroseconds =
      std::chrono::duration_cast<std::chrono::microseconds>(
          matchSceneEntryDuration)
          .count();
  RecordProperty("match_scene_entry_microseconds", matchSceneEntryMicroseconds);
  EXPECT_LT(matchSceneEntryDuration, MAX_MATCH_SCENE_ENTRY_TIME)
      << "Match scene initialization blocked the UI thread";
  auto* matchScene = dynamic_cast<MatchScene*>(view.getActiveScene());
  ASSERT_NE(matchScene, nullptr);
  const Lineup& managedLineup = teams[0].get().getLineup();
  ASSERT_FALSE(managedLineup.getOutfieldPlayers().empty());
  ASSERT_FALSE(managedLineup.getReserves().empty());
  matchScene->show_substitutions = true;
  matchScene->is_paused = true;
  matchScene->selected_pitch_player =
      managedLineup.getOutfieldPlayers().front().player->getId();
  matchScene->selected_bench_player =
      managedLineup.getReserves().front()->getId();
  view.render();
  view.render();
  const std::filesystem::path substitutionScreenshotPath =
      RuntimePaths::capturePath("substitution.bmp");
  std::filesystem::remove(substitutionScreenshotPath);
  EXPECT_TRUE(view.captureScreenshot(substitutionScreenshotPath.string()));
  ASSERT_TRUE(std::filesystem::exists(substitutionScreenshotPath));
  EXPECT_GT(std::filesystem::file_size(substitutionScreenshotPath), 1'000u);
  matchScene->show_substitutions = false;
  matchScene->is_paused = false;
  view.beginMatchRenderTimings();
  const auto firstMatchRenderStart = std::chrono::steady_clock::now();
  view.render();
  EXPECT_EQ(view.matchFramesTimed, 1);
  const auto firstMatchRenderDuration =
      std::chrono::steady_clock::now() - firstMatchRenderStart;
  const auto firstMatchRenderMicroseconds =
      std::chrono::duration_cast<std::chrono::microseconds>(
          firstMatchRenderDuration)
          .count();
  RecordProperty("first_match_render_microseconds",
                 firstMatchRenderMicroseconds);
  if (!view.rendererIsSoftware)
  {
    EXPECT_LT(firstMatchRenderDuration, MAX_FIRST_HARDWARE_MATCH_RENDER_TIME)
        << "The first hardware-rendered match frame blocked the UI thread";
  }
  for (int frame = 0; frame < LIVE_MATCH_WARMUP_FRAMES; ++frame)
  {
    view.update(LIVE_MATCH_TEST_FRAME_SECONDS);
  }
  const auto screenshotPath = RuntimePaths::capturePath("screenshot.bmp");
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
  const auto debugSnapshotPath = RuntimePaths::capturePath("match.json");
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
  const auto debugScreenshotPath = RuntimePaths::capturePath("ai_debug.bmp");
  std::filesystem::remove(debugScreenshotPath);
  EXPECT_TRUE(view.captureScreenshot(debugScreenshotPath.string()));
  ASSERT_TRUE(std::filesystem::exists(debugScreenshotPath));
  EXPECT_GT(std::filesystem::file_size(debugScreenshotPath), 1'000u);
#endif
}

TEST_F(GameFlowTest, ManagementScreensMidSeason)
{
  SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
  const auto teams = controller->getTeams();
  ASSERT_FALSE(teams.empty());
  const TeamID managedId = teams.front().get().getId();
  controller->selectManagedTeam(managedId);

  // Play several managed fixtures so tables, results, the inbox, the ledger
  // and player statistics hold real data.
  constexpr int MATCHDAYS = 8;
  for (int matchday = 0; matchday < MATCHDAYS; ++matchday)
  {
    controller->advanceToNextManagedFixture();
    const GameDateValue today = controller->getCurrentDate();
    for (const Match& match :
         controller->getGame()->getCalendar().getMatchesForDate(today))
    {
      if (match.isPlayed() || (match.getHomeTeamId() != managedId &&
                               match.getAwayTeamId() != managedId))
        continue;
      const auto home = static_cast<uint8_t>(2 + matchday % 2);
      const auto away = static_cast<uint8_t>(matchday % 3);
      ASSERT_TRUE(controller->setMatchResult(today, match.getHomeTeamId(),
                                             match.getAwayTeamId(), home,
                                             away));
      break;
    }
    controller->advanceDay();
  }

  GUIView view(*controller);
  ASSERT_TRUE(view.initialize());
  const auto step_frame = [&view]()
  {
    view.applyPendingSceneChanges();
    view.handleEvents();
    view.update(0.016f);
    view.render();
    EXPECT_EQ(ImGui::GetCurrentContext()->ErrorCountCurrentFrame, 0)
        << "ImGui reported a usage error (unbalanced push/pop, bad layout)";
  };
  const auto capture = [&view](const char* fileName)
  {
    const auto path = RuntimePaths::capturePath(fileName);
    std::filesystem::remove(path);
    EXPECT_TRUE(view.captureScreenshot(path.string()));
    ASSERT_TRUE(std::filesystem::exists(path));
    EXPECT_GT(std::filesystem::file_size(path), 1'000u);
  };
  view.changeScene(std::make_unique<MainGameScene>(&view));
  step_frame();
  step_frame();
  ASSERT_EQ(view.getActiveScene()->getID(), SceneID::GAME_MENU);
  capture("season_home.bmp");

  const std::array<std::pair<NavSection, const char*>, 9> screens = {{
      {NavSection::INBOX, "season_inbox.bmp"},
      {NavSection::SQUAD, "season_squad.bmp"},
      {NavSection::LINEUP, "season_lineup.bmp"},
      {NavSection::FIXTURES, "season_fixtures.bmp"},
      {NavSection::STANDINGS, "season_standings.bmp"},
      {NavSection::TRANSFERS, "season_transfers.bmp"},
      {NavSection::FINANCES, "season_finances.bmp"},
      {NavSection::CLUB, "season_club.bmp"},
      {NavSection::TACTICS, "season_tactics.bmp"},
  }};
  for (const auto& [section, fileName] : screens)
  {
    Navigation::open(&view, section);
    step_frame();
    step_frame();
    capture(fileName);
  }
  const PlayerID star =
      controller->getPlayersForTeam(managedId).front().get().getId();
  Navigation::openPlayer(&view, star);
  step_frame();
  step_frame();
  EXPECT_EQ(view.getActiveScene()->getID(), SceneID::PLAYER_PROFILE);
  capture("season_profile.bmp");

  // Every dialog renders cleanly: transfer-list confirm, contract renewal
  // and the leave-to-menu confirmation.
  auto* profile = dynamic_cast<PlayerProfileScene*>(view.getActiveScene());
  ASSERT_NE(profile, nullptr);
  profile->list_confirm_requested = true;
  step_frame();
  step_frame();
  capture("season_dialog_list.bmp");
  const auto pressEscape = [&]()
  {
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, true);
    step_frame();
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, false);
    step_frame();
  };
  pressEscape();
  EXPECT_FALSE(ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId));
  EXPECT_EQ(view.getActiveScene()->getID(), SceneID::PLAYER_PROFILE);
  profile->renew_requested = true;
  step_frame();
  step_frame();
  capture("season_dialog_renew.bmp");
  pressEscape();
  EXPECT_FALSE(ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId));
  profile->main_menu_confirm_requested = true;
  step_frame();
  step_frame();
  capture("season_dialog_menu.bmp");
  pressEscape();
  EXPECT_FALSE(ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId));

  // Report of the latest managed result, then the national cup draw.
  const auto fixtures = controller->getTeamFixtures(managedId);
  const auto lastPlayed = std::ranges::find_if(
      fixtures.rbegin(), fixtures.rend(),
      [](const Match& match) { return match.isPlayed(); });
  ASSERT_NE(lastPlayed, fixtures.rend());
  Navigation::openMatchReport(&view, lastPlayed->getDate(),
                              lastPlayed->getHomeTeamId(),
                              lastPlayed->getAwayTeamId());
  step_frame();
  step_frame();
  EXPECT_EQ(view.getActiveScene()->getID(), SceneID::MATCH_REPORT);
  capture("season_report.bmp");

  Navigation::open(&view, NavSection::STANDINGS);
  step_frame();
  auto* standings = dynamic_cast<StandingsScene*>(view.getActiveScene());
  ASSERT_NE(standings, nullptr);
  EXPECT_FALSE(standings->table.empty());
  standings->showing_cup = true;
  standings->refresh();
  step_frame();
  step_frame();
  capture("season_cup.bmp");

  // Appearance options apply live: light and high-contrast palettes,
  // compact density and a 150% interface scale.
  Settings& settings = SettingsManager::instance()->get();
  const Settings original = settings;
  const auto appearance = [&](int preset, bool compact, float scale,
                              const char* home, const char* squad)
  {
    settings.theme_preset = preset;
    settings.compact_density = compact;
    settings.ui_scale = scale;
    view.refreshTheme();
    Navigation::open(&view, NavSection::HOME);
    step_frame();
    step_frame();
    capture(home);
    Navigation::open(&view, NavSection::SQUAD);
    step_frame();
    step_frame();
    capture(squad);
  };
  appearance(static_cast<int>(Theme::Preset::LIGHT), false, 0.0f,
             "theme_light_home.bmp", "theme_light_squad.bmp");
  appearance(static_cast<int>(Theme::Preset::HIGH_CONTRAST), true, 0.0f,
             "theme_contrast_home.bmp", "theme_contrast_squad.bmp");
  appearance(static_cast<int>(Theme::Preset::MIDNIGHT_BLUE), false, 1.5f,
             "theme_scaled_home.bmp", "theme_scaled_squad.bmp");
  EXPECT_FLOAT_EQ(Theme::scale(), 1.5f);
  settings = original;
  view.refreshTheme();
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

TEST_F(GameFlowTest, RuntimeRootDoesNotTouchExternalSentinels)
{
  const auto runtimeRoot = RuntimePaths::root();
  const auto sourceRoot = std::filesystem::path(PROJECT_ROOT);
  EXPECT_NE(runtimeRoot, sourceRoot);

  const auto sentinel =
      runtimeRoot.parent_path() /
      ("football-management-sentinel-" +
       std::to_string(static_cast<unsigned long>(SDL_GetTicks())));
  {
    std::ofstream output(sentinel);
    ASSERT_TRUE(output.is_open());
    output << "do not modify";
  }

  controller->newGame(1);
  std::ifstream input(sentinel);
  std::string contents;
  std::getline(input, contents);
  EXPECT_EQ(contents, "do not modify");
  std::filesystem::remove(sentinel);
}
