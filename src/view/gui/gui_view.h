#pragma once
#include "controller/game_controller.h"
#include <SDL3/SDL.h>
#include <memory>

class GUIScene;

class GUIView {
public:
  GUIView(GameController& controller);
  ~GUIView();

  void run();
  void changeScene(std::unique_ptr<GUIScene> newScene);
  void quit();

  // Resource access for scenes
  SDL_Renderer* getRenderer() const;
  SDL_Window* getWindow() const;

private:
  bool initialize();
  void handleEvents();
  void update(float deltaTime);
  void render();

  GameController& controller;
  SDL_Window* window;
  SDL_Renderer* renderer;
  bool running;

  std::unique_ptr<GUIScene> currentScene;
};
