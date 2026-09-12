// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "gui/gui_scene.h"
#include "gui/render/imatch_renderer.h"
#include "gui/scenes/match_scene_tuning.h"
#include "model/match_engine.h"

/** Which renderer presents the live match. */
enum class MatchViewMode : std::uint8_t
{
  PITCH_2D,
  BROADCAST_3D,
};

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
  friend class MatchRenderer3DSceneTest_SwitchesViewsAndCapturesFrames_Test;
  uint16_t home_team_id;
  uint16_t away_team_id;

  std::string home_name;
  std::string away_name;

  std::string home_label;
  std::string away_label;

  std::unique_ptr<MatchEngine> engine;
  std::unique_ptr<IMatchRenderer> renderer_2d;
  std::unique_ptr<IMatchRenderer> renderer_3d;
  MatchViewMode view_mode = MatchViewMode::PITCH_2D;
  MatchCameraMode camera_mode = MatchCameraMode::BROADCAST;
  bool show_player_names = false;
  float pending_zoom_steps = 0.0f;
  float frame_seconds = 0.0f;

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
  float last_render_milliseconds = 0.0f;
  float average_render_milliseconds = 0.0f;

  void setViewMode(MatchViewMode mode);
  void setCameraMode(MatchCameraMode mode);
  void renderViewControls();
  void renderSubstitutionsModal();
#ifdef DEBUG
  void exportDebugSnapshot();
#endif
};
