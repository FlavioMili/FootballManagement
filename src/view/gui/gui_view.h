// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once
#include "controller/game_controller.h"
#include <SDL3/SDL.h>
#include <memory>
#include <stack>

class GUIScene;

class GUIView {
public:
  GUIView(GameController& controller);
  ~GUIView();

  /* Starts the scene run */
  void run();

  /*
  * Function to change the scene, this should be used
  * when we also want to remove the previous scene from
  * the Scenes Stack.
  */
  void changeScene(std::unique_ptr<GUIScene> newScene);

  /*
  * This functions adds a scene as an overlay, this 
  * makes it possible to show faster the scene that 
  * was previously dispalyed.
  * Example: When we are on the main game dashboard
  * if we want to display the Lineup, we could just 
  * overlay on the main game dashboard since we will
  * for sure go back. Then if we click on a player to
  * see his profile, we can make a new overlay, and these
  * scenes will be just popped at the end of their usage.
  */
  void overlayScene(std::unique_ptr<GUIScene> overlay);

  // This is the functio to pop Overlaid scenes
  void popScene();

  // Stops the run
  void quit();

  // Resource access for scenes
  SDL_Renderer* getRenderer() const;
  SDL_Window* getWindow() const;

 private:
  bool initialize();
  void handleEvents();
  void update(float deltaTime);
  void render();

  // Helper method to get the currently active scene (top overlay or current scene)
  GUIScene* getActiveScene() const;

  GameController& controller;
  SDL_Window* window;
  SDL_Renderer* renderer;
  bool running;

  // Main scene
  std::unique_ptr<GUIScene> currentScene;

  // Overlay scene stack
  std::stack<std::unique_ptr<GUIScene>> sceneStack;
};
