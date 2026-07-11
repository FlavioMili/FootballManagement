// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include "gui/gui_scene.h"
#include "gui/gui_view.h"

class MainMenuScene : public GUIScene
{
 public:
  explicit MainMenuScene(GUIView* guiView_ptr);
  ~MainMenuScene() override = default;

  void update(float deltaTime) override;
  void render() override;
  [[nodiscard]] SceneID getID() const override;
};
