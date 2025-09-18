// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include "gui/gui_scene.h"
#include "gui/button_manager.h"
#include "model/player.h"
#include <vector>

class GUIView;

class RosterScene : public GUIScene {
 public:
  explicit RosterScene(GUIView* parent);
  ~RosterScene() override;

  void onEnter() override;
  void onExit() override;
  void handleEvent(const SDL_Event& event) override;
  void update(float deltaTime) override;
  void render() override;
  SceneID getID() const override;

  void onResize(int width, int height) override;

 private:
  GUIView* parent_view;
  ButtonManager button_manager;
  std::vector<Player> roster_players;
  int backButtonId = -1;

  void setupUI();

  void loadRoster();
  void setupPlayerDisplay();
};
