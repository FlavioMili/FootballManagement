// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once
#include <SDL3/SDL.h>

#include <cstdint>
#include <memory>

#include "gui/gui_view.h"

/**
 * @brief Identifiers for different GUI scenes.
 */
// TODO add scene types
enum class SceneID : uint8_t
{
  MAIN_MENU,      /**< Main menu scene */
  SETTINGS,       /**< Settings scene */
  GAME_MENU,      /**< In-game menu scene */
  TEAM_SELECTION, /**< Team selection scene */
  ROSTER,         /**< Team roster scene */
  STRATEGY,       /**< Strategy management scene */
};

/**
 * @brief Base class for all GUI scenes.
 */
class GUIScene
{
 public:
  /**
   * @brief Constructs a new GUIScene.
   * @param guiView_ptr Pointer to the main GUIView.
   */
  explicit GUIScene(GUIView* guiView_ptr);

  /**
   * @brief Destroys the GUIScene.
   */
  virtual ~GUIScene() = default;

  // Core scene interface - must be implemented by derived classes

  /**
   * @brief Handles SDL events for the scene.
   * @param event The SDL event to process.
   */
  virtual void handleEvent(const SDL_Event& event);

  /**
   * @brief Updates scene logic.
   * @param deltaTime The time elapsed since the last update.
   */
  virtual void update(float deltaTime) = 0;

  /**
   * @brief Renders the scene.
   */
  virtual void render() = 0;

  // Optional lifecycle hooks

  /**
   * @brief Called when the scene is entered/becomes active.
   */
  virtual void onEnter();

  /**
   * @brief Called when the scene is exited/becomes inactive.
   */
  virtual void onExit();

  /**
   * @brief Called when the window is resized.
   * @param width The new window width.
   * @param height The new window height.
   */
  virtual void onResize(int width, int height);

  /**
   * @brief Gets the ID of the scene.
   * @return The SceneID of this scene.
   *
   * This function might be used to check what we are rendering.
   * we would simply do something like this:
   * MainMenuScene::getID() { return SceneID::MAIN_MENU; }
   * TODO : remove or not?
   */
  virtual SceneID getID() const = 0;

 protected:
  // Helper methods for derived classes

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
   * @brief Gets the associated GUIView.
   * @return The GUIView pointer.
   */
  GUIView* getGuiView() const { return guiView; }

  /**
   * @brief Requests a scene change.
   * @param newScene The new scene to transition to.
   */
  void changeScene(std::unique_ptr<GUIScene> newScene);

  /**
   * @brief Requests the application to quit.
   */
  void quit();

  GUIView* guiView;

 private:
};
