// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

// Medical centre, calendar, squad planner, player comparison and the set-piece
// dialog through the real GUIView at several window sizes and interface
// scales. Screenshots land in the test runtime's captures folder
// (FM_KEEP_TEST_ARTIFACTS=1 keeps them).

#include <SDL3/SDL.h>
#include <gtest/gtest.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>
#include <string>
#include <tuple>

#include "controller/game_controller.h"
#include "database/gamedata.h"
#include "global/logger.h"
#include "global/runtime_paths.h"
#include "gui/gui_view.h"
#include "gui/scenes/calendar_scene.h"
#include "gui/scenes/lineup_scene.h"
#include "gui/scenes/main_game_scene.h"
#include "gui/scenes/management_scene.h"
#include "gui/scenes/medical_scene.h"
#include "gui/scenes/player_compare_scene.h"
#include "gui/scenes/squad_planner_scene.h"
#include "model/settings_manager.h"

/**
 * The GUI classes grant their internals to this name (the lifecycle test of
 * test_game_flow.cpp, not linked into this executable).
 */
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
  static bool sidebarOverflows(const GUIView& view)
  {
    const auto* shell = dynamic_cast<ManagementScene*>(view.getActiveScene());
    return shell == nullptr || shell->sidebar_nav_overflow;
  }
  static std::size_t injuredLines(const MedicalScene& scene)
  {
    return scene.injured.size();
  }
  static std::size_t riskLines(const MedicalScene& scene)
  {
    return scene.risks.size();
  }
  static std::size_t agendaEntries(const CalendarScene& scene)
  {
    return scene.entries.size();
  }
  static void selectDay(CalendarScene& scene, int day)
  {
    scene.selected_day = day;
  }
  static void selectPlanner(SquadPlannerScene& scene, PlayerID id, int season)
  {
    scene.selected = id;
    scene.season = season;
  }
  static std::size_t plannerRows(const SquadPlannerScene& scene)
  {
    std::size_t rows = 0;
    for (const auto& group : scene.views[0].groups) rows += group.rows.size();
    return rows;
  }
  static const std::vector<ScoutedAttribute>& compared(
      const PlayerCompareScene& scene, std::size_t slot)
  {
    return scene.slots[slot].attributes;
  }
  static void openSetPieces(LineupScene& scene) { scene.set_pieces.open(); }
  static void closeSetPieces(LineupScene& scene) { scene.set_pieces.close(); }
};

using Bridge = GameFlowTest_GUIFlowLifecycle_Test;

namespace
{
constexpr std::uint64_t WORLD_SEED = 20250702;

int uniqueSlot(int offset)
{
  return 800'000 + static_cast<int>(getpid() % 100'000) * 10 + offset;
}

struct SlotCleanup
{
  int slot;
  ~SlotCleanup() { RuntimePaths::removeSave(slot); }
};

void capture(GUIView& view, const std::string& name)
{
  const auto path = RuntimePaths::capturePath(name.c_str());
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

template <typename Scene>
Scene* active(GUIView& view)
{
  return dynamic_cast<Scene*>(Bridge::activeScene(view));
}
}  // namespace

