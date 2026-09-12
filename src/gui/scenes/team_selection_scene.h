// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once
#include <imgui.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "gui/gui_scene.h"
#include "model/league.h"
#include "model/team.h"

/**
 * @brief Scene for selecting a team.
 */
class TeamSelectionScene : public GUIScene
{
 public:
  /**
   * @brief Constructs a new TeamSelectionScene.
   * @param parent Pointer to the GUIView.
   */
  explicit TeamSelectionScene(GUIView* parent);

  /**
   * @brief Destroys the TeamSelectionScene.
   */
  ~TeamSelectionScene() override = default;

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
   * @brief Renders the scene.
   */
  void render() override;

  /**
   * @brief Gets the ID of this scene.
   * @return The SceneID (TEAM_SELECTION).
   */
  [[nodiscard]] SceneID getID() const override;

 private:
  /** @brief Cached, display-ready summary of one selectable club. */
  struct ClubSummary
  {
    TeamID id = 0;
    std::string name;
    uint8_t reputation = 0;
    uint32_t stadium = 0;
    size_t squad_size = 0;
    float average_overall = 0.0f;
    float best_overall = 0.0f;
    int64_t balance = 0;
    int64_t weekly_wages = 0;
    int expected_position = 0;
    const char* objective_key = "";
    int difficulty = 0; /**< 0 easy .. 3 very hard. */
    uint32_t shirt = 0;
    uint32_t trim = 0;
    std::string balance_text;
    std::string wages_text;
    std::string stadium_text;
    std::vector<std::pair<std::string, float>> key_players;
  };

  /** @brief One entry of the grouped league list. */
  struct LeagueEntry
  {
    LeagueID id = 0;
    std::string name;
    uint8_t tier = 1;
  };

  void loadAvailableLeagues();
  void loadAvailableTeams();
  void sortClubs();
  void renderLeagueList(float width, float height);
  void renderClubTable(float height);
  void renderSelectedClub(float width, float height);
  void startCareer(TeamID teamId);

  std::vector<LeagueEntry> league_entries;
  std::vector<std::reference_wrapper<const Team>> available_teams;
  std::optional<uint8_t> selected_league_id;
  std::optional<TeamID> selected_team_id;
  std::vector<ClubSummary> club_summaries;
  ImGuiID sort_column = 0;
  bool sort_ascending = false;
};
