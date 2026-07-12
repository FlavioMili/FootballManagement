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

class StrategyScene : public GUIScene
{
 public:
  explicit StrategyScene(GUIView* parent);
  ~StrategyScene() override = default;

  void onEnter() override;
  void update(float deltaTime) override;
  void render() override;
  [[nodiscard]] SceneID getID() const override;

 private:
  void saveStrategy();
  void loadStrategy();

  StrategySliders current_sliders;
};
