// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "team_selection_scene.h"

#include <imgui.h>

#include "database/gamedata.h"
#include "global/global.h"
#include "global/language_manager.h"
#include "gui/gui_constants.h"
#include "gui/gui_view.h"

namespace {
constexpr float WINDOW_WIDTH = 800.0f;
constexpr float WINDOW_HEIGHT = 600.0f;
constexpr float LEAGUES_REGION_HEIGHT = 100.0f;
constexpr int TEAM_COLUMNS = 4;
}  // namespace

TeamSelectionScene::TeamSelectionScene(GUIView* parent) : GUIScene(parent) {}

void TeamSelectionScene::onEnter() { loadAvailableLeagues(); }

void TeamSelectionScene::update(float deltaTime) { (void)deltaTime; }

void TeamSelectionScene::render() {
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always,
                          ImVec2(GUIConstants::CENTER_PIVOT, GUIConstants::CENTER_PIVOT));
  ImGui::SetNextWindowSize(ImVec2(WINDOW_WIDTH, WINDOW_HEIGHT), ImGuiCond_FirstUseEver);
  ImGui::Begin(LOC("TEAM_SELECTION_TITLE"), nullptr, ImGuiWindowFlags_NoCollapse);

  ImGui::Text("%s", LOC("TEAM_SELECTION_LEAGUES"));
  ImGui::Separator();

  ImGui::BeginChild("LeaguesRegion", ImVec2(0, LEAGUES_REGION_HEIGHT), true,
                    ImGuiWindowFlags_HorizontalScrollbar);
  for (const auto& league_ref : available_leagues) {
    const League& league = league_ref.get();
    bool is_selected = (selected_league_id && selected_league_id.value() == league.getId());
    if (is_selected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));

    if (ImGui::Button(league.getName().c_str(),
                      ImVec2(GUIConstants::MENU_BUTTON_WIDTH, GUIConstants::MENU_BUTTON_HEIGHT))) {
      selected_league_id = league.getId();
      loadAvailableTeams();
    }

    if (is_selected) ImGui::PopStyleColor();
    ImGui::SameLine();
  }
  ImGui::EndChild();

  ImGui::Spacing();
  ImGui::Text("%s", LOC("TEAM_SELECTION_TEAMS"));
  ImGui::Separator();

  if (selected_league_id) {
    ImGui::BeginChild("TeamsRegion", ImVec2(0, 0), true);

    if (ImGui::BeginTable("TeamsTable", TEAM_COLUMNS)) {
      for (size_t i = 0; i < available_teams.size(); i++) {
        ImGui::TableNextColumn();
        const Team& team = available_teams[i].get();
        if (ImGui::Button(team.getName().c_str(),
                          ImVec2(-FLT_MIN, GUIConstants::MENU_BUTTON_HEIGHT))) {
          guiView->getController().selectManagedTeam(team.getId());
          guiView->popScene();  // Assumes TeamSelection is overlay
        }
      }
      ImGui::EndTable();
    }
    ImGui::EndChild();
  } else {
    ImGui::Text("%s", LOC("TEAM_SELECTION_SELECT_LEAGUE_FIRST"));
  }

  ImGui::End();
}

void TeamSelectionScene::loadAvailableLeagues() {
  available_leagues = guiView->getController().getLeagues();
}

void TeamSelectionScene::loadAvailableTeams() {
  if (!selected_league_id) return;
  auto all_teams = guiView->getController().getTeams();
  available_teams.clear();
  for (const auto& team_ref : all_teams) {
    const Team& team = team_ref.get();
    if (team.getLeagueId() == selected_league_id.value() &&
        team.getName() != FREE_AGENTS_TEAM_NAME) {
      available_teams.push_back(team_ref);
    }
  }
}

SceneID TeamSelectionScene::getID() const { return SceneID::TEAM_SELECTION; }
