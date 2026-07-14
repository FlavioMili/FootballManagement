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
 * @brief Scene for the main menu.
 */
class MainMenuScene : public GUIScene
{
 public:
  /**
   * @brief Constructs a new MainMenuScene.
   * @param guiView_ptr Pointer to the GUIView.
   */
  explicit MainMenuScene(GUIView* guiView_ptr);

  /**
   * @brief Destroys the MainMenuScene.
   */
  ~MainMenuScene() override = default;

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
   * @return The SceneID (MAIN_MENU).
   */
  [[nodiscard]] SceneID getID() const override;

 private:
  bool is_new_game = false;
};
