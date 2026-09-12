// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "gui/gui_view.h"

#include <SDL3_ttf/SDL_ttf.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stack>
#include <string_view>
#include <vector>

#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"
#include "controller/game_controller.h"
#include "global/logger.h"
#include "global/paths.h"
#include "global/runtime_paths.h"
#include "gui/gui_scene.h"
#include "gui/render_scale.h"
#include "gui/scenes/main_menu_scene.h"
#include "gui/scenes/match_scene.h"
#include "gui/scenes/team_selection_scene.h"
#include "gui/widgets/theme.h"
#include "imgui.h"
#include "settings_manager.h"

GUIView::GUIView(GameController& controller_ref)
    : controller(controller_ref),
      window(nullptr),
      renderer(nullptr),
      running(false),
      currentScene(nullptr)
{
}

GUIView::~GUIView()
{
  // Clean up any overlaid scenes and active scenes before shutting down
  // renderer
  while (!sceneStack.empty())
  {
    sceneStack.pop();
  }
  currentScene.reset();
  pendingScene.reset();
  releaseBackdrop();

  if (renderer != nullptr)
  {
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
  }
  if (window != nullptr)
  {
    SDL_DestroyWindow(window);
  }
  TTF_Quit();
  SDL_Quit();
}

bool GUIView::initialize()
{
  // Initialize SDL
  // Prefer native Wayland over XWayland: XWayland windows are upscaled by the
  // compositor on fractionally scaled outputs, which makes every glyph blurry.
  // Only a default: SDL_VIDEO_DRIVER or an explicit hint still wins.
  if (std::getenv("WAYLAND_DISPLAY") != nullptr &&
      SDL_GetHint(SDL_HINT_VIDEO_DRIVER) == nullptr)
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "wayland,x11",
                            SDL_HINT_DEFAULT);

  if (!SDL_Init(SDL_INIT_VIDEO))
  {
    std::cerr << "Failed to initialize SDL: " << SDL_GetError() << '\n';
    return false;
  }

  // Initialize SDL_ttf
  if (!TTF_Init())
  {
    std::cerr << "Failed to initialize SDL_ttf: " << SDL_GetError() << '\n';
    return false;
  }

  // Create window
  // High pixel density: the swapchain matches the output's real pixels, and
  // ImGui rasterises glyphs at that density (DisplayFramebufferScale), so
  // text stays sharp on HiDPI and fractionally scaled displays.
  window =
      SDL_CreateWindow("Football Management", 1280, 720,
                       SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (window == nullptr)
  {
    std::cerr << "Failed to create window: " << SDL_GetError() << '\n';
    return false;
  }

  // Create renderer
  renderer = SDL_CreateRenderer(window, nullptr);
  if (renderer == nullptr)
  {
    std::cerr << "Failed to create renderer: " << SDL_GetError() << '\n';
    return false;
  }
  const char* rendererName = SDL_GetRendererName(renderer);
  Logger::info(std::string("SDL renderer driver: ") +
               (rendererName ? rendererName : "unknown"));
  rendererIsSoftware = (rendererName != nullptr &&
                        std::string_view(rendererName).find("software") !=
                            std::string_view::npos);
  if (rendererIsSoftware)
  {
    Logger::warn(
        "Software renderer active (SDL chose a CPU rendering driver). Match "
        "performance and responsiveness may be poor; install/select a GPU "
        "driver if the match appears slow.");
  }

  SettingsManager::instance()->load();
  SettingsManager::instance()->apply(window);

  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  static std::string iniPath = RuntimePaths::imguiIniPath().string();
  io.IniFilename = iniPath.c_str();
  applyManagementTheme();

  // One TTF serves every typography level: the dynamic atlas bakes glyphs
  // on demand for each size requested through Theme::ScopedText.
  std::string fontPath = std::string(PROJECT_ROOT) + "assets/fonts/font.ttf";
  ImFontConfig fontConfig;
  fontConfig.OversampleH = 2;
  if (io.Fonts->AddFontFromFileTTF(fontPath.c_str(),
                                   Theme::textSize(Theme::Text::BODY),
                                   &fontConfig) == nullptr)
  {
    std::cerr << "Failed to load font: " << fontPath << '\n';
    return false;
  }

  ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
  ImGui_ImplSDLRenderer3_Init(renderer);

  changeScene(std::make_unique<MainMenuScene>(this));

  return true;
}

