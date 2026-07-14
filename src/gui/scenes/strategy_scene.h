// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once
#include "gui/gui_scene.h"
#include "model/strategy.h"

/**
 * @brief Scene for managing team strategy.
 */
class StrategyScene : public GUIScene
{
 public:
  /**
   * @brief Constructs a new StrategyScene.
   * @param parent Pointer to the GUIView.
   */
  explicit StrategyScene(GUIView* parent);

  /**
   * @brief Destroys the StrategyScene.
   */
  ~StrategyScene() override = default;

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
   * @return The SceneID (STRATEGY).
   */
  [[nodiscard]] SceneID getID() const override;

 private:
  void saveStrategy();
  void loadStrategy();

  StrategySliders current_sliders;
};
