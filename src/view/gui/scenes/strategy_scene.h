// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include "view/gui/gui_scene.h"
#include "view/gui/button_manager.h"
#include "model/strategy.h"
#include <string>

class GUIView;

class StrategyScene : public GUIScene {
 public:
  explicit StrategyScene(GUIView* parent);
  ~StrategyScene() override;

  void onEnter() override;
  void onExit() override;
  void handleEvent(const SDL_Event& event) override;
  void update(float deltaTime) override;
  void render() override;
  SceneID getID() const override;

  void onResize(int width, int height) override;

 private:
  GUIView* parent_view;
  ButtonManager button_manager;
  StrategySliders current_sliders;
  int backButtonId = -1;
  int applyButtonId = -1;

  void setupUI();

  // For simplicity, we'll use buttons to increment/decrement values for now
  // A proper slider implementation would be more complex.
  void setupStrategyControls();
  void updateStrategyValue(const std::string& slider_name, float delta);
  void saveStrategy();
  void loadStrategy();

  // Helper to render text for slider values
  void renderSliderValue(const std::string& label, float value, int y_pos);
};
