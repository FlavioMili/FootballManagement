// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <future>
#include <optional>
#include <string>
#include <vector>

#include "gui/gui_scene.h"
#include "gui/gui_view.h"
#include "gui/scenes/management_scene.h"
#include "gui/view_models/competition_view.h"
#include "gui/view_models/player_view.h"
#include "model/finances.h"

/**
 * @brief Club hub: the base scene of a career.
 *
 * Hosts the Home dashboard and the Finances page, and owns the career clock
 * (Continue / Play Match). Other management screens are opened above it and
 * drive the clock through requestContinue().
 */
class MainGameScene : public ManagementScene
{
 public:
  /** @brief Pages hosted directly by the club hub. */
  enum class Page
  {
    OVERVIEW,
    FINANCES
  };

  /**
   * @brief Constructs a new MainGameScene.
   * @param guiView_ptr Pointer to the GUIView.
   */
  explicit MainGameScene(GUIView* guiView_ptr);

  /**
   * @brief Destroys the MainGameScene.
   */
  ~MainGameScene() override = default;

  /**
   * @brief Called when entering the scene.
   */
  void onEnter() override;

  /**
   * @brief Updates scene logic.
   * @param deltaTime Time elapsed since last update.
   */
  void update(float deltaTime) override;

  /**
   * @brief Gets the ID of this scene.
   * @return The SceneID (GAME_MENU).
   */
  [[nodiscard]] SceneID getID() const override;

  /** @brief True while days are being simulated in the background. */
  [[nodiscard]] bool isAdvancing() const { return continuation_running; }

  /** @brief Label of the context-aware Continue / Play Match button. */
  [[nodiscard]] std::string continueLabel() const;

  /**
   * @brief Plays today's match, or returns to the hub and simulates days up
   * to the next managed fixture.
   */
  void requestContinue();

  /** @brief Switches the page shown by the hub. */
  void showPage(Page page) { active_page = page; }

 protected:
  void renderContent() override;
  [[nodiscard]] NavSection navSection() const override;
  void refresh() override { refreshData(); }

 private:
  friend class GameFlowTest_GUIFlowLifecycle_Test;

  void renderOverview();
  void renderFinances();
  void renderNextMatchCard(float width, float height);
  void renderStandingsCard(float width, float height);
  void renderRecentResultsCard(float width, float height);
  void renderKeyPlayersCard(float width, float height);
  void startContinuation();

  void refreshData();

  Page active_page = Page::OVERVIEW;
  std::vector<std::pair<int, int>> cached_standings;
  std::vector<std::reference_wrapper<const Player>> cached_top_players;
  int cached_season = -1;
  GameDateValue cached_date;
  std::future<int> continue_operation;
  bool continuation_running = false;
  bool continuation_requested = false;

  // Dashboard view models, rebuilt by refreshData().
  std::vector<CompetitionView::StandingRow> cached_table;
  CompetitionView::Zones cached_zones;
  std::vector<CompetitionView::FixtureRow> cached_recent;
  std::optional<CompetitionView::FixtureRow> cached_next;
  std::vector<PlayerView::PlayerRow> cached_squad;
  int64_t cached_payroll = 0;
  int64_t cached_squad_value = 0;
  float cached_average_overall = 0.0f;

  // Finance page view models.
  struct FinanceBar
  {
    const char* label_key; /**< Localised at render time. */
    int64_t amount;
    std::string amount_text;
  };
  struct LedgerRow
  {
    std::string date;
    const char* category_key; /**< Localised at render time. */
    int64_t amount;
    std::string amount_text;
  };
  FinanceSummary cached_season_summary;
  std::vector<FinanceBar> cached_finance_bars;
  std::vector<FinanceBar> cached_month_bars;
  bool finance_show_month = false;
  std::vector<LedgerRow> cached_ledger; /**< Newest first. */
  std::vector<float> cached_balance_trend;
  float budget_shift = 0.0f;
};
