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

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"
#include "controller/game_controller.h"
#include "global/language_manager.h"
#include "global/logger.h"
#include "global/paths.h"
#include "global/runtime_paths.h"
#include "gui/gui_view.h"
#include "gui/render/match_camera_3d.h"
#include "gui/render/match_kit_colors.h"
#include "gui/render/match_render_3d_tuning.h"
#include "gui/render/match_render_math.h"
#include "gui/scenes/match_scene.h"

namespace
{
using RenderMath::Projection;
using RenderMath::ScreenPoint;
using RenderMath::Vec3;
using RenderMath::Vec4;

constexpr RenderMath::ScreenRect TEST_RECT{100.0f, 50.0f, 800.0f, 400.0f};
constexpr float TEST_FOV = 0.8f;
constexpr float TEST_NEAR = 0.5f;

Projection testProjection()
{
  return Projection::make({0.0f, -50.0f, 20.0f}, {10.0f, 5.0f, 0.0f}, TEST_FOV,
                          TEST_NEAR, 500.0f, TEST_RECT);
}

MatchCameraFocus focusAt(float x, float y)
{
  MatchCameraFocus focus;
  focus.ball = {x, y, 0.0f};
  focus.carrier = focus.ball;
  focus.hasCarrier = true;
  focus.carrierYaw = 0.3f;
  return focus;
}
}  // namespace

TEST(MatchRender3DMath, CameraTargetProjectsToViewportCentre)
{
  const Projection projection = testProjection();
  ScreenPoint point;
  ASSERT_TRUE(projection.project({10.0f, 5.0f, 0.0f}, point));
  EXPECT_NEAR(point.x, TEST_RECT.x + TEST_RECT.width * 0.5f, 1e-3f);
  EXPECT_NEAR(point.y, TEST_RECT.y + TEST_RECT.height * 0.5f, 1e-3f);
  EXPECT_NEAR(point.depth, RenderMath::length(Vec3{10.0f, 55.0f, -20.0f}),
              1e-3f);
}

TEST(MatchRender3DMath, AxesProjectWithScreenConventions)
{
  const Projection projection = testProjection();
  ScreenPoint centre;
  ScreenPoint right;
  ScreenPoint above;
  ASSERT_TRUE(projection.project({10.0f, 5.0f, 0.0f}, centre));
  // The camera looks roughly along +y, so +x is to the right on screen.
  ASSERT_TRUE(projection.project({15.0f, 5.0f, 0.0f}, right));
  ASSERT_TRUE(projection.project({10.0f, 5.0f, 5.0f}, above));
  EXPECT_GT(right.x, centre.x);
  EXPECT_LT(above.y, centre.y);
}

TEST(MatchRender3DMath, PointsBehindCameraAreRejected)
{
  const Projection projection = testProjection();
  ScreenPoint point;
  EXPECT_FALSE(projection.project({0.0f, -80.0f, 20.0f}, point));
  EXPECT_FALSE(projection.project(projection.eye, point));
}

TEST(MatchRender3DMath, NearPlaneClippingKeepsOnlyVisiblePart)
{
  const Projection projection = testProjection();
  // A ground quad spanning from behind the camera to in front of it.
  const std::array<Vec4, 4> straddling{projection.toClip({-5.0f, -70.0f, 0.0f}),
                                       projection.toClip({5.0f, -70.0f, 0.0f}),
                                       projection.toClip({5.0f, 10.0f, 0.0f}),
                                       projection.toClip({-5.0f, 10.0f, 0.0f})};
  ASSERT_LT(straddling[0].w, TEST_NEAR);
  ASSERT_GT(straddling[2].w, TEST_NEAR);
  std::array<Vec4, 5> clipped{};
  const std::size_t count =
      RenderMath::clipPolygonToNearPlane(straddling, clipped, TEST_NEAR);
  EXPECT_EQ(count, 4u);
  for (std::size_t index = 0; index < count; ++index)
    EXPECT_GE(clipped[index].w, TEST_NEAR - 1e-4f);

  const std::array<Vec4, 3> behind{projection.toClip({0.0f, -60.0f, 0.0f}),
                                   projection.toClip({2.0f, -60.0f, 0.0f}),
                                   projection.toClip({1.0f, -65.0f, 0.0f})};
  EXPECT_EQ(RenderMath::clipPolygonToNearPlane(behind, clipped, TEST_NEAR), 0u);

  const std::array<Vec4, 3> inFront{projection.toClip({0.0f, 0.0f, 0.0f}),
                                    projection.toClip({2.0f, 0.0f, 0.0f}),
                                    projection.toClip({1.0f, 5.0f, 0.0f})};
  EXPECT_EQ(RenderMath::clipPolygonToNearPlane(inFront, clipped, TEST_NEAR),
            3u);
}

