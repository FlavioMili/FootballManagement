// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once
#include <SDL3/SDL.h>

#include <memory>
#include <stack>

#include "controller/game_controller.h"

class GUIScene;

/**
 * @brief Main view manager for the GUI. Handles scenes and overlays.
 */
class GUIView
{
 public:
  // TODO change to a unique_ptr
  /**
   * @brief Constructs a new GUIView.
   * @param controller_ref Reference to the game controller.
   */
  explicit GUIView(GameController& controller_ref);

  /**
   * @brief Destroys the GUIView.
   */
  ~GUIView();

  /**
   * @brief Starts the scene run loop.
   */
  void run();

  /**
   * @brief Changes the current scene.
   * @param newScene The new scene to switch to.
   *
   * Function to change the scene, this should be used
   * when we also want to remove the previous scene from
   * the Scenes Stack.
   */
  void changeScene(std::unique_ptr<GUIScene> newScene);

  /**
   * @brief Overlays a new scene on top of the current one.
   * @param overlay The scene to overlay.
   *
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

  /**
   * @brief Pops the top overlaid scene.
   *
   * This is the functio to pop Overlaid scenes
   */
  void popScene();

  /**
   * @brief Stops the run loop and quits.
   */
  void quit();

  // Resource access for scenes

  /**
   * @brief Gets the SDL renderer.
   * @return The SDL_Renderer pointer.
   */
  SDL_Renderer* getRenderer() const;

  /**
   * @brief Gets the SDL window.
   * @return The SDL_Window pointer.
   */
  SDL_Window* getWindow() const;

  /**
   * @brief Gets the game controller.
   * @return Reference to the GameController.
   */
  GameController& getController() const;

 private:
  friend class GameFlowTest_GUIFlowLifecycle_Test;
  bool initialize();
  void applyCatppuccinLatteTheme();
  void handleEvents();
  void update(float deltaTime);
  void render();

  // Helper method to get the currently active scene (top overlay or current
  // scene)
  GUIScene* getActiveScene() const;

  GameController& controller;
  SDL_Window* window;
  SDL_Renderer* renderer;
  bool running;

  // Main scene
  std::unique_ptr<GUIScene> currentScene;

  // Overlay scene stack
  std::stack<std::unique_ptr<GUIScene>> sceneStack;

  // Deferred scene management
  enum class PendingAction
  {
    NONE,
    CHANGE,
    OVERLAY,
    POP
  };
  PendingAction pendingAction = PendingAction::NONE;
  std::unique_ptr<GUIScene> pendingScene;
  void applyPendingSceneChanges();
};
