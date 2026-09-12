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
    coarse.update(moved, mode, {}, 1.0f / 30.0f);
    fine.update(moved, mode, {}, 1.0f / 60.0f);
    fine.update(moved, mode, {}, 1.0f / 60.0f);
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
    camera.update(focus, MatchCameraMode::BROADCAST, {}, 1.0f / 60.0f);
  EXPECT_NEAR(camera.eye().y, MatchRender3DTuning::Broadcast::EYE_Y, 0.05f);
  EXPECT_NEAR(camera.eye().z, MatchRender3DTuning::Broadcast::EYE_HEIGHT,
              0.05f);

  // Switching to the end camera swings round behind the attack.
  for (int frame = 0; frame < 180; ++frame)
    camera.update(focus, MatchCameraMode::END, {}, 1.0f / 60.0f);
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
  MatchCameraControl zoomIn;
  zoomIn.zoomSteps = 100.0f;
  zoomed.update(focus, MatchCameraMode::BROADCAST, zoomIn, 0.016f);
  EXPECT_FLOAT_EQ(zoomed.zoom(), CameraTuning::MIN_ZOOM);
  MatchCameraControl zoomOut;
  zoomOut.zoomSteps = -100.0f;
  zoomed.update(focus, MatchCameraMode::BROADCAST, zoomOut, 0.016f);
  EXPECT_FLOAT_EQ(zoomed.zoom(), CameraTuning::MAX_ZOOM);
}

TEST(MatchRender3DMath, GroundPickingInvertsProjection)
{
  const Projection projection = testProjection();
  for (const Vec3 ground : {Vec3{10.0f, 5.0f, 0.0f}, Vec3{-20.0f, 30.0f, 0.0f},
                            Vec3{40.0f, -10.0f, 0.0f}})
  {
    ScreenPoint screen;
    ASSERT_TRUE(projection.project(ground, screen));
    Vec3 picked;
    ASSERT_TRUE(projection.groundPointAt(screen.x, screen.y, picked));
    EXPECT_NEAR(picked.x, ground.x, 1e-2f);
    EXPECT_NEAR(picked.y, ground.y, 1e-2f);
    EXPECT_NEAR(picked.z, 0.0f, 1e-4f);
  }
  // The top edge of this view looks above the horizon: nothing to pick.
  Vec3 sky;
  EXPECT_FALSE(projection.groundPointAt(TEST_RECT.x, TEST_RECT.y - 400.0f, sky));
}

TEST(MatchCamera3DTest, FreeCameraTakesOverFromThePoseOnScreen)
{
  const MatchCameraFocus focus = focusAt(40.0f, 30.0f);
  MatchCamera3D camera;
  camera.snap(focus, MatchCameraMode::BROADCAST);
  for (int frame = 0; frame < 120; ++frame)
    camera.update(focus, MatchCameraMode::BROADCAST, {}, 1.0f / 60.0f);
  const Vec3 before = camera.eye();
  const float fovBefore = camera.verticalFov();
  camera.update(focus, MatchCameraMode::FREE, {}, 1.0f / 60.0f);
  EXPECT_NEAR(camera.eye().x, before.x, 1e-3f);
  EXPECT_NEAR(camera.eye().y, before.y, 1e-3f);
  EXPECT_NEAR(camera.eye().z, before.z, 1e-3f);
  EXPECT_NEAR(camera.verticalFov(), fovBefore, 1e-5f);

  // Orbiting turns around the same target.
  MatchCameraControl orbit;
  orbit.orbitYaw = 0.6f;
  camera.update(focus, MatchCameraMode::FREE, orbit, 1.0f / 60.0f);
  for (int frame = 0; frame < 120; ++frame)
    camera.update(focus, MatchCameraMode::FREE, {}, 1.0f / 60.0f);
  const Vec3 target = camera.target();
  EXPECT_NEAR(RenderMath::length(camera.eye() - target),
              RenderMath::length(before - target), 0.05f);
  EXPECT_GT(RenderMath::length(camera.eye() - before), 10.0f);
}