TEST(SquadScreensTest, ScreensRenderAtEverySizeAndScale)
{
  SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
  Logger::init();
  const SlotCleanup slot{uniqueSlot(0)};
  GameController controller;
  controller.newGame(slot.slot, WORLD_SEED);
  const TeamID club = controller.getTeams().front().get().getId();
  controller.selectManagedTeam(club);
  // A few weeks in: friendlies played, loads built up; two injuries.
  for (int day = 0; day < 40; ++day) controller.advanceDay();
  auto& players = controller.getGameData()->getPlayers();
  const auto& squad = controller.getPlayersForTeam(club);
  ASSERT_GE(squad.size(), 16u);
  const PlayerID hamstring = squad[3].get().getId();
  const PlayerID knee = squad[7].get().getId();
  players.at(hamstring).mutableDynamics().injury = InjuryType::HamstringStrain;
  players.at(hamstring).mutableDynamics().injury_days = 18;
  players.at(knee).mutableDynamics().injury = InjuryType::KneeMcl;
  players.at(knee).mutableDynamics().injury_days = 45;
  const PlayerID youngest = std::ranges::min_element(
      squad, {}, [](const auto& player) { return player.get().getAge(); })
                                ->get()
                                .getId();
  ASSERT_TRUE(controller.setSquadStatus(squad[0].get().getId(),
                                        SquadStatus::Star));

  GUIView view(controller);
  ASSERT_TRUE(Bridge::initialize(view));
  view.changeScene(std::make_unique<MainGameScene>(&view));
  frames(view, 2);
  EXPECT_FALSE(Bridge::sidebarOverflows(view))
      << "every sidebar destination fits at 720p";

  Navigation::open(&view, NavSection::MEDICAL);
  frames(view, 3);
  auto* medical = active<MedicalScene>(view);
  ASSERT_NE(medical, nullptr);
  const std::size_t injured = controller.getInjuredPlayers(club).size();
  EXPECT_GE(injured, 2u);
  EXPECT_EQ(Bridge::injuredLines(*medical), injured);
  EXPECT_EQ(Bridge::riskLines(*medical) + injured, squad.size());
  capture(view, "squad_medical.bmp");

  Navigation::open(&view, NavSection::CALENDAR);
  frames(view, 3);
  auto* calendar = active<CalendarScene>(view);
  ASSERT_NE(calendar, nullptr);
  EXPECT_GT(Bridge::agendaEntries(*calendar), 20u);
  capture(view, "squad_calendar.bmp");

  Navigation::open(&view, NavSection::SQUAD_PLANNER);
  frames(view, 3);
  auto* planner = active<SquadPlannerScene>(view);
  ASSERT_NE(planner, nullptr);
  EXPECT_EQ(Bridge::plannerRows(*planner), squad.size());
  Bridge::selectPlanner(*planner, youngest, 0);
  frames(view, 2);
  capture(view, "squad_planner.bmp");
  Bridge::selectPlanner(*planner, 0, 1);
  frames(view, 2);
  capture(view, "squad_planner_next.bmp");

  Navigation::open(&view, NavSection::LINEUP);
  frames(view, 2);
  auto* lineup = active<LineupScene>(view);
  ASSERT_NE(lineup, nullptr);
  Bridge::openSetPieces(*lineup);
  frames(view, 3);
  capture(view, "squad_set_pieces.bmp");
  Bridge::closeSetPieces(*lineup);
  frames(view, 2);
  Navigation::open(&view, NavSection::HOME);
  frames(view, 2);

  // Every screen at small, 4K and HiDPI (scale 2) sizes.
  Settings& settings = SettingsManager::instance()->get();
  const Settings original = settings;
  const TeamID rival = controller.getTeams().back().get().getId();
  const PlayerID foreign =
      controller.getPlayersForTeam(rival).front().get().getId();
  for (const auto& [width, height, scale, tag] :
       {std::tuple{1280, 720, 1.0f, "1280"}, std::tuple{900, 700, 1.0f, "900"},
        std::tuple{3840, 2160, 1.0f, "3840"},
        std::tuple{2560, 1440, 2.0f, "2560_200"}})
  {
    resize(view, width, height);
    settings.ui_scale = scale;
    view.refreshTheme();
    for (const auto& [section, name] :
         {std::pair{NavSection::MEDICAL, "medical"},
          std::pair{NavSection::CALENDAR, "calendar"},
          std::pair{NavSection::SQUAD_PLANNER, "planner"}})
    {
      Navigation::open(&view, section);
      frames(view, 3);
      capture(view, std::string("squad_") + name + "_" + tag + ".bmp");
    }
    Navigation::openCompare(&view, squad[0].get().getId(), foreign);
    frames(view, 3);
    capture(view, std::string("squad_compare_") + tag + ".bmp");
    Navigation::back(&view);
    frames(view, 1);
  }
  settings = original;
  view.refreshTheme();
}

TEST(SquadScreensTest, ComparisonShowsScoutedEstimatesForOtherClubs)
{
  SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
  Logger::init();
  const SlotCleanup slot{uniqueSlot(1)};
  GameController controller;
  controller.newGame(slot.slot, WORLD_SEED);
  const TeamID club = controller.getTeams().front().get().getId();
  controller.selectManagedTeam(club);
  const PlayerID own = controller.getPlayersForTeam(club).front().get().getId();
  const TeamID rival = controller.getTeams().back().get().getId();
  const Player& stranger = controller.getPlayersForTeam(rival).front().get();

  GUIView view(controller);
  ASSERT_TRUE(Bridge::initialize(view));
  view.changeScene(std::make_unique<MainGameScene>(&view));
  frames(view, 2);
  Navigation::openCompare(&view, own, stranger.getId());
  frames(view, 3);
  auto* compare = active<PlayerCompareScene>(view);
  ASSERT_NE(compare, nullptr);

  const auto estimate = controller.getScoutedView(stranger.getId());
  ASSERT_TRUE(estimate.has_value());
  ASSERT_FALSE(estimate->own);
  const auto& shown = Bridge::compared(*compare, 1);
  ASSERT_EQ(shown.size(), estimate->attributes.size());
  bool differs = false;
  for (std::size_t index = 0; index < shown.size(); ++index)
  {
    EXPECT_EQ(shown[index].name, estimate->attributes[index].name);
    EXPECT_FLOAT_EQ(shown[index].estimate, estimate->attributes[index].estimate);
    const auto truth = stranger.getStats().find(shown[index].name);
    ASSERT_NE(truth, stranger.getStats().end());
    differs |= std::abs(truth->second - shown[index].estimate) > 0.5f;
  }
  EXPECT_TRUE(differs) << "an unscouted player's true attributes leaked";
  // Own players are known exactly.
  const auto& mine = Bridge::compared(*compare, 0);
  const Player& ownPlayer = controller.getGameData()->getPlayer(own)->get();
  for (const ScoutedAttribute& attribute : mine)
    EXPECT_NEAR(attribute.estimate, ownPlayer.getStats().at(attribute.name),
                0.01f);
}
