// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once
#include <array>
#include <optional>
#include <vector>

#include "gui/gui_scene.h"
#include "model/player.h"

/**
 * @brief Scene for displaying the team roster.
 */
class RosterScene : public GUIScene
{
 public:
  /**
   * @brief Constructs a new RosterScene.
   * @param parent Pointer to the GUIView.
   */
  explicit RosterScene(GUIView* parent);

  /**
   * @brief Destroys the RosterScene.
   */
  ~RosterScene() override = default;

  /**
   * @brief Called when entering the scene.
   */
  void onEnter() override;

  /**
   * @brief Updates scene logic.
   * @param deltaTime Time elapsed since last update.
   */
  void update(float deltaTime) override;

  /**
   * @brief Renders the scene.
   */
  void render() override;

  /**
   * @brief Gets the ID of this scene.
   * @return The SceneID (ROSTER).
   */
  [[nodiscard]] SceneID getID() const override;

 private:
  friend class GameFlowTest_GUIFlowLifecycle_Test;
  void loadRoster();
  [[nodiscard]] const Player* selectedPlayer() const;
  std::vector<std::reference_wrapper<const Player>> roster_players;
  std::optional<PlayerID> selected_player_id;
  std::array<char, 64> search_text{};
  int role_filter_index = 0;
};
