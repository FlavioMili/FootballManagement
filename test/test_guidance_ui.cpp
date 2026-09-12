// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

// Guidance screens through the real GUIView: Home with the first-week
// checklist and next steps, delegation, opposition report, data hub, the
// inbox decisions and the half-time analysis, at 1280x720, a narrow window
// and UI scale 2. Screenshots land in the test runtime's captures folder
// (FM_KEEP_TEST_ARTIFACTS=1 keeps them).

#include <SDL3/SDL.h>
#include <gtest/gtest.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <unistd.h>

#include <filesystem>
#include <memory>

#include "controller/game_controller.h"
#include "global/logger.h"
#include "global/runtime_paths.h"
#include "gui/gui_view.h"
#include "gui/scenes/data_hub_scene.h"
#include "gui/scenes/main_game_scene.h"
#include "gui/scenes/management_scene.h"
#include "gui/scenes/match_scene.h"
#include "model/match_engine.h"
#include "model/settings_manager.h"

/** GUI classes grant their internals to this name (see test_game_flow). */
class GameFlowTest_GUIFlowLifecycle_Test
{
 public:
  static bool initialize(GUIView& view) { return view.initialize(); }
  static void frame(GUIView& view)
  {
    view.applyPendingSceneChanges();
    view.handleEvents();
    view.update(0.016f);
    view.render();
    EXPECT_EQ(ImGui::GetCurrentContext()->ErrorCountCurrentFrame, 0)
        << "ImGui reported a usage error";
  }
  static GUIScene* activeScene(const GUIView& view)
  {
    return view.getActiveScene();
  }
  static void showPlayers(DataHubScene& scene) { scene.tab = 1; }
  static size_t trendPoints(const DataHubScene& scene)
  {
    return scene.hub.team.trend.size();
  }
};

/** Live match internals (MatchScene grants them to this name). */
class GameFlowTest_ManagedMatchIntegration_Test
{
 public:
  static MatchEngine* engine(MatchScene& scene) { return scene.engine.get(); }
  static bool analysisOpen(const MatchScene& scene)
  {
    return scene.analysis_panel.isOpen();
  }
};

using Bridge = GameFlowTest_GUIFlowLifecycle_Test;
using MatchBridge = GameFlowTest_ManagedMatchIntegration_Test;

namespace
{
constexpr std::uint64_t WORLD_SEED = 20250702;

int uniqueSlot(int offset)
{
  return 410'000 + static_cast<int>(getpid() % 100'000) * 10 + offset;
}

struct SlotCleanup
{
  int slot;
  ~SlotCleanup() { RuntimePaths::removeSave(slot); }
};

void capture(GUIView& view, const char* name)
{
  const auto path = RuntimePaths::capturePath(name);
  std::filesystem::remove(path);
  EXPECT_TRUE(view.captureScreenshot(path.string())) << name;
  EXPECT_TRUE(std::filesystem::exists(path)) << name;
}

void resize(GUIView& view, int width, int height)
{
  SDL_SetWindowSize(view.getWindow(), width, height);
  SDL_Event event{};
  event.type = SDL_EVENT_WINDOW_RESIZED;
  event.window.windowID = SDL_GetWindowID(view.getWindow());
  event.window.data1 = width;
  event.window.data2 = height;
  SDL_PushEvent(&event);
}

void frames(GUIView& view, int count)
{
  for (int index = 0; index < count; ++index) Bridge::frame(view);
}

void setUiScale(GUIView& view, float scale)
{
  SettingsManager::instance()->get().ui_scale = scale;
  view.refreshTheme();
}

/** Plays the managed club's next matches headless, like a quick result. */
int playManagedMatches(GameController& controller, int count)
{
  int played = 0;
  const TeamID club = controller.getManagedTeam()->get().getId();
  for (int attempt = 0; attempt < count * 3 && played < count; ++attempt)
  {
    controller.advanceToNextManagedFixture(60);
    const auto fixture = controller.getNextManagedFixture();
    if (!fixture || !(fixture->date == controller.getCurrentDate())) continue;
    const TeamID home = fixture->home ? club : fixture->opponent;
    const TeamID away = fixture->home ? fixture->opponent : club;
    controller.autoFixLineup(club, fixture->type);
    const Team& home_team = controller.getTeamById(home)->get();
    const Team& away_team = controller.getTeamById(away)->get();
    MatchEngine engine(home_team.getLineup(), away_team.getLineup(),
                       home_team.getStrategy(), away_team.getStrategy(),
                       controller.getStatsConfig(),
                       static_cast<std::uint32_t>(1000 + played));
    engine.simulateToEnd();
    if (!controller.setMatchResult(controller.getCurrentDate(), home, away,
                                   engine))
      break;
    controller.advanceDay();
    ++played;
  }
  return played;
}
}  // namespace

