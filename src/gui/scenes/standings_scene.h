// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "gui/scenes/management_scene.h"
#include "gui/view_models/competition_view.h"
#include "model/competition.h"

/**
 * @brief Competitions: the full table of any league (form, promotion and
 * relegation zones, top scorers) or the draw of any national cup.
 */
class StandingsScene : public ManagementScene
{
 public:
  explicit StandingsScene(GUIView* parent);

  void update(float deltaTime) override;
  [[nodiscard]] SceneID getID() const override { return SceneID::STANDINGS; }

 protected:
  void renderContent() override;
  [[nodiscard]] NavSection navSection() const override
  {
    return NavSection::STANDINGS;
  }
  void refresh() override;

 private:
  friend class GameFlowTest_GUIFlowLifecycle_Test;
  friend class GameFlowTest_ManagementScreensMidSeason_Test;
  /** @brief One formatted top-scorer line. */
  struct ScorerRow
  {
    PlayerID id = 0;
    std::string name;
    std::string club;
    uint16_t goals = 0;
    uint16_t assists = 0;
  };

  void renderCompetitionSelector();
  void renderHighlights();
  void renderTable(float width, float height);
  void renderScorers(float width, float height);
  void renderCup();

  LeagueID league_id = 0;
  bool league_chosen = false;
  bool showing_cup = false;
  LeagueID cup_id = 0;
  std::vector<CompetitionView::StandingRow> table;
  CompetitionView::Zones zones;
  std::vector<ScorerRow> scorers;
  std::optional<Competitions::CupStatus> cup;
  std::vector<std::pair<LeagueID, std::string>> cup_names;
};
