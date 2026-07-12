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

class MainGameScene : public GUIScene
{
 public:
  explicit MainGameScene(GUIView* guiView_ptr);
  ~MainGameScene() override = default;

  void onEnter() override;
  void update(float deltaTime) override;
  void render() override;
  [[nodiscard]] SceneID getID() const override;

 private:
  void renderSidebar();
  void renderMainArea();
  void renderTopBar();
};
