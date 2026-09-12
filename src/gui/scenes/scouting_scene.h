// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "gui/scenes/management_scene.h"
#include "model/scouting.h"

/**
 * @brief Scouting and recruitment: scouts, assignments, reports, player
 * search on estimated values, shortlist and recruitment focus.
 *
 * Every figure about another club's player comes from the scouting
 * estimates (GameController::getScoutedView / searchScoutedPlayers), never
 * from the hidden true attributes. Rows are rebuilt on refresh or when a
 * filter changes, never per frame.
 */
class ScoutingScene : public ManagementScene
{
 public:
  /** @brief Tabs of the screen. */
  enum class Tab : uint8_t
  {
    OVERVIEW,
    REPORTS,
    SEARCH,
    SHORTLIST,
    FOCUS
  };

  explicit ScoutingScene(GUIView* parent, Tab initialTab = Tab::OVERVIEW);

  void update(float deltaTime) override;
  [[nodiscard]] SceneID getID() const override { return SceneID::SCOUTING; }

 protected:
  void renderContent() override;
  [[nodiscard]] NavSection navSection() const override
  {
    return NavSection::SCOUTING;
  }
  void refresh() override;

 private:
  friend class GameFlowTest_GUIFlowLifecycle_Test;

  /** @brief Display-ready estimated player row. */
  struct PlayerLine
  {
    ScoutedPlayerRow row;
    std::string name;
    std::string name_lower;
    std::string club;
    std::string role;
    std::string ability_range;
    std::string potential_text;
    std::string value_text;
  };

  struct ReportLine
  {
    ScoutReport report;
    std::string player_name;
    std::string club;
    std::string date_text;
    std::string potential_text;
    std::string fee_text;
  };

  struct AssignmentLine
  {
    ScoutAssignment assignment;
    std::string scout_name;
    std::string target_text;
    std::string cost_text;
    std::string period_text;
    std::string started_text;
  };

  struct TargetOption
  {
    ScoutTargetKind kind;
    uint32_t id;
    std::string label;
  };

  struct ComparisonLine
  {
    SquadComparisonRow row;
    std::string role;
    std::string own_name;
    std::string candidate_name;
    std::string potential_text;
  };

  enum class SearchColumn : uint8_t
  {
    NAME,
    CLUB,
    ROLE,
    AGE,
    ABILITY,
    POTENTIAL,
    KNOWLEDGE,
    VALUE
  };

  void rebuildAssignments();
  void rebuildReports();
  void rebuildSearch();
  void rebuildShortlist();
  void rebuildTargets();
  PlayerLine makeLine(const ScoutedPlayerRow& row) const;
  void sortSearch();

  void renderOverview();
  void renderScouts(float width, float height);
  void renderAssignments(float height);
  void renderNewAssignment();
  void renderReports();
  void renderSearch();
  void renderSearchFilters();
  void renderPlayerTable(const char* id, std::vector<PlayerLine>& lines,
                         float height, bool sortable);
  void renderShortlist();
  void renderComparison();
  void renderFocus();
  void renderFocusEditor();
  void renderPlayerActions();

  void selectPlayer(PlayerID playerId, const std::string& name);
  void sendScout(ScoutTargetKind kind, uint32_t targetId);
  bool playerCell(PlayerID playerId, const std::string& name, bool selected);

  std::vector<ScoutProfile> scouts;
  std::vector<AssignmentLine> active_assignments;
  std::vector<AssignmentLine> finished_assignments;
  std::vector<ReportLine> reports;
  std::vector<PlayerLine> search_rows;
  std::vector<PlayerLine> shortlist_rows;
  std::vector<ComparisonLine> comparison;
  std::vector<TargetOption> targets;
  std::vector<std::pair<LeagueID, std::string>> leagues;

  // Assignment form.
  int form_scout = 0;
  int form_target = 0;
  int form_duration = 1;

  // Search filters.
  ScoutSearchFilter filter;
  int filter_role = 0;   /**< 0 = any, else PlayerRole + 1. */
  int filter_league = 0; /**< 0 = all, 1 = free agents, else leagues[i-2]. */
  int filter_min_age = 16;
  int filter_max_age = 40;
  int filter_min_overall = 0;
  int filter_max_value_m = 0; /**< Millions, 0 = any. */
  int filter_min_knowledge = 0;
  std::array<char, 48> name_query{};
  bool search_dirty = true;
  SearchColumn sort_column = SearchColumn::ABILITY;
  bool sort_ascending = false;

  // Focus editor.
  RecruitmentFocus focus_draft;
  int focus_role = 0;
  int focus_min_age = 17;
  int focus_max_age = 30;
  int focus_min_ability = 0;
  int focus_max_fee_k = 0;
  int focus_max_wage = 0;

  PlayerID selected_player = 0;
  std::string selected_name;
  Tab requested_tab;
  bool tab_pending = true;
};
