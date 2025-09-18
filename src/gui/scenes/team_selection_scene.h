// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

// team_selection_scene.h
#pragma once

#include "gui_scene.h"
#include "model/team.h"
#include "view/gui/button_manager.h"
#include <vector>
#include <SDL3_ttf/SDL_ttf.h>

class GUIView;

class TeamSelectionScene : public GUIScene {
 private:
  GUIView* parent_view;
  TTF_Font* font;
  TTF_Font* title_font;
  std::vector<Team> available_teams;
  ButtonManager button_manager;

  void loadAvailableTeams();
  void setupTeamButtons();
  void onTeamSelected(int team_id);
  void cleanup();

 public:
  explicit TeamSelectionScene(GUIView* parent);
  ~TeamSelectionScene() override;

  void onEnter() override;
  void onExit() override;
  void handleEvent(const SDL_Event& event) override;
  void update(float deltaTime) override;
  void render() override;
  SceneID getID() const override;
};
