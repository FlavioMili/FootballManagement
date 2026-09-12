// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <vector>

#include "gui/scenes/management_scene.h"
#include "gui/view_models/competition_view.h"

/**
 * @brief Fixtures and results: the managed club's season calendar and the
 * league schedule round by round.
 */
class FixturesScene : public ManagementScene
{
 public:
  explicit FixturesScene(GUIView* parent);

  void update(float deltaTime) override;
  [[nodiscard]] SceneID getID() const override { return SceneID::FIXTURES; }

 protected:
  void renderContent() override;
  [[nodiscard]] NavSection navSection() const override
  {
    return NavSection::FIXTURES;
  }
  void refresh() override;

 private:
  friend class GameFlowTest_GUIFlowLifecycle_Test;

  enum class View : uint8_t
  {
    CLUB,
    LEAGUE
  };

  void renderClubSummary();
  void renderClubFixtures(float height);
  void renderLeagueRound();

  View view = View::CLUB;
  TeamID club_id = 0;
  std::vector<CompetitionView::FixtureRow> club_fixtures;
  std::vector<CompetitionView::FixtureRow> league_fixtures;
  int round_count = 0;
  int selected_round = 1;
  int wins = 0;
  int draws = 0;
  int losses = 0;
  int goals_for = 0;
  int goals_against = 0;
  int next_fixture_index = -1;
  bool scroll_to_next = true;
};
