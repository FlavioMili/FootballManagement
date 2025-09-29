// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

// team_selection_scene.h
#pragma once

#include "gui/button_manager.h"
#include "gui/gui_scene.h"
#include "model/league.h"
#include "model/team.h"
#include <SDL3_ttf/SDL_ttf.h>
#include <cstdint>
#include <sys/types.h>
#include <vector>

class GUIView;

class TeamSelectionScene : public GUIScene {
 public:
  explicit TeamSelectionScene(GUIView *parent);
  ~TeamSelectionScene() override;

  void onEnter() override;
  void onExit() override;
  void handleEvent(const SDL_Event &event) override;
  void update(float deltaTime) override;
  void render() override;
  SceneID getID() const override;

 private:
  GUIView *parent_view;
  TTF_Font *font;
  TTF_Font *title_font;
  std::vector<std::reference_wrapper<const League>> available_leagues;
  std::vector<std::reference_wrapper<const Team>> available_teams;
  ButtonManager button_manager;
  std::optional<uint8_t> selected_league_id;
  std::vector<int> league_button_ids;
  float league_scroll_offset_ = 0.0f;
  bool is_dragging_leagues_ = false;
  SDL_FRect league_list_rect_;
  float team_scroll_offset_ = 0.0f;
  bool is_dragging_teams_ = false;
  SDL_FRect team_list_rect_;

  void loadAvailableLeagues();
  void loadAvailableTeams();
  void setupLeagueAndTeamButtons();
  void onLeagueSelected(uint8_t league_id);
  void onTeamSelected(uint16_t team_id);
  void cleanup();
  void renderLeagueScrollbar();
  void renderTeamScrollbar();
};