void GUIView::run()
{
  if (!initialize())
  {
    return;
  }

  running = true;
  Uint64 lastTime = SDL_GetTicks();

  while (running)
  {
    const Uint64 frameStart = SDL_GetTicks();
    Uint64 currentTime = SDL_GetTicks();
    float deltaTime = static_cast<float>(currentTime - lastTime) / 1000.0f;
    lastTime = currentTime;

    applyPendingSceneChanges();

    handleEvents();
    update(deltaTime);
    render();

    const int fpsLimit =
        std::clamp(SettingsManager::instance()->get().fps_limit, 15, 360);
    const Uint64 frameBudget = static_cast<Uint64>(1000 / fpsLimit);
    const Uint64 elapsed = SDL_GetTicks() - frameStart;
    if (elapsed < frameBudget)
      SDL_Delay(static_cast<Uint32>(frameBudget - elapsed));
  }

  // Release scenes before the caller saves controller state. A scene may own
  // bounded background work, and its destructor joins that work safely.
  while (!sceneStack.empty()) sceneStack.pop();
  currentScene.reset();
  pendingScene.reset();
}

bool GUIView::runMatchRenderProfile()
{
  if (!controller.isGameLoaded())
  {
    Logger::error("Match render profiling requires an existing loaded save");
    return false;
  }

  const auto& teams = controller.getTeams();
  if (teams.size() < 2)
  {
    Logger::error("Match render profiling requires at least two teams");
    return false;
  }

  TeamID homeTeamId = teams.front().get().getId();
  if (const auto managedTeam = controller.getManagedTeam();
      managedTeam.has_value())
  {
    homeTeamId = managedTeam->get().getId();
  }
  else
  {
    controller.selectManagedTeam(homeTeamId);
  }

  const auto opponent = std::ranges::find_if(
      teams, [homeTeamId](const std::reference_wrapper<const Team>& team)
      { return team.get().getId() != homeTeamId; });
  if (opponent == teams.end())
  {
    Logger::error("Match render profiling could not find an opponent");
    return false;
  }

  if (!initialize()) return false;

  applyPendingSceneChanges();
  overlayScene(
      std::make_unique<MatchScene>(this, homeTeamId, opponent->get().getId()));
  applyPendingSceneChanges();

  running = true;
  while (running && matchFramesTimed < MATCH_FRAME_TIMING_COUNT)
  {
    handleEvents();
    update(MATCH_PROFILE_FRAME_SECONDS);
    render();
  }

  const bool completed = matchFramesTimed == MATCH_FRAME_TIMING_COUNT;
  if (!completed)
  {
    Logger::warn("Match render profiling ended before collecting 120 frames");
  }
  running = false;
  return completed;
}

void GUIView::handleEvents()
{
  SDL_Event event;
  while (SDL_PollEvent(&event))
  {
    ImGui_ImplSDL3_ProcessEvent(&event);
    if (event.type == SDL_EVENT_QUIT)
    {
      running = false;
    }

    if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_F12)
    {
      screenshotPending = true;
    }

    if (event.type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED ||
        event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
    {
      applyManagementTheme();
    }

    if (event.type == SDL_EVENT_WINDOW_RESIZED)
    {
      int width = 0;
      int height = 0;
      SDL_GetWindowSizeInPixels(window, &width, &height);
      GUIScene* activeScene = getActiveScene();
      if (activeScene != nullptr)
      {
        activeScene->onResize(width, height);
      }
    }

    // Pass event to the topmost scene
    // (overlay if exists, otherwise current scene)
    GUIScene* activeScene = getActiveScene();
    if (activeScene != nullptr)
    {
      activeScene->handleEvent(event);
    }
  }
}

void GUIView::update(float deltaTime)
{
  // Update only the active scene
  // (topmost overlay or current scene)
  GUIScene* activeScene = getActiveScene();
  if (activeScene != nullptr)
  {
    activeScene->update(deltaTime);
  }
}

