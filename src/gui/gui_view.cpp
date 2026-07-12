// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "gui/gui_view.h"

#include <SDL3_ttf/SDL_ttf.h>

#include <iostream>
#include <stack>

#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"
#include "controller/game_controller.h"
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
  // Clean up any overlaid scenes
  while (!sceneStack.empty())
  {
    sceneStack.pop();
  }

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
    Uint64 currentTime = SDL_GetTicks();
    float deltaTime = static_cast<float>(currentTime - lastTime) / 1000.0f;
    lastTime = currentTime;

    applyPendingSceneChanges();

    handleEvents();
    update(deltaTime);
    render();

    // Cap framerate to ~60 FPS
    // TODO check framerate of computer and update this
    // or edit in settings
    SDL_Delay(16);
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

  // Present the rendered frame
  SDL_RenderPresent(renderer);
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
  if (pendingAction == PendingAction::NONE)
  {
    return;
  }

  if (pendingAction == PendingAction::CHANGE)
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
    currentScene = std::move(pendingScene);

    // Enter new scene
    if (currentScene)
    {
      currentScene->onEnter();
    }
  }
  else if (pendingAction == PendingAction::OVERLAY)
  {
    if (pendingScene)
    {
      pendingScene->onEnter();
      sceneStack.push(std::move(pendingScene));
    }
  }
  else if (pendingAction == PendingAction::POP)
  {
    if (!sceneStack.empty())
    {
      // Exit the top overlay scene
      sceneStack.top()->onExit();
      sceneStack.pop();
    }
  }

  pendingAction = PendingAction::NONE;
  pendingScene.reset();
}

void GUIView::quit() { running = false; }

SDL_Renderer* GUIView::getRenderer() const { return renderer; }

SDL_Window* GUIView::getWindow() const { return window; }

GameController& GUIView::getController() const { return controller; }

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

  ImVec4 base = ImVec4(0.937f, 0.945f, 0.961f, 1.00f);
  ImVec4 crust = ImVec4(0.863f, 0.878f, 0.910f, 1.00f);
  ImVec4 mantle = ImVec4(0.902f, 0.914f, 0.937f, 1.00f);
  ImVec4 text = ImVec4(0.298f, 0.310f, 0.412f, 1.00f);
  ImVec4 sapphire = ImVec4(0.125f, 0.624f, 0.710f, 1.00f);
  ImVec4 sapphireHover = ImVec4(0.125f, 0.7f, 0.8f, 1.00f);
  ImVec4 sapphireActive = ImVec4(0.125f, 0.8f, 0.9f, 1.00f);

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
