// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once
#include "view/gui/gui_view.h"
#include <SDL3/SDL.h>
#include <cstdint>
#include <memory>

// TODO add scene types
enum class SceneID : uint8_t { MAIN_MENU, SETTINGS, GAME_MENU, };

class GUIScene {
 public:
  explicit GUIScene(GUIView* guiView);
  virtual ~GUIScene() = default;

  // Core scene interface - must be implemented by derived classes
  virtual void handleEvent(const SDL_Event& event) = 0;
  virtual void update(float deltaTime) = 0;
  virtual void render() = 0;

  // Optional lifecycle hooks
  virtual void onEnter();
  virtual void onExit();

  /*
  * This function might be used to check what we are rendering.
  * we would simply do something like this: 
  * MainMenuScene::getID() { return SceneID::MAIN_MENU; }
  * TODO : remove or not?
  */
  virtual SceneID getID() const = 0;

 protected:
  // Helper methods for derived classes
  SDL_Renderer* getRenderer() const;
  SDL_Window* getWindow() const;
  GUIView* getGuiView() const { return guiView; }
  void changeScene(std::unique_ptr<GUIScene> newScene);
  void quit();

  GUIView* guiView;

 private:
};