void GUIView::render()
{
  const auto renderStart = std::chrono::steady_clock::now();

  // Clear screen with dark background
  SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
  SDL_RenderClear(renderer);

  // A frozen frame (see requestBackdropCapture) sits behind the UI; scenes
  // drawing over it use transparent windows.
  if (backdropTexture != nullptr && !backdropPending)
    SDL_RenderTexture(renderer, backdropTexture, nullptr, nullptr);

  ImGui_ImplSDLRenderer3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();

  // Render only the active scene
  // (topmost overlay or current scene)
  GUIScene* activeScene = getActiveScene();
  if (activeScene != nullptr)
  {
    activeScene->render();
  }

  ImGui::Render();
  // The SDL_Renderer backend scales only clip rectangles by the framebuffer
  // scale; vertex positions are in window coordinates. Without this, a
  // HiDPI output (e.g. Wayland scale 2) shows the UI in the top-left quarter.
  {
    const ScopedRenderScale scale(renderer,
                                  ImGui::GetIO().DisplayFramebufferScale);
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
  }

  if (backdropPending)
  {
    backdropPending = false;
    releaseBackdrop();
    if (SDL_Surface* frame = SDL_RenderReadPixels(renderer, nullptr))
    {
      backdropTexture = SDL_CreateTextureFromSurface(renderer, frame);
      SDL_DestroySurface(frame);
      // Read-back alpha is undefined on some drivers; the frame is opaque.
      if (backdropTexture != nullptr)
        SDL_SetTextureBlendMode(backdropTexture, SDL_BLENDMODE_NONE);
    }
  }

  bool capturedScreenshot = false;
  if (screenshotPending)
  {
    const char* configuredPath = std::getenv("FM_SCREENSHOT_PATH");
    const std::string path =
        configuredPath && *configuredPath
            ? configuredPath
            : RuntimePaths::capturePath("screenshot.bmp").string();
    if (!captureScreenshot(path))
      std::cerr << "Failed to capture screenshot: " << SDL_GetError() << '\n';
    screenshotPending = false;
    capturedScreenshot = true;
  }

  // Present the rendered frame
  SDL_RenderPresent(renderer);

  // Keep screenshot encoding outside normal frame timing. A negative count
  // means no live match has been entered yet.
  if (!capturedScreenshot && matchFramesTimed >= 0 &&
      matchFramesTimed < MATCH_FRAME_TIMING_COUNT)
  {
    const float renderMs =
        static_cast<float>(std::chrono::duration<double, std::milli>(
                               std::chrono::steady_clock::now() - renderStart)
                               .count());
    matchFrameTimes[static_cast<size_t>(matchFramesTimed++)] = renderMs;
    if (matchFramesTimed == MATCH_FRAME_TIMING_COUNT)
    {
      reportMatchRenderTimings();
    }
  }
}

void GUIView::beginMatchRenderTimings()
{
  matchFrameTimes.fill(0.0f);
  matchFramesTimed = 0;
  Logger::info("Measuring the first 120 live-match render/present frames");
}

void GUIView::reportMatchRenderTimings()
{
  std::vector<float> samples(matchFrameTimes.begin(), matchFrameTimes.end());
  std::sort(samples.begin(), samples.end());
  const float median = samples[samples.size() / 2];
  const float p95 = samples[static_cast<size_t>(
      std::floor(0.95 * static_cast<double>(samples.size() - 1)))];
  const float worst = samples.back();
  Logger::info(
      "Live-match render/present timings over the first 120 frames: median " +
      std::to_string(median) + " ms, p95 " + std::to_string(p95) +
      " ms, worst " + std::to_string(worst) + " ms");
}

void GUIView::changeScene(std::unique_ptr<GUIScene> newScene)
{
  pendingAction = PendingAction::CHANGE;
  pendingScene = std::move(newScene);
}

void GUIView::overlayScene(std::unique_ptr<GUIScene> overlay)
{
  pendingAction = PendingAction::OVERLAY;
  pendingScene = std::move(overlay);
}

void GUIView::popScene() { pendingAction = PendingAction::POP; }

void GUIView::navigateTo(std::unique_ptr<GUIScene> scene)
{
  pendingAction = PendingAction::NAVIGATE;
  pendingScene = std::move(scene);
}

GUIScene* GUIView::getBaseScene() const { return currentScene.get(); }

size_t GUIView::getOverlayDepth() const { return sceneStack.size(); }