TEST(MatchRender3DMath, PitchMappingAndAngles)
{
  const Vec3 origin = RenderMath::worldFromPitch({0.0f, 0.0f});
  const Vec3 corner = RenderMath::worldFromPitch({1.0f, 1.0f});
  EXPECT_FLOAT_EQ(origin.x, 0.0f);
  EXPECT_FLOAT_EQ(origin.y, MatchTuning::Pitch::WIDTH_METRES);
  EXPECT_FLOAT_EQ(corner.x, MatchTuning::Pitch::LENGTH_METRES);
  EXPECT_FLOAT_EQ(corner.y, 0.0f);

  EXPECT_NEAR(RenderMath::worldYawFromFacing(0.0f), 0.0f, 1e-5f);
  EXPECT_NEAR(RenderMath::worldYawFromFacing(std::numbers::pi_v<float> * 0.5f),
              -std::numbers::pi_v<float> * 0.5f, 1e-5f);
  EXPECT_NEAR(std::abs(RenderMath::lerpAngle(3.0f, -3.0f, 0.5f)),
              std::numbers::pi_v<float>, 1e-4f);
}

TEST(MatchCamera3DTest, DampingIsFrameRateIndependent)
{
  for (const MatchCameraMode mode :
       {MatchCameraMode::BROADCAST, MatchCameraMode::TACTICAL,
        MatchCameraMode::END})
  {
    MatchCamera3D coarse;
    MatchCamera3D fine;
    coarse.snap(focusAt(20.0f, 20.0f), mode);
    fine.snap(focusAt(20.0f, 20.0f), mode);
    const MatchCameraFocus moved = focusAt(70.0f, 50.0f);
    coarse.update(moved, mode, 0.0f, 1.0f / 30.0f);
    fine.update(moved, mode, 0.0f, 1.0f / 60.0f);
    fine.update(moved, mode, 0.0f, 1.0f / 60.0f);
    EXPECT_NEAR(coarse.eye().x, fine.eye().x, 1e-3f);
    EXPECT_NEAR(coarse.eye().y, fine.eye().y, 1e-3f);
    EXPECT_NEAR(coarse.eye().z, fine.eye().z, 1e-3f);
  }
}

TEST(MatchCamera3DTest, PresetsConvergeAndStayOutOfTheStands)
{
  MatchCamera3D camera;
  const MatchCameraFocus focus = focusAt(52.5f, 34.0f);
  camera.snap(focus, MatchCameraMode::TACTICAL);
  for (int frame = 0; frame < 600; ++frame)
    camera.update(focus, MatchCameraMode::BROADCAST, 0.0f, 1.0f / 60.0f);
  EXPECT_NEAR(camera.eye().y, MatchRender3DTuning::Broadcast::EYE_Y, 0.05f);
  EXPECT_NEAR(camera.eye().z, MatchRender3DTuning::Broadcast::EYE_HEIGHT,
              0.05f);

  // Switching to the end camera swings round behind the attack.
  for (int frame = 0; frame < 180; ++frame)
    camera.update(focus, MatchCameraMode::END, 0.0f, 1.0f / 60.0f);
  EXPECT_LT(camera.eye().x, camera.target().x - 10.0f);
  EXPECT_NEAR(camera.eye().y, camera.target().y, 0.5f);

  using CameraTuning = MatchRender3DTuning::Camera;
  for (const float ballX : {0.0f, 10.0f, 52.5f, 95.0f, 105.0f})
  {
    for (const float direction : {1.0f, -1.0f})
    {
      MatchCameraFocus endFocus = focusAt(ballX, 10.0f);
      endFocus.attackDirection = direction;
      MatchCamera3D endCamera;
      endCamera.snap(endFocus, MatchCameraMode::END);
      EXPECT_GE(endCamera.eye().x, CameraTuning::EYE_MIN_X - 1e-3f);
      EXPECT_LE(endCamera.eye().x, CameraTuning::EYE_MAX_X + 1e-3f);
      EXPECT_GE(endCamera.eye().z, CameraTuning::MIN_EYE_HEIGHT);
    }
  }

  MatchCamera3D zoomed;
  zoomed.update(focus, MatchCameraMode::BROADCAST, 100.0f, 0.016f);
  EXPECT_FLOAT_EQ(zoomed.zoom(), CameraTuning::MIN_ZOOM);
  zoomed.update(focus, MatchCameraMode::BROADCAST, -100.0f, 0.016f);
  EXPECT_FLOAT_EQ(zoomed.zoom(), CameraTuning::MAX_ZOOM);
}

