// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once
#include <imgui.h>

#include <array>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "gui/scenes/management_scene.h"
#include "gui/view_models/player_view.h"
#include "model/player.h"

/**
 * @brief Squad list of the managed club, or of any other club (read-only).
 */
class RosterScene : public ManagementScene
{
 public:
  /**
   * @brief Constructs a new RosterScene for the managed club.
   * @param parent Pointer to the GUIView.
   */
  explicit RosterScene(GUIView* parent);

  /**
   * @brief Constructs a RosterScene for a specific club.
   * @param parent Pointer to the GUIView.
   * @param teamId Club to list; other clubs are shown read-only.
   */
  RosterScene(GUIView* parent, TeamID teamId);

  /**
   * @brief Destroys the RosterScene.
   */
  ~RosterScene() override = default;

  /**
   * @brief Updates scene logic.
   * @param deltaTime Time elapsed since last update.
   */
  void update(float deltaTime) override;

  /**
   * @brief Gets the ID of this scene.
   * @return The SceneID (ROSTER).
   */
  [[nodiscard]] SceneID getID() const override;

 protected:
  void renderContent() override;
  [[nodiscard]] NavSection navSection() const override;
  void refresh() override { loadRoster(); }

 private:
  friend class GameFlowTest_GUIFlowLifecycle_Test;
  void loadRoster();
  void renderSummary();
  void renderFilters();
  void renderTable(float height);
  void renderDetails(float height);
  void applyFilter();
  void applySort();
  [[nodiscard]] const Player* selectedPlayer() const;
  [[nodiscard]] bool isManagedClub() const;

  std::optional<TeamID> team_id;
  std::vector<std::reference_wrapper<const Player>> roster_players;
  std::optional<PlayerID> selected_player_id;
  std::array<char, 64> search_text{};
  int role_filter_index = 0;
  int group_filter_index = 0;

  // View models rebuilt by loadRoster(); visible_rows is filtered and sorted.
  std::vector<PlayerView::PlayerRow> rows;
  std::vector<size_t> visible_rows;
  std::unordered_set<PlayerID> starters;
  std::string filtered_search;
  int filtered_role = -1;
  int filtered_group = -1;
  ImGuiID sort_column = 0;
  bool sort_ascending = true;
  std::string club_name;
  double average_age = 0.0;
  double average_overall = 0.0;
  int64_t payroll = 0;
  int expiring_contracts = 0;
  bool show_details = false;
};
