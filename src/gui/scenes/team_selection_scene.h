// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once
#include <cstdint>
#include <optional>
#include <vector>

#include "gui/gui_scene.h"
#include "model/league.h"
#include "model/team.h"

/**
 * @brief Scene for selecting a team.
 */
class TeamSelectionScene : public GUIScene
{
 public:
  /**
   * @brief Constructs a new TeamSelectionScene.
   * @param parent Pointer to the GUIView.
   */
  explicit TeamSelectionScene(GUIView* parent);

  /**
   * @brief Destroys the TeamSelectionScene.
   */
  ~TeamSelectionScene() override = default;

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
   * @return The SceneID (TEAM_SELECTION).
   */
  [[nodiscard]] SceneID getID() const override;

 private:
  void loadAvailableLeagues();
  void loadAvailableTeams();

  std::vector<std::reference_wrapper<const League>> available_leagues;
  std::vector<std::reference_wrapper<const Team>> available_teams;
  std::optional<uint8_t> selected_league_id;
};
