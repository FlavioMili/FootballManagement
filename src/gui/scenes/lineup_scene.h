// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include "gui/gui_scene.h"
#include "model/lineup.h"

/**
 * @class LineupScene
 * @brief GUI scene for managing the team's lineup.
 *
 * This scene allows the user to view and modify their lineup using a 2D pitch
 * representation.
 */
class LineupScene : public GUIScene
{
 public:
  /**
   * @brief Constructor
   * @param parent Pointer to the GUIView managing this scene
   */
  explicit LineupScene(class GUIView* parent);

  void onEnter() override;
  void update(float deltaTime) override;
  void render() override;

  [[nodiscard]] SceneID getID() const override { return SceneID::LINEUP; }

 private:
  void loadLineup();
  void renderPitch();
  void renderBench();

  PlayerID dragging_player_id = 0;
  bool is_dragging = false;

  Lineup* current_lineup = nullptr;
};
