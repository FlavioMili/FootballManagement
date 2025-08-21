#include "gui_view.h"
#include "gui_scene.h"
#include <iostream>

GUIView::GUIView(GameController& controller) 
  : controller(controller), window(nullptr),
  renderer(nullptr), running(false),
  currentScene(nullptr) { }

GUIView::~GUIView() {
  if (renderer) {
    SDL_DestroyRenderer(renderer);
  }
  if (window) {
    SDL_DestroyWindow(window);
  }
  SDL_Quit();
}

bool GUIView::initialize() {
  // Initialize SDL
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
    return false;
  }

  // Create window
  window = SDL_CreateWindow("Game GUI", 800, 600, SDL_WINDOW_RESIZABLE);
  if (!window) {
    std::cerr << "Failed to create window: " << SDL_GetError() << std::endl;
    return false;
  }

  // Create renderer
  renderer = SDL_CreateRenderer(window, nullptr);
  if (!renderer) {
    std::cerr << "Failed to create renderer: " << SDL_GetError() << std::endl;
    return false;
  }

  // Enable blending for transparency
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

  return true;
}

void GUIView::run() {
  if (!initialize()) {
    return;
  }

  running = true;
  Uint64 lastTime = SDL_GetTicks();

  while (running) {
    Uint64 currentTime = SDL_GetTicks();
    float deltaTime = (currentTime - lastTime) / 1000.0f;
    lastTime = currentTime;

    handleEvents();
    update(deltaTime);
    render();

    // Cap framerate to ~60 FPS
    // TODO check framerate of computer and update this
    SDL_Delay(16);
  }
}

void GUIView::handleEvents() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_EVENT_QUIT) {
      running = false;
    }

    // Pass event to current scene
    if (currentScene) {
      currentScene->handleEvent(event);
    }
  }
}

void GUIView::update(float deltaTime) {
  if (currentScene) {
    currentScene->update(deltaTime);
  }
}

void GUIView::render() {
  // Clear screen with dark background
  SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
  SDL_RenderClear(renderer);

  // Render current scene
  if (currentScene) {
    currentScene->render();
  }

  // Present the rendered frame
  SDL_RenderPresent(renderer);
}

void GUIView::changeScene(std::unique_ptr<GUIScene> newScene) {
  // Exit current scene
  if (currentScene) {
    currentScene->onExit();
  }

  // Switch to new scene
  currentScene = std::move(newScene);

  // Enter new scene
  if (currentScene) {
    currentScene->onEnter();
  }
}

void GUIView::quit() {
  running = false;
}

SDL_Renderer* GUIView::getRenderer() const {
  return renderer;
}

SDL_Window* GUIView::getWindow() const {
  return window;
}
