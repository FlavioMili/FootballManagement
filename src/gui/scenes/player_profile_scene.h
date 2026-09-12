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

#include "gui/scenes/management_scene.h"
#include "gui/view_models/player_view.h"
#include "model/player.h"
#include "model/season_history.h"

/**
 * @brief Full profile of one player: bio and contract, fitness and form,
 * attributes grouped by category, position suitability, season and career
 * statistics, and context actions (transfer list, lineup, contract).
 */
class PlayerProfileScene : public ManagementScene
{
 public:
  PlayerProfileScene(GUIView* parent, PlayerID playerId);

  void update(float deltaTime) override;
  [[nodiscard]] SceneID getID() const override
  {
    return SceneID::PLAYER_PROFILE;
  }

 protected:
  void renderContent() override;
  [[nodiscard]] NavSection navSection() const override;
  void refresh() override;

 private:
  friend class GameFlowTest_GUIFlowLifecycle_Test;
  friend class GameFlowTest_ManagementScreensMidSeason_Test;

  struct AttributeLine
  {
    std::string name;
    float value = 0.0f;
  };

  struct AttributeSection
  {
    const char* title_key;
    std::vector<AttributeLine> lines;
  };

  /** @brief One statistics row (a competition this season, or a season). */
  struct StatsRow
  {
    std::string label;
    PlayerSeasonStats stats;
  };

  void renderHeader();
  void renderBio(const Player& player, float width, float height);
  void renderStatus(float width, float height);
  void renderAttributes(float width, float height);
  void renderSuitability(float width, float height);
  void renderStatistics(float width, float height);
  void renderDialogs();
  [[nodiscard]] const Player* player() const;
  [[nodiscard]] bool isOwnPlayer() const;

  PlayerID player_id;
  PlayerView::PlayerRow row;
  std::string club_name;
  std::string nationality;
  std::vector<PlayerView::RoleFit> fits;
  std::vector<AttributeSection> sections;
  std::vector<StatsRow> season_rows;
  std::vector<StatsRow> career_rows;

  // Fitness, availability and outlook, cached on refresh.
  PlayerDynamics dynamics;
  float form = 0.0f;
  uint8_t league_ban = 0;
  uint8_t cup_ban = 0;
  const char* squad_role_key = nullptr;
  float potential_low = 0.0f;
  float potential_high = 0.0f;

  bool show_career = false;
  bool list_confirm_requested = false;
  bool renew_requested = false;
  float renew_wage = 0.0f;
  int renew_years = 0;
  std::string renew_status;
};