void GUIView::applyPendingSceneChanges()
{
  while (pendingAction != PendingAction::NONE)
  {
    PendingAction currentAction = pendingAction;
    std::unique_ptr<GUIScene> sceneToApply = std::move(pendingScene);

    // Reset state before processing, so that onEnter/onExit can trigger new
    // scene changes
    pendingAction = PendingAction::NONE;

    if (currentAction == PendingAction::CHANGE)
    {
      // Clear any overlays when changing main scene
      while (!sceneStack.empty())
      {
        sceneStack.top()->onExit();
        sceneStack.pop();
      }

      // Exit current scene
      if (currentScene)
      {
        currentScene->onExit();
      }

      // Switch to new scene
      currentScene = std::move(sceneToApply);

      // Enter new scene
      if (currentScene)
      {
        currentScene->onEnter();
      }
    }
    else if (currentAction == PendingAction::OVERLAY)
    {
      if (sceneToApply)
      {
        sceneToApply->onEnter();
        if (sceneToApply->getID() == SceneID::MATCH)
        {
          beginMatchRenderTimings();
        }
        sceneStack.push(std::move(sceneToApply));
      }
    }
    else if (currentAction == PendingAction::POP)
    {
      if (!sceneStack.empty())
      {
        // Exit the top overlay scene
        sceneStack.top()->onExit();
        sceneStack.pop();
        if (GUIScene* revealed = getActiveScene()) revealed->onResume();
      }
    }
    else if (currentAction == PendingAction::NAVIGATE)
    {
      const bool hadOverlays = !sceneStack.empty();
      while (!sceneStack.empty())
      {
        sceneStack.top()->onExit();
        sceneStack.pop();
      }
      if (sceneToApply)
      {
        sceneToApply->onEnter();
        if (sceneToApply->getID() == SceneID::MATCH)
        {
          beginMatchRenderTimings();
        }
        sceneStack.push(std::move(sceneToApply));
      }
      else if (hadOverlays && currentScene)
      {
        currentScene->onResume();
      }
    }
  }
}

void GUIView::quit() { running = false; }

SDL_Renderer* GUIView::getRenderer() const { return renderer; }

SDL_Window* GUIView::getWindow() const { return window; }

GameController& GUIView::getController() const { return controller; }

bool GUIView::captureScreenshot(std::string_view path) const
{
  if (!renderer || path.empty()) return false;
  std::filesystem::create_directories(
      std::filesystem::path(path).parent_path());
  SDL_Surface* surface = SDL_RenderReadPixels(renderer, nullptr);
  if (!surface) return false;
  const bool saved = SDL_SaveBMP(surface, std::string(path).c_str());
  SDL_DestroySurface(surface);
  return saved;
}

// Return the topmost scene
// (overlay if exists, otherwise current scene)
GUIScene* GUIView::getActiveScene() const
{
  if (!sceneStack.empty())
  {
    return sceneStack.top().get();
  }
  return currentScene.get();
}

float GUIView::displayScale() const
{
  if (window == nullptr) return 1.0f;
  // Display scale = pixel density x content scale. Window coordinates (and
  // therefore ImGui's) already include the pixel density wherever the
  // platform reports one (Wayland, macOS), so only the remaining content
  // scale enlarges the layout. On Windows density is 1 and the whole display
  // scale applies.
  const float display = SDL_GetWindowDisplayScale(window);
  const float density = SDL_GetWindowPixelDensity(window);
  if (display <= 0.0f) return 1.0f;
  return density > 0.0f ? display / density : display;
}

void GUIView::applyManagementTheme()
{
  // Palette, spacing and typography live in the shared design system so
  // scenes and widgets draw from the same tokens. Re-applied when appearance
  // settings change or the window moves to a display with another scale.
  const Settings& settings = SettingsManager::instance()->get();
  Theme::Appearance appearance;
  appearance.preset = static_cast<Theme::Preset>(std::clamp(
      settings.theme_preset, 0, static_cast<int>(Theme::Preset::COUNT) - 1));
  appearance.club_accent = settings.club_accent;
  appearance.custom_accent = Theme::unpackRgb(settings.accent_rgb);
  appearance.ui_scale = settings.ui_scale;
  appearance.compact = settings.compact_density;
  appearance.reduced_motion = settings.reduced_motion;
  Theme::apply(appearance, displayScale());
}

void GUIView::refreshTheme() { applyManagementTheme(); }

void GUIView::requestBackdropCapture() { backdropPending = true; }

void GUIView::releaseBackdrop()
{
  if (backdropTexture != nullptr) SDL_DestroyTexture(backdropTexture);
  backdropTexture = nullptr;
}
