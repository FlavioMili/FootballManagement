// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once
#include "gui/scenes/management_scene.h"
#include "model/strategy.h"

/**
 * @brief Scene for managing team strategy.
 */
class StrategyScene : public ManagementScene
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
   * @brief Updates scene logic.
   * @param deltaTime Time elapsed since last update.
   */
  void update(float deltaTime) override;

  /**
   * @brief Gets the ID of this scene.
   * @return The SceneID (STRATEGY).
   */
  [[nodiscard]] SceneID getID() const override;

 protected:
  void renderContent() override;
  [[nodiscard]] NavSection navSection() const override
  {
    return NavSection::TACTICS;
  }
  void refresh() override { loadStrategy(); }

 private:
  void renderInstructions(float width);
  void renderSummary(float width);
  [[nodiscard]] bool hasUnsavedChanges() const;
  void saveStrategy();
  void loadStrategy();

  StrategySliders current_sliders;
  StrategySliders saved_sliders;
  int selected_preset = -1;
};
