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
#include "gui/gui_scene.h"
#include "gui/scenes/main_menu_scene.h"
#include "gui/scenes/team_selection_scene.h"
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
  window = SDL_CreateWindow("Game GUI", 1200, 800, SDL_WINDOW_RESIZABLE);
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
  static std::string iniPath = std::string(PROJECT_ROOT) + "assets/imgui.ini";
  io.IniFilename = iniPath.c_str();
  applyCatppuccinLatteTheme();

  // Load high quality TTF font to replace the pixelated default
  std::string fontPath = std::string(PROJECT_ROOT) + "assets/fonts/font.ttf";
  if (io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 20.0f) == nullptr)
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
  ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);

  bool capturedScreenshot = false;
  if (screenshotPending)
  {
    const char* configuredPath = std::getenv("FM_SCREENSHOT_PATH");
    const std::string path = configuredPath && *configuredPath
                                 ? configuredPath
                                 : "/tmp/football_management_screenshot.bmp";
    if (!captureScreenshot(path))
      std::cerr << "Failed to capture screenshot: " << SDL_GetError() << '\n';
    screenshotPending = false;
    capturedScreenshot = true;
  }

  // Present the rendered frame
  SDL_RenderPresent(renderer);

  // Keep screenshot encoding outside normal frame timing. Screenshot frames
  // are excluded from the startup statistics.
  if (!capturedScreenshot && startupFramesTimed < STARTUP_FRAME_TIMING_COUNT)
  {
    const float renderMs =
        static_cast<float>(std::chrono::duration<double, std::milli>(
                               std::chrono::steady_clock::now() - renderStart)
                               .count());
    startupFrameTimes[static_cast<size_t>(startupFramesTimed++)] = renderMs;
    if (startupFramesTimed == STARTUP_FRAME_TIMING_COUNT)
    {
      reportStartupRenderTimings();
    }
  }
}

void GUIView::reportStartupRenderTimings()
{
  std::vector<float> samples(startupFrameTimes.begin(),
                             startupFrameTimes.end());
  std::sort(samples.begin(), samples.end());
  const float median = samples[samples.size() / 2];
  const float p95 = samples[static_cast<size_t>(
      std::floor(0.95 * static_cast<double>(samples.size() - 1)))];
  const float worst = samples.back();
  Logger::info("Render/present timings over the first 120 frames: median " +
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

void GUIView::applyCatppuccinLatteTheme()
{
  ImGuiStyle& style = ImGui::GetStyle();
  ImVec4* colors = style.Colors;

  auto base = ImVec4(0.937f, 0.945f, 0.961f, 1.00f);
  auto crust = ImVec4(0.863f, 0.878f, 0.910f, 1.00f);
  auto mantle = ImVec4(0.902f, 0.914f, 0.937f, 1.00f);
  auto text = ImVec4(0.298f, 0.310f, 0.412f, 1.00f);
  auto sapphire = ImVec4(0.125f, 0.624f, 0.710f, 1.00f);
  auto sapphireHover = ImVec4(0.125f, 0.7f, 0.8f, 1.00f);
  auto sapphireActive = ImVec4(0.125f, 0.8f, 0.9f, 1.00f);

  colors[ImGuiCol_Text] = text;
  colors[ImGuiCol_TextDisabled] = ImVec4(0.424f, 0.435f, 0.522f, 1.00f);
  colors[ImGuiCol_WindowBg] = base;
  colors[ImGuiCol_ChildBg] = crust;
  colors[ImGuiCol_PopupBg] = base;
  colors[ImGuiCol_Border] = mantle;
  colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
  colors[ImGuiCol_FrameBg] = mantle;
  colors[ImGuiCol_FrameBgHovered] = crust;
  colors[ImGuiCol_FrameBgActive] = crust;
  colors[ImGuiCol_TitleBg] = mantle;
  colors[ImGuiCol_TitleBgActive] = crust;
  colors[ImGuiCol_TitleBgCollapsed] = base;
  colors[ImGuiCol_MenuBarBg] = mantle;
  colors[ImGuiCol_ScrollbarBg] = base;
  colors[ImGuiCol_ScrollbarGrab] = crust;
  colors[ImGuiCol_ScrollbarGrabHovered] = mantle;
  colors[ImGuiCol_ScrollbarGrabActive] = text;
  colors[ImGuiCol_CheckMark] = sapphire;
  colors[ImGuiCol_SliderGrab] = sapphire;
  colors[ImGuiCol_SliderGrabActive] = sapphireActive;
  colors[ImGuiCol_Button] = sapphire;
  colors[ImGuiCol_ButtonHovered] = sapphireHover;
  colors[ImGuiCol_ButtonActive] = sapphireActive;
  colors[ImGuiCol_Header] = mantle;
  colors[ImGuiCol_HeaderHovered] = crust;
  colors[ImGuiCol_HeaderActive] = crust;
  colors[ImGuiCol_Separator] = mantle;
  colors[ImGuiCol_SeparatorHovered] = crust;
  colors[ImGuiCol_SeparatorActive] = crust;
  colors[ImGuiCol_ResizeGrip] = sapphire;
  colors[ImGuiCol_ResizeGripHovered] = sapphireHover;
  colors[ImGuiCol_ResizeGripActive] = sapphireActive;
  colors[ImGuiCol_TabHovered] = crust;
  colors[ImGuiCol_Tab] = mantle;
  colors[ImGuiCol_TabSelected] = crust;
  colors[ImGuiCol_TabSelectedOverline] = sapphire;
  colors[ImGuiCol_TabDimmed] = mantle;
  colors[ImGuiCol_TabDimmedSelected] = mantle;
  colors[ImGuiCol_TabDimmedSelectedOverline] =
      ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
  colors[ImGuiCol_NavHighlight] = sapphireHover;
  colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
  colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
  colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);

  style.WindowRounding = 8.0f;
  style.FrameRounding = 6.0f;
  style.PopupRounding = 6.0f;
  style.ScrollbarRounding = 6.0f;
  style.GrabRounding = 4.0f;
  style.ItemSpacing = ImVec2(10, 10);
  style.FramePadding = ImVec2(10, 8);
}