TEST(MatchCamera3DTest, FreeCameraClampsPitchZoomAndTarget)
{
  using Free = MatchRender3DTuning::Free;
  const MatchCameraFocus focus = focusAt(52.5f, 34.0f);
  MatchCamera3D camera;
  camera.snap(focus, MatchCameraMode::FREE);

  MatchCameraControl under;
  under.orbitPitch = -10.0f;
  camera.update(focus, MatchCameraMode::FREE, under, 1.0f / 60.0f);
  EXPECT_GE(camera.freePitch(), Free::MIN_PITCH - 1e-5f);
  for (int frame = 0; frame < 240; ++frame)
    camera.update(focus, MatchCameraMode::FREE, {}, 1.0f / 60.0f);
  EXPECT_GE(camera.eye().z, MatchRender3DTuning::Camera::MIN_EYE_HEIGHT);

  MatchCameraControl over;
  over.orbitPitch = 10.0f;
  camera.update(focus, MatchCameraMode::FREE, over, 1.0f / 60.0f);
  EXPECT_LE(camera.freePitch(), Free::MAX_PITCH + 1e-5f);

  MatchCameraControl zoomIn;
  zoomIn.zoomSteps = 200.0f;
  camera.update(focus, MatchCameraMode::FREE, zoomIn, 1.0f / 60.0f);
  EXPECT_FLOAT_EQ(camera.freeDistance(), Free::MIN_DISTANCE);
  MatchCameraControl zoomOut;
  zoomOut.zoomSteps = -200.0f;
  camera.update(focus, MatchCameraMode::FREE, zoomOut, 1.0f / 60.0f);
  EXPECT_FLOAT_EQ(camera.freeDistance(), Free::MAX_DISTANCE);

  // Panning far away stops at the pitch surrounds.
  MatchCameraControl pan;
  pan.pan = {-500.0f, 900.0f, 0.0f};
  camera.update(focus, MatchCameraMode::FREE, pan, 1.0f / 60.0f);
  EXPECT_FLOAT_EQ(camera.freeTarget().x, -Free::TARGET_MARGIN);
  EXPECT_FLOAT_EQ(camera.freeTarget().y,
                  MatchTuning::Pitch::WIDTH_METRES + Free::TARGET_MARGIN);

  // Low, long views never end up inside a stand.
  MatchCameraControl low;
  low.orbitPitch = -10.0f;
  low.reset = true;
  camera.update(focus, MatchCameraMode::FREE, low, 1.0f / 60.0f);
  for (int frame = 0; frame < 600; ++frame)
    camera.update(focus, MatchCameraMode::FREE, zoomOut, 1.0f / 60.0f);
  const Vec3 eye = camera.eye();
  if (eye.z < MatchRender3DTuning::Camera::STAND_CLEAR_HEIGHT)
  {
    EXPECT_GE(eye.x, Free::EYE_MIN_X - 0.1f);
    EXPECT_LE(eye.x, Free::EYE_MAX_X + 0.1f);
    EXPECT_GE(eye.y, Free::EYE_MIN_Y - 0.1f);
    EXPECT_LE(eye.y, Free::EYE_MAX_Y + 0.1f);
  }

  // Re-targeting and following the ball move the orbit centre.
  MatchCameraControl retarget;
  retarget.retarget = true;
  retarget.retargetPoint = {80.0f, 20.0f, 0.0f};
  camera.update(focus, MatchCameraMode::FREE, retarget, 1.0f / 60.0f);
  EXPECT_FLOAT_EQ(camera.freeTarget().x, 80.0f);
  MatchCameraControl follow;
  follow.followBall = true;
  camera.update(focusAt(12.0f, 50.0f), MatchCameraMode::FREE, follow,
                1.0f / 60.0f);
  EXPECT_FLOAT_EQ(camera.freeTarget().x, 12.0f);
  EXPECT_FLOAT_EQ(camera.freeTarget().y, 50.0f);
}

TEST(MatchCamera3DTest, FreeCameraIsFrameRateIndependent)
{
  const MatchCameraFocus focus = focusAt(30.0f, 20.0f);
  MatchCamera3D coarse;
  MatchCamera3D fine;
  coarse.snap(focus, MatchCameraMode::FREE);
  fine.snap(focus, MatchCameraMode::FREE);
  MatchCameraControl input;
  input.orbitYaw = 0.8f;
  input.orbitPitch = 0.3f;
  input.zoomSteps = 2.0f;
  input.pan = {6.0f, -4.0f, 0.0f};
  // The same drag lands in one 30 fps frame or the first of two 60 fps ones.
  coarse.update(focus, MatchCameraMode::FREE, input, 1.0f / 30.0f);
  fine.update(focus, MatchCameraMode::FREE, input, 1.0f / 60.0f);
  fine.update(focus, MatchCameraMode::FREE, {}, 1.0f / 60.0f);
  EXPECT_NEAR(coarse.eye().x, fine.eye().x, 1e-3f);
  EXPECT_NEAR(coarse.eye().y, fine.eye().y, 1e-3f);
  EXPECT_NEAR(coarse.eye().z, fine.eye().z, 1e-3f);
}

