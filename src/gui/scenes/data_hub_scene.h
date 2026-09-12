// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <string>
#include <vector>

#include "controller/game_controller.h"
#include "gui/scenes/management_scene.h"

/**
 * @brief Data hub: the managed club's season in numbers. Team tab: xG
 * trend, league comparison with ranks, shot map and set pieces; Players
 * tab: per-90 table with rating trends and pass involvement. Every figure
 * states its definition and sample size.
 */
class DataHubScene : public ManagementScene
{
 public:
  explicit DataHubScene(GUIView* parent);

  void update(float /*deltaTime*/) override {}
  [[nodiscard]] SceneID getID() const override { return SceneID::DATA_HUB; }

 protected:
  void renderContent() override;
  [[nodiscard]] NavSection navSection() const override
  {
    return NavSection::DATA_HUB;
  }
  void refresh() override;

 private:
  friend class GameFlowTest_GUIFlowLifecycle_Test;

  struct PlayerRow
  {
    PlayerID id = 0;
    std::string name;
    std::string role;
    PlayerAnalyticsRow stats;
  };

  void renderTeam();
  void renderPlayers();
  void renderTrendChart(float width);
  void renderShotMap(float width);
  void renderSetPieces(float width);
  void renderComparison(float width);

  GameController::DataHubView hub;
  std::vector<PlayerRow> players;
  std::vector<std::string> opponent_names; /*!< Per trend point. */
  int tab = 0;
  int shot_side = 0; /*!< 0 = shots for, 1 = shots against. */
};