TEST(MatchKitColorsTest, KitsNeverClash)
{
  for (TeamID home = 1; home <= 80; ++home)
  {
    for (TeamID away = 1; away <= 80; ++away)
    {
      const MatchKits kits = chooseMatchKits(home, away);
      EXPECT_GE(kitColorDistance(kits.home.shirt, kits.away.shirt),
                KIT_CLASH_DISTANCE)
          << home << " vs " << away;
      for (const ImU32 keeper :
           {kits.homeGoalkeeper.shirt, kits.awayGoalkeeper.shirt})
      {
        EXPECT_GE(kitColorDistance(keeper, kits.home.shirt),
                  KIT_CLASH_DISTANCE);
        EXPECT_GE(kitColorDistance(keeper, kits.away.shirt),
                  KIT_CLASH_DISTANCE);
      }
      EXPECT_GE(kitColorDistance(kits.homeGoalkeeper.shirt,
                                 kits.awayGoalkeeper.shirt),
                KIT_CLASH_DISTANCE);
    }
  }
  const MatchKits first = chooseMatchKits(7, 12);
  const MatchKits second = chooseMatchKits(7, 12);
  EXPECT_EQ(first.home.shirt, second.home.shirt);
  EXPECT_EQ(first.away.shirt, second.away.shirt);
}

class MatchRenderer3DSceneTest : public ::testing::Test
{
 protected:
  void SetUp() override
  {
    Logger::init();
    ASSERT_TRUE(LanguageManager::instance().loadLanguage(Language::EN));
    controller = std::make_unique<GameController>();
    controller->newGame(98);
  }

  std::unique_ptr<GameController> controller;
};