TEST(MatchRender3DProportions, WorldIsBuiltAtRealScale)
{
  using T = MatchRender3DTuning;
  EXPECT_FLOAT_EQ(MatchTuning::Pitch::LENGTH_METRES, 105.0f);
  EXPECT_FLOAT_EQ(MatchTuning::Pitch::WIDTH_METRES, 68.0f);
  EXPECT_FLOAT_EQ(T::Goal::WIDTH, 7.32f);
  EXPECT_FLOAT_EQ(T::Goal::HEIGHT, 2.44f);
  EXPECT_FLOAT_EQ(T::Markings::PENALTY_AREA_DEPTH, 16.5f);
  EXPECT_FLOAT_EQ(T::Markings::CIRCLE_RADIUS, 9.15f);
  EXPECT_LE(T::Markings::LINE_WIDTH, 0.12f);
  EXPECT_FLOAT_EQ(T::Ball::RADIUS * 2.0f, 0.22f);
  EXPECT_LE(T::Ball::MAX_BOOST, 2.0f);

  // The reference footballer stands 1.80 m with ~0.5 m across the arms.
  using P = T::Player;
  const float headTop =
      (P::TORSO_BASE + P::TORSO_LENGTH + P::NECK_LENGTH + P::HEAD_RADIUS) *
      P::SCALE;
  EXPECT_NEAR(headTop, P::REFERENCE_HEIGHT_METRES, 0.01f);
  EXPECT_NEAR(P::HIP_HEIGHT - P::THIGH_LENGTH - P::SHIN_LENGTH,
              P::BOOT_HALF_HEIGHT, 0.01f);
  const float span =
      2.0f * (P::SHOULDER_SPREAD + P::UPPER_ARM_HALF_WIDTH) * P::SCALE;
  EXPECT_GE(span, 0.45f);
  EXPECT_LE(span, 0.56f);
  EXPECT_GE(2.0f * P::CHEST_HALF_WIDTH * P::SCALE, 0.34f);
  EXPECT_LE(2.0f * P::CHEST_HALF_WIDTH * P::SCALE, 0.44f);
  EXPECT_LE(P::MIN_HEIGHT_METRES, 1.70f);
  EXPECT_GE(P::MAX_HEIGHT_METRES, 1.95f);

  // Broadcast fields of view stay in the TV range (about 20-35 degrees).
  constexpr float DEGREES = 180.0f / std::numbers::pi_v<float>;
  EXPECT_GE(T::Broadcast::FOV * DEGREES, 20.0f);
  EXPECT_LE(T::Broadcast::FOV * DEGREES, 35.0f);
}

