// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
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

class TeamSelectionScene : public GUIScene
{
 public:
  explicit TeamSelectionScene(GUIView* parent);
  ~TeamSelectionScene() override = default;

  void onEnter() override;
  void update(float deltaTime) override;
  void render() override;
  [[nodiscard]] SceneID getID() const override;

 private:
  void loadAvailableLeagues();
  void loadAvailableTeams();

  std::vector<std::reference_wrapper<const League>> available_leagues;
  std::vector<std::reference_wrapper<const Team>> available_teams;
  std::optional<uint8_t> selected_league_id;
};
