// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "gui/gui_view.h"

#include <iostream>
#include <stack>

#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"
#include <SDL3_ttf/SDL_ttf.h>
#include "controller/game_controller.h"
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
  io.IniFilename = "assets/imgui.ini";
  ImGui::StyleColorsDark();

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
  currentScene = std::move(newScene);

  // Enter new scene
  if (currentScene)
  {
    currentScene->onEnter();
  }
}

void GUIView::overlayScene(std::unique_ptr<GUIScene> overlay)
{
  if (overlay)
  {
    overlay->onEnter();
    sceneStack.push(std::move(overlay));
  }
}

void GUIView::popScene()
{
  if (!sceneStack.empty())
  {
    // Exit the top overlay scene
    sceneStack.top()->onExit();
    sceneStack.pop();
  }
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
