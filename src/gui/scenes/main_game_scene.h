// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once
#include "gui/gui_scene.h"
#include "gui/gui_view.h"
#include "gui/button_manager.h"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <cstdint>
#include <memory>
#include <vector>

enum GameSceneButtonIndex : uint8_t {
  VIEW_ROSTER_BUTTON = 0,
  SET_STRATEGY_BUTTON = 1,
  FINANCES_BUTTON = 2,
  TRANSFER_MARKET_BUTTON = 3,
  NEXT_WEEK_BUTTON = 4
};

class MainGameScene : public GUIScene {
 public:
  explicit MainGameScene(GUIView* guiView_ptr);
  ~MainGameScene();
  void handleEvent(const SDL_Event& event) override;
  void update(float deltaTime) override;
  void render() override;
  void onEnter() override;
  void onExit() override;
  void onResize(int width, int height) override;
  SceneID getID() const override;

 private:
  void initializeUI();
  void renderSidebar();
  void renderLeaderboard();
  void renderTopPlayers();
  void updateLayout();
  void cleanup();
  void setupButtons();

  TTF_Font* font = nullptr;
  std::unique_ptr<ButtonManager> buttonManager;

  // UI element dimensions
  SDL_FRect sidebarRect;

  std::vector<int> sidebarButtonIds;
  int nextButtonId = -1;
};
