// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <imgui.h>

#include <unordered_map>

#include "gui/scenes/management_scene.h"
#include "model/lineup.h"

/**
 * @class LineupScene
 * @brief GUI scene for managing the team's lineup.
 *
 * This scene allows the user to view and modify their lineup using a 2D pitch
 * representation, formation presets and an automatic best-XI pick.
 */
class LineupScene : public ManagementScene
{
 public:
  /**
   * @brief Constructor
   * @param parent Pointer to the GUIView managing this scene
   */
  explicit LineupScene(class GUIView* parent);

  /**
   * @brief Opens the lineup with one player pre-selected.
   * @param parent Pointer to the GUIView managing this scene
   * @param focusPlayer Player to highlight on the pitch or bench
   */
  LineupScene(class GUIView* parent, PlayerID focusPlayer);

  void update(float deltaTime) override;

  [[nodiscard]] SceneID getID() const override { return SceneID::LINEUP; }

 protected:
  void renderContent() override;
  [[nodiscard]] NavSection navSection() const override
  {
    return NavSection::LINEUP;
  }
  void refresh() override { loadLineup(); }

 private:
  friend class GameFlowTest_GUIFlowLifecycle_Test;
  void loadLineup();
  void renderToolbar();
  void renderPitch(float width, float height);
  void renderPlayerToken(const Player& player, ImVec2 center, bool goalkeeper,
                         ImVec2 pitchMin, ImVec2 pitchSize);
  void renderBench(float height);
  void autoPickBestEleven();
  [[nodiscard]] const Player* selectedPitchPlayer() const;
  [[nodiscard]] const Player* selectedBenchPlayer() const;

  /** @brief Why a player cannot be picked right now. */
  enum class Unavailability : uint8_t
  {
    INJURED,
    SUSPENDED
  };

  [[nodiscard]] size_t unavailableStarters() const;

  PlayerID focus_player_id = 0;
  int formation_index = -1;
  std::unordered_map<PlayerID, Unavailability> unavailable;

  Lineup* current_lineup = nullptr;
  PlayerID selected_pitch_player_id = 0;
  PlayerID selected_bench_player_id = 0;
};
