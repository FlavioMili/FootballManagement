// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include "gui/gui_scene.h"
#include "model/match_engine.h"

class MatchScene : public GUIScene
{
 public:
  MatchScene(class GUIView* guiView_ptr, uint16_t home_id, uint16_t away_id);

  void onEnter() override;
  void update(float deltaTime) override;
  void render() override;
  SceneID getID() const override;

 private:
  uint16_t home_team_id;
  uint16_t away_team_id;

  std::string home_name;
  std::string away_name;

  std::unique_ptr<MatchEngine> engine;

  bool match_finished = false;

  float match_speed = 1.0f;
  bool is_paused = false;

  bool show_substitutions = false;
  uint32_t selected_pitch_player = 0;
  uint32_t selected_bench_player = 0;

  void renderSubstitutionsModal();
};