TEST_F(MatchRenderer3DSceneTest, SwitchesViewsAndCapturesFrames)
{
  // GUIView is only used as the controller gateway for the scene; it is never
  // initialised and outlives the SDL/ImGui objects created below.
  GUIView view(*controller);
  SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
  ASSERT_TRUE(SDL_Init(SDL_INIT_VIDEO));
  SDL_Window* window = SDL_CreateWindow("3D match view test", 1280, 800, 0);
  ASSERT_NE(window, nullptr);
  SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
  ASSERT_NE(renderer, nullptr);
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  ImGui::CreateContext();
  ImGui::GetIO().IniFilename = nullptr;
  const std::string fontPath =
      std::string(PROJECT_ROOT) + "assets/fonts/font.ttf";
  ASSERT_NE(ImGui::GetIO().Fonts->AddFontFromFileTTF(fontPath.c_str(), 20.0f),
            nullptr);
  ImGui::StyleColorsDark();
  ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
  ImGui_ImplSDLRenderer3_Init(renderer);

  {
    const auto teams = controller->getTeams();
    ASSERT_GE(teams.size(), 2u);
    controller->selectManagedTeam(teams[0].get().getId());
    MatchScene scene(&view, teams[0].get().getId(), teams[1].get().getId());
    scene.onEnter();
    ASSERT_NE(scene.engine, nullptr);

    const auto frame = [&](float seconds)
    {
      ImGui_ImplSDLRenderer3_NewFrame();
      ImGui_ImplSDL3_NewFrame();
      ImGui::NewFrame();
      scene.update(seconds);
      scene.render();
      ImGui::Render();
      SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
      SDL_RenderClear(renderer);
      ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
    };
    const auto press = [&](SDL_Keycode key)
    {
      SDL_Event event{};
      event.type = SDL_EVENT_KEY_DOWN;
      event.key.key = key;
      scene.handleEvent(event);
    };
    const auto capture = [&](const std::string& name)
    {
      const auto path = RuntimePaths::capturePath(name.c_str());
      std::filesystem::remove(path);
      SDL_Surface* surface = SDL_RenderReadPixels(renderer, nullptr);
      ASSERT_NE(surface, nullptr);
      EXPECT_TRUE(SDL_SaveBMP(surface, path.string().c_str()))
          << path.string() << ": " << SDL_GetError();
      SDL_DestroySurface(surface);
      SDL_RenderPresent(renderer);
      ASSERT_TRUE(std::filesystem::exists(path));
      EXPECT_GT(std::filesystem::file_size(path), 1'000u);
      std::cout << "[match-3d] captured " << path.string() << '\n';
    };

    constexpr float FRAME_SECONDS = 1.0f / 60.0f;
    const MatchViewMode initialView = scene.view_mode;
    press(SDLK_V);
    EXPECT_NE(scene.view_mode, initialView);
    if (scene.view_mode != MatchViewMode::BROADCAST_3D) press(SDLK_V);
    ASSERT_EQ(scene.view_mode, MatchViewMode::BROADCAST_3D);
    for (int index = 0; index < 30; ++index) frame(FRAME_SECONDS);

    // Broadcast-camera CPU cost of building the 3D draw lists.
    press(SDLK_1);
    std::vector<float> samples;
    samples.reserve(120);
    for (int index = 0; index < 120; ++index)
    {
      frame(FRAME_SECONDS);
      samples.push_back(scene.last_render_milliseconds);
    }
    std::sort(samples.begin(), samples.end());
    const float median = samples[samples.size() / 2];
    const float p95 = samples[samples.size() * 95 / 100];
    std::cout << "[match-3d] broadcast render CPU median " << median
              << " ms, p95 " << p95 << " ms, draw list "
              << ImGui::GetDrawData()->TotalVtxCount << " vertices\n";
    RecordProperty("render_3d_median_microseconds",
                   static_cast<int>(median * 1000.0f));
    EXPECT_LT(median, 12.0f);
    capture("match_3d_broadcast.bmp");

    const std::array<std::pair<SDL_Keycode, const char*>, 3> cameras{{
        {SDLK_2, "match_3d_tactical.bmp"},
        {SDLK_3, "match_3d_end.bmp"},
        {SDLK_4, "match_3d_follow.bmp"},
    }};
    for (const auto& [key, name] : cameras)
    {
      press(key);
      for (int index = 0; index < 80; ++index) frame(FRAME_SECONDS);
      capture(name);
    }
    EXPECT_EQ(scene.camera_mode, MatchCameraMode::PLAYER_FOLLOW);

    // Zoom extremes: fully out on the tactical camera, fully in on broadcast.
    press(SDLK_2);
    scene.pending_zoom_steps = -30.0f;
    for (int index = 0; index < 80; ++index) frame(FRAME_SECONDS);
    capture("match_3d_tactical_zoom_out.bmp");
    press(SDLK_1);
    scene.pending_zoom_steps = 30.0f;
    for (int index = 0; index < 80; ++index) frame(FRAME_SECONDS);
    capture("match_3d_broadcast_zoom_in.bmp");
    scene.pending_zoom_steps = -30.0f;
    for (int index = 0; index < 80; ++index) frame(FRAME_SECONDS);
    capture("match_3d_broadcast_zoom_out.bmp");

    scene.show_player_names = true;
    for (int index = 0; index < 60; ++index) frame(FRAME_SECONDS);
    capture("match_3d_names.bmp");

    press(SDLK_V);
    EXPECT_EQ(scene.view_mode, MatchViewMode::PITCH_2D);
    frame(FRAME_SECONDS);
    capture("match_2d_after_toggle.bmp");
  }

  ImGui_ImplSDLRenderer3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
}
