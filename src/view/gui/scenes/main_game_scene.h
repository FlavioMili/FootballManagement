// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once
#include "view/gui/gui_scene.h"
#include "view/gui/gui_view.h"
#include "view/gui/button_manager.h"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <memory>
#include <vector>

class MainGameScene : public GUIScene {
 public:
  explicit MainGameScene(GUIView* guiView);
  ~MainGameScene();
  void handleEvent(const SDL_Event& event) override;
  void update(float deltaTime) override;
  void render() override;
  void onEnter() override;
  void onExit() override;
  SceneID getID() const override;

 private:
  void initializeUI();
  void renderSidebar();
  void updateLayout();
  void cleanup();
  void setupButtons();

  TTF_Font* font = nullptr;
  std::unique_ptr<ButtonManager> buttonManager;

  // UI element dimensions
  SDL_FRect sidebarRect;

  // Button IDs
  enum ButtonIndex : int {
    VIEW_ROSTER_BUTTON = 0,
    SET_STRATEGY_BUTTON = 1,
    FINANCES_BUTTON = 2,
    TRANSFER_MARKET_BUTTON = 3,
    NEXT_WEEK_BUTTON = 4
  };
  std::vector<int> sidebarButtonIds;
  int nextButtonId = -1;
};
