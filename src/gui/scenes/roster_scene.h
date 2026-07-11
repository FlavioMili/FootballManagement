// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once
#include "gui/gui_scene.h"
#include "model/player.h"
#include <vector>

class RosterScene : public GUIScene
{
 public:
  explicit RosterScene(GUIView* parent);
  ~RosterScene() override = default;

  void onEnter() override;
  void update(float deltaTime) override;
  void render() override;
  [[nodiscard]] SceneID getID() const override;

 private:
  void loadRoster();
  std::vector<std::reference_wrapper<const Player>> roster_players;
};
