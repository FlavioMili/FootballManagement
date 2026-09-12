// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

// The About screen and the crash notice of the main menu, through the real
// GUIView (screenshots in the runtime captures folder).

#include <SDL3/SDL.h>
#include <gtest/gtest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <filesystem>
#include <memory>
#include <string>

#include "controller/game_controller.h"
#include "global/crash_report.h"
#include "global/logger.h"
#include "global/paths.h"
#include "global/runtime_paths.h"
#include "gui/gui_view.h"
#include "gui/scenes/about_scene.h"
#include "gui/scenes/main_menu_scene.h"

namespace fs = std::filesystem;

/**
 * GUIView grants its internals to this name (a fixture of test_game_flow.cpp,
 * not linked into this executable).
 */
class GameFlowTest
{
 public:
  static bool initialize(GUIView& view) { return view.initialize(); }
  static void frame(GUIView& view)
  {
    view.applyPendingSceneChanges();
    view.handleEvents();
    view.update(0.16f);
    view.render();
    EXPECT_EQ(ImGui::GetCurrentContext()->ErrorCountCurrentFrame, 0)
        << "ImGui reported a usage error";
  }
};

TEST(About, EveryCreditHasItsLicenceText)
{
  ASSERT_FALSE(Credits::components().empty());
  for (const Credits::Component& component : Credits::components())
  {
    const fs::path file = AssetPaths::file(
        (std::string("assets/licenses/") + component.file).c_str());
    EXPECT_TRUE(fs::is_regular_file(file)) << file;
    EXPECT_GT(fs::is_regular_file(file) ? fs::file_size(file) : 0U, 100U)
        << file;
  }
}

TEST(About, MainMenuShowsTheCrashNoticeAndOpensAbout)
{
  SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
  Logger::init();
  // A report left by an earlier session.
  CrashReport::write("test: earlier session");
  ASSERT_TRUE(CrashReport::pending().has_value());

  GameController controller;
  GUIView view(controller);
  ASSERT_TRUE(GameFlowTest::initialize(view));
  const auto frame = [&view] { GameFlowTest::frame(view); };
  frame();
  frame();
  EXPECT_TRUE(ImGui::IsPopupOpen("##crash_notice", ImGuiPopupFlags_AnyPopupId |
                                                       ImGuiPopupFlags_AnyPopupLevel));
  EXPECT_TRUE(view.captureScreenshot(
      RuntimePaths::capturePath("main_menu_crash_notice.bmp").string()));
  CrashReport::acknowledge();
  EXPECT_FALSE(CrashReport::pending().has_value());
  ImGui::GetCurrentContext()->OpenPopupStack.resize(0);

  view.changeScene(std::make_unique<AboutScene>(&view, false));
  frame();
  frame();
  ASSERT_NE(view.getTopScene(), nullptr);
  EXPECT_EQ(view.getTopScene()->getID(), SceneID::ABOUT);
  EXPECT_TRUE(view.captureScreenshot(
      RuntimePaths::capturePath("about.bmp").string()));
}
