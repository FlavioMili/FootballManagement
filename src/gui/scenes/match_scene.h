// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <imgui.h>

#include <cstdint>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "gui/gui_scene.h"
#include "gui/render/imatch_renderer.h"
#include "gui/scenes/match_scene_tuning.h"
#include "gui/scenes/team_talk_dialog.h"
#include "model/match_engine.h"

/** Which renderer presents the live match. */
enum class MatchViewMode : std::uint8_t
{
  PITCH_2D,
  BROADCAST_3D,
};

/**
 * Live match of the managed club (or any two clubs for tooling).
 *
 * Before kick-off the managed selection is validated for the fixture: an
 * injured or suspended player blocks the match until the lineup is fixed
 * (auto-fix or the lineup screen). The engine then starts with every
 * player's persistent condition, the AI substitutes only for the opponent
 * unless the manager lets the assistant handle changes, and the finished
 * match is recorded with its full engine report and consequences.
 */
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
  friend class GameFlowTest_ManagedMatchIntegration_Test;
  friend class GameFlowTest_ManagementScreensMidSeason_Test;
  friend class GameFlowTest_WatchedMatchSeedIsDeterministic_Test;
  friend class MatchRenderer3DSceneTest_SwitchesViewsAndCapturesFrames_Test;

  /** A selected player who may not take part in today's fixture. */
  struct LineupProblem
  {
    PlayerID id{};
    std::string name;
    std::string reason;
    /** The assistant's pick for the slot (empty: none or bench only). */
    std::string replacement;
  };

  // What the assistant may decide for the manager; kept for the session.
  /** Substitutions for the managed side (off: only the manager's). The
   * lineup assistant lives in the game (GameController). */
  inline static bool assistant_substitutions = false;

  uint16_t home_team_id;
  uint16_t away_team_id;

  std::string home_name;
  std::string away_name;

  /** Side of the managed club, if it plays in this match. */
  std::optional<bool> managed_is_home;
  /** Type of today's calendar fixture between the teams, if there is one. */
  std::optional<MatchType> fixture_type;
  std::vector<LineupProblem> lineup_problems;
  std::string lineup_status;
  /** What the assistant changed before kick-off, shown in the HUD. */
  std::string pre_match_note;

  std::unique_ptr<MatchEngine> engine;
  /** Quick result running on a worker thread (declared after the engine
   * so it is joined before the engine is destroyed). */
  std::future<void> quick_result;
  /** Seconds the "skipping to the next highlight" note stays visible. */
  float skip_indicator_seconds = 0.0f;
  std::unique_ptr<IMatchRenderer> renderer_2d;
  std::unique_ptr<IMatchRenderer> renderer_3d;
  MatchViewMode view_mode = MatchViewMode::PITCH_2D;
  MatchCameraMode camera_mode = MatchCameraMode::BROADCAST;
  bool show_player_names = false;
  float pending_zoom_steps = 0.0f;
  /** Mouse input over the 3D view waiting for the next rendered frame. */
  MatchCameraInput pending_camera_input;
  /** The free camera orbits the moving ball (B). */
  bool free_follow_ball = false;
  /** "Pitch focus": the view fills the window under a compact HUD (F). */
  bool pitch_focus = false;
  /** Statistics and events hidden so the view gets the whole width. */
  bool side_panels_hidden = false;
  /** Width of the focus HUD's control strip last frame (right-aligned). */
  float focus_controls_width = 0.0f;
  float frame_seconds = 0.0f;

  bool match_finished = false;

  /** Multiplier of the engine's real-time pace (1x = real time). */
  float match_speed = 1.0f;
  bool highlights_only = false;
  bool is_paused = false;
  TeamTalkDialog team_talk;

  bool show_substitutions = false;
#ifdef DEBUG
  bool show_ai_debug = false;
#endif
  PlayerID selected_pitch_player{};
  PlayerID selected_bench_player{};
  std::string substitution_status;
  /** Feed rows: indices into the engine's events (key moments by default). */
  std::vector<std::size_t> visible_events;
  std::size_t indexed_events = 0;
  bool show_all_events = false;
  bool indexed_show_all = false;
  bool substitution_refused = false;
  std::string debug_status;
  float scene_entry_milliseconds = 0.0f;
  float last_update_milliseconds = 0.0f;
  float maximum_update_milliseconds = 0.0f;
  std::uint64_t slow_update_count = 0;
  float last_render_milliseconds = 0.0f;
  float average_render_milliseconds = 0.0f;

  /** Re-checks the managed selection against today's fixture. */
  void refreshLineupProblems();
  /** Lets the assistant replace unavailable players (notes the changes). */
  void applyLineupFix();
  /** Builds the engine once the selection is valid. */
  void startMatch();
  /** AI substitutions: opponent always, managed side only via assistant. */
  void applySubstitutionPolicy();
  /** Performs a manual change; false (with a reason) when refused. */
  bool substitute(PlayerID outgoing, PlayerID incoming);
  /** Why the managed side cannot make a change now (nullptr if it can). */
  [[nodiscard]] std::string substitutionBlockReason() const;
  /** Records the result with the full engine report and shows the report. */
  bool finishMatch();
  /** Plays the rest of the match instantly, then finishes it. */
  bool quickResult();
  [[nodiscard]] std::string clockText() const;
  [[nodiscard]] ImU32 teamColor(bool home) const;

  void setPlaybackSpeed(float speed);
  void setHighlightsOnly(bool enabled);
  void setViewMode(MatchViewMode mode);
  void setCameraMode(MatchCameraMode mode);
  /** Enters or leaves pitch focus (full-window view with overlay HUD). */
  void setPitchFocus(bool enabled);
  void setSidePanelsHidden(bool hidden);
  /** Hands the 3D view to the free camera from the pose on screen. */
  void takeFreeCamera();
  void setFreeFollowBall(bool follow);
  /** Toggles the real window between windowed and full screen. */
  void toggleWindowFullscreen();
  /** Orbit, pan, zoom and double-click over the view (last item). */
  void handleViewInput();
  void renderPitchFocus();
  void renderFocusHud(ImVec2 origin, ImVec2 size);
  void renderLineupGate();
  void renderQuickResultProgress();
  void renderScoreboard();
  void renderTimeline();
  /** Appends new engine events to the (optionally filtered) feed. */
  void refreshVisibleEvents();
  void renderControls();
  void renderViewControls();
  void renderPitch(ImVec2 size);
  void renderStatistics(ImVec2 size);
  void renderEvents(ImVec2 size);
  void renderSubstitutionsModal();
#ifdef DEBUG
  void renderDebugLines();
  void exportDebugSnapshot();
#endif
};
