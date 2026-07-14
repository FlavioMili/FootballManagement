// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once
#include "gui/gui_scene.h"
#include "gui/gui_view.h"

/**
 * @brief Scene for the main game dashboard.
 */
class MainGameScene : public GUIScene
{
 public:
  /**
   * @brief Constructs a new MainGameScene.
   * @param guiView_ptr Pointer to the GUIView.
   */
  explicit MainGameScene(GUIView* guiView_ptr);

  /**
   * @brief Destroys the MainGameScene.
   */
  ~MainGameScene() override = default;

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
   * @return The SceneID (GAME_MENU).
   */
  [[nodiscard]] SceneID getID() const override;

 private:
  void renderSidebar();
  void renderMainArea();
  void renderTopBar();

  void refreshData();

  std::vector<std::pair<int, int>> cached_standings;
  std::vector<std::reference_wrapper<const Player>> cached_top_players;
  int cached_season = -1;
  GameDateValue cached_date;
};