TEST(MatchRender3DProportions, PlayerHeightOnScreenIsBroadcastSized)
{
  // A 1.80 m player at the centre spot, seen by the settled broadcast
  // camera, is neither a giant nor an ant, at 720p and at 1440p.
  MatchCamera3D camera;
  camera.snap(focusAt(52.5f, 34.0f), MatchCameraMode::BROADCAST);
  for (const float height : {720.0f, 1440.0f})
  {
    const RenderMath::ScreenRect rect{0.0f, 0.0f, height * 16.0f / 9.0f,
                                      height};
    const Projection projection = Projection::make(
        camera.eye(), camera.target(), camera.verticalFov(),
        MatchRender3DTuning::Camera::NEAR_PLANE,
        MatchRender3DTuning::Camera::FAR_PLANE, rect);
    ScreenPoint feet;
    ScreenPoint head;
    ASSERT_TRUE(projection.project({52.5f, 34.0f, 0.0f}, feet));
    ASSERT_TRUE(projection.project({52.5f, 34.0f, 1.8f}, head));
    const float pixels = feet.y - head.y;
    std::cout << "[match-3d] 1.80 m player at the centre spot: " << pixels
              << " px of " << height << '\n';
    EXPECT_GT(pixels / height, 0.035f);
    EXPECT_LT(pixels / height, 0.09f);

    // The goal mouth at the far end reads as 7.32 x 2.44 m (3:1).
    ScreenPoint postBottom;
    ScreenPoint postTop;
    ASSERT_TRUE(projection.project({0.0f, 34.0f, 0.0f}, postBottom));
    ASSERT_TRUE(projection.project({0.0f, 34.0f, 2.44f}, postTop));
    EXPECT_GT(postBottom.y - postTop.y, pixels * 0.6f);
  }
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
  const std::string fontPath = AssetPaths::font();
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
    // Starts every run from the defaults (the scene remembers the last
    // presentation for the session).
    scene.setSidePanelsHidden(false);
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
    scene.show_player_names = false;

    // Pitch focus: the view takes the whole window under an overlay HUD.
    const auto medianRenderMilliseconds = [&](int frames)
    {
      std::vector<float> timings;
      timings.reserve(static_cast<std::size_t>(frames));
      for (int index = 0; index < frames; ++index)
      {
        frame(FRAME_SECONDS);
        timings.push_back(scene.last_render_milliseconds);
      }
      std::sort(timings.begin(), timings.end());
      return timings[timings.size() / 2];
    };
    press(SDLK_1);
    press(SDLK_F);
    EXPECT_TRUE(scene.pitch_focus);
    const float focusMedian = medianRenderMilliseconds(90);
    std::cout << "[match-3d] focus 1280x800 broadcast render CPU median "
              << focusMedian << " ms, draw list "
              << ImGui::GetDrawData()->TotalVtxCount << " vertices\n";
    capture("match_3d_focus_broadcast.bmp");
    press(SDLK_2);
    for (int index = 0; index < 80; ++index) frame(FRAME_SECONDS);
    capture("match_3d_focus_tactical.bmp");

    // A left drag over the view hands the camera to the free orbit camera.
    press(SDLK_1);
    for (int index = 0; index < 60; ++index) frame(FRAME_SECONDS);
    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 centre(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.6f);
    io.AddMousePosEvent(centre.x, centre.y);
    frame(FRAME_SECONDS);
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    frame(FRAME_SECONDS);
    for (int step = 1; step <= 20; ++step)
    {
      io.AddMousePosEvent(centre.x + static_cast<float>(step) * 12.0f,
                          centre.y - static_cast<float>(step) * 4.0f);
      frame(FRAME_SECONDS);
    }
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    frame(FRAME_SECONDS);
    EXPECT_EQ(scene.camera_mode, MatchCameraMode::FREE);
    for (int index = 0; index < 60; ++index) frame(FRAME_SECONDS);
    capture("match_3d_focus_free_orbit.bmp");
    // Wheel zoom and a right-drag pan over the view.
    io.AddMouseWheelEvent(0.0f, 4.0f);
    frame(FRAME_SECONDS);
    io.AddMouseButtonEvent(ImGuiMouseButton_Right, true);
    frame(FRAME_SECONDS);
    for (int step = 1; step <= 15; ++step)
    {
      io.AddMousePosEvent(centre.x + 240.0f - static_cast<float>(step) * 10.0f,
                          centre.y - 80.0f + static_cast<float>(step) * 6.0f);
      frame(FRAME_SECONDS);
    }
    io.AddMouseButtonEvent(ImGuiMouseButton_Right, false);
    for (int index = 0; index < 60; ++index) frame(FRAME_SECONDS);
    capture("match_3d_focus_free_pan_zoom.bmp");
    press(SDLK_B);
    EXPECT_TRUE(scene.free_follow_ball);
    press(SDLK_R);
    for (int index = 0; index < 90; ++index) frame(FRAME_SECONDS);
    capture("match_3d_focus_free_reset.bmp");
    io.AddMousePosEvent(-1000.0f, -1000.0f);

    // A HiDPI-sized window (2560x1440 pixels at scale 1).
    SDL_SetWindowSize(window, 2560, 1440);
    SDL_PumpEvents();
    press(SDLK_1);
    for (int index = 0; index < 90; ++index) frame(FRAME_SECONDS);
    const float largeMedian = medianRenderMilliseconds(90);
    std::cout << "[match-3d] focus 2560x1440 broadcast render CPU median "
              << largeMedian << " ms, draw list "
              << ImGui::GetDrawData()->TotalVtxCount << " vertices\n";
    RecordProperty("render_3d_focus_1440p_median_microseconds",
                   static_cast<int>(largeMedian * 1000.0f));
    capture("match_3d_focus_1440p_broadcast.bmp");
    press(SDLK_4);
    for (int index = 0; index < 90; ++index) frame(FRAME_SECONDS);
    capture("match_3d_focus_1440p_follow.bmp");
    press(SDLK_ESCAPE);
    EXPECT_FALSE(scene.pitch_focus);
    press(SDLK_1);
    for (int index = 0; index < 60; ++index) frame(FRAME_SECONDS);
    capture("match_3d_1440p_panels.bmp");
    scene.setSidePanelsHidden(true);
    for (int index = 0; index < 30; ++index) frame(FRAME_SECONDS);
    capture("match_3d_1440p_panels_hidden.bmp");
    scene.setSidePanelsHidden(false);
    SDL_SetWindowSize(window, 1280, 800);
    SDL_PumpEvents();

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
