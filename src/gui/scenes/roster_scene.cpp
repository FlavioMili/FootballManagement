// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "roster_scene.h"

#include <imgui.h>

#include "global/language_manager.h"
#include "gui/gui_constants.h"
#include "gui/gui_view.h"

namespace
{
constexpr float WINDOW_WIDTH = 600.0f;
constexpr float WINDOW_HEIGHT = 600.0f;
constexpr int TABLE_COLUMNS = 3;
constexpr float OVERALL_COLUMN_WIDTH = 60.0f;
}  // namespace

RosterScene::RosterScene(GUIView* parent) : GUIScene(parent) {}

void RosterScene::onEnter() { loadRoster(); }

void RosterScene::update(float deltaTime) { (void)deltaTime; }

void RosterScene::render()
{
  ImGui::SetNextWindowPos(
      ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always,
      ImVec2(GUIConstants::CENTER_PIVOT, GUIConstants::CENTER_PIVOT));
  ImGui::SetNextWindowSize(ImVec2(WINDOW_WIDTH, WINDOW_HEIGHT),
                           ImGuiCond_FirstUseEver);
  ImGui::Begin(LOC("ROSTER_TITLE"), nullptr,
               ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove);

  if (ImGui::Button(LOC("ROSTER_BACK"), ImVec2(GUIConstants::BUTTON_WIDTH,
                                               GUIConstants::BUTTON_HEIGHT)))
  {
    guiView->popScene();
  }
  ImGui::Separator();
  ImGui::Spacing();

  if (ImGui::BeginTable("RosterTable", TABLE_COLUMNS,
                        ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                            ImGuiTableFlags_Resizable))
  {
    ImGui::TableSetupColumn(LOC("ROSTER_COL_NAME"));
    ImGui::TableSetupColumn(LOC("ROSTER_COL_ROLE"));
    ImGui::TableSetupColumn(LOC("ROSTER_COL_OVERALL"),
                            ImGuiTableColumnFlags_WidthFixed,
                            OVERALL_COLUMN_WIDTH);
    ImGui::TableHeadersRow();

    for (const auto& player_ref : roster_players)
    {
      const Player& player = player_ref.get();
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("%s", player.getName().c_str());
      ImGui::TableNextColumn();
      ImGui::Text("%s", player.getRole().c_str());
      ImGui::TableNextColumn();
      ImGui::Text("%.1f",
                  player.getOverall(guiView->getController().getStatsConfig()));
    }
    ImGui::EndTable();
  }

  ImGui::End();
}

void RosterScene::loadRoster()
{
  auto managedTeamOpt = guiView->getController().getManagedTeam();
  if (managedTeamOpt.has_value())
  {
    roster_players = guiView->getController().getPlayersForTeam(
        managedTeamOpt->get().getId());
  }
}

SceneID RosterScene::getID() const { return SceneID::ROSTER; }
