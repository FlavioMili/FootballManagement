// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include "gui/gui_scene.h"
#include "gui/render/imatch_renderer.h"
#include "gui/scenes/match_scene_tuning.h"
#include "model/match_engine.h"

class MatchScene : public GUIScene
{
 public:
  MatchScene(class GUIView* guiView_ptr, uint16_t home_id, uint16_t away_id);

  void onEnter() override;
  void handleEvent(const SDL_Event& event) override;
  void update(float deltaTime) override;
  void render() override;
  SceneID getID() const override;

 private:
  friend class GameFlowTest_GUIFlowLifecycle_Test;
  uint16_t home_team_id;
  uint16_t away_team_id;

  std::string home_name;
  std::string away_name;

  std::unique_ptr<MatchEngine> engine;
  std::unique_ptr<IMatchRenderer> matchRenderer;

  bool match_finished = false;

  float match_speed = MatchSceneTuning::Controls::DEFAULT_MATCH_SPEED;
  bool is_paused = false;

  bool show_substitutions = false;
#ifdef DEBUG
  bool show_ai_debug = false;
#endif
  PlayerID selected_pitch_player{};
  PlayerID selected_bench_player{};
  std::string debug_status;
  float scene_entry_milliseconds = 0.0f;
  float last_update_milliseconds = 0.0f;
  float maximum_update_milliseconds = 0.0f;
  std::uint64_t slow_update_count = 0;

  void renderSubstitutionsModal();
#ifdef DEBUG
  void exportDebugSnapshot();
#endif
};