TEST(GuidanceUiTest, GuidanceScreensAtEverySize)
{
  SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
  Logger::init();
  const SlotCleanup slot{uniqueSlot(0)};
  Settings& settings = SettingsManager::instance()->get();
  settings.screen_tips = true;
  settings.screen_tips_seen = 0;
  GameController controller;
  controller.newGame(slot.slot, WORLD_SEED);
  controller.setSimulationThreads(1);
  controller.selectManagedTeam(controller.getTeams().front().get().getId());

  GUIView view(controller);
  ASSERT_TRUE(Bridge::initialize(view));
  resize(view, 1280, 720);
  view.changeScene(std::make_unique<MainGameScene>(&view));
  frames(view, 3);
  EXPECT_TRUE(controller.getOnboarding().isVisible());
  capture(view, "guidance_home.bmp");
  // The Home tip was shown once and does not come back.
  EXPECT_NE(settings.screen_tips_seen, 0U);

  Navigation::open(&view, NavSection::TACTICS);
  frames(view, 2);
  EXPECT_TRUE(controller.getOnboarding().isDone(OnboardingTask::ReviewTactics));

  Navigation::open(&view, NavSection::DELEGATION);
  frames(view, 3);
  capture(view, "guidance_delegation.bmp");

  Navigation::open(&view, NavSection::OPPOSITION);
  frames(view, 3);
  capture(view, "guidance_opposition.bmp");
  const auto fixture = controller.getNextManagedFixture();
  ASSERT_TRUE(fixture.has_value());
  EXPECT_TRUE(controller.wasOppositionReportViewed(fixture->opponent));

  // A few matches for the data hub and the opposition's form.
  Navigation::open(&view, NavSection::HOME);
  frames(view, 1);
  EXPECT_GE(playManagedMatches(controller, 8), 4);
  EXPECT_TRUE(
      controller.getOnboarding().isDone(OnboardingTask::PlayFirstMatch));

  Navigation::open(&view, NavSection::DATA_HUB);
  frames(view, 3);
  auto* hub = dynamic_cast<DataHubScene*>(Bridge::activeScene(view));
  ASSERT_NE(hub, nullptr);
  EXPECT_GE(Bridge::trendPoints(*hub), 4U);
  capture(view, "guidance_data_hub_team.bmp");
  Bridge::showPlayers(*hub);
  frames(view, 2);
  capture(view, "guidance_data_hub_players.bmp");

  Navigation::open(&view, NavSection::INBOX);
  frames(view, 3);
  capture(view, "guidance_inbox.bmp");

  Navigation::open(&view, NavSection::OPPOSITION);
  frames(view, 3);
  capture(view, "guidance_opposition_form.bmp");

  // Narrow window: stacked layouts, no horizontal scrolling.
  resize(view, 900, 700);
  frames(view, 3);
  capture(view, "guidance_opposition_narrow.bmp");
  Navigation::open(&view, NavSection::DATA_HUB);
  frames(view, 3);
  capture(view, "guidance_data_hub_narrow.bmp");

  // UI scale 2 on a 2560x1440 window (a HiDPI desktop at scale 2).
  resize(view, 2560, 1440);
  setUiScale(view, 2.0f);
  frames(view, 3);
  capture(view, "guidance_data_hub_hidpi.bmp");
  Navigation::open(&view, NavSection::HOME);
  frames(view, 3);
  capture(view, "guidance_home_hidpi.bmp");
  Navigation::open(&view, NavSection::DELEGATION);
  frames(view, 3);
  capture(view, "guidance_delegation_hidpi.bmp");
  setUiScale(view, 0.0f);
  resize(view, 1280, 720);
  frames(view, 2);
}

TEST(GuidanceUiTest, HalfTimeAnalysisOpensAtTheBreak)
{
  SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
  Logger::init();
  const SlotCleanup slot{uniqueSlot(1)};
  GameController controller;
  controller.newGame(slot.slot, WORLD_SEED);
  controller.setSimulationThreads(1);
  controller.selectManagedTeam(controller.getTeams().front().get().getId());
  controller.advanceToNextManagedFixture(60);
  const auto fixture = controller.getNextManagedFixture();
  ASSERT_TRUE(fixture.has_value());
  ASSERT_TRUE(fixture->date == controller.getCurrentDate());
  const TeamID club = controller.getManagedTeam()->get().getId();
  const TeamID home = fixture->home ? club : fixture->opponent;
  const TeamID away = fixture->home ? fixture->opponent : club;
  // Talks given beforehand, so their dialogs stay closed.
  controller.giveTeamTalk(TeamTalkMoment::PreMatch, TeamTalkTone::Calm);
  controller.giveTeamTalk(TeamTalkMoment::HalfTime, TeamTalkTone::Calm);

  GUIView view(controller);
  ASSERT_TRUE(Bridge::initialize(view));
  resize(view, 1280, 720);
  view.changeScene(std::make_unique<MatchScene>(&view, home, away));
  frames(view, 2);
  auto* scene = dynamic_cast<MatchScene*>(Bridge::activeScene(view));
  ASSERT_NE(scene, nullptr);
  MatchEngine* engine = MatchBridge::engine(*scene);
  ASSERT_NE(engine, nullptr);
  EXPECT_FALSE(MatchBridge::analysisOpen(*scene));
  for (int step = 0;
       step < 200000 && engine->getState() != MatchState::HALF_TIME; ++step)
    engine->update(0.05f);
  ASSERT_EQ(engine->getState(), MatchState::HALF_TIME);
  frames(view, 3);
  EXPECT_TRUE(MatchBridge::analysisOpen(*scene));
  capture(view, "guidance_half_time_analysis.bmp");
  resize(view, 2560, 1440);
  setUiScale(view, 2.0f);
  frames(view, 3);
  capture(view, "guidance_half_time_analysis_hidpi.bmp");
  setUiScale(view, 0.0f);
}
