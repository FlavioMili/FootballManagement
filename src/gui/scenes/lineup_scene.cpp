// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "lineup_scene.h"

#include <imgui.h>

#include <algorithm>
#include <format>
#include <numbers>

#include "global/language_manager.h"
#include "gui/gui_constants.h"
#include "gui/gui_view.h"
#include "model/game.h"
#include "model/role_utils.h"
#include "model/team.h"

namespace
{
constexpr float PITCH_WIDTH = 800.0f;
constexpr float PITCH_HEIGHT = 500.0f;
constexpr float PLAYER_RADIUS = 15.0f;

void renderPlayerTooltip(const Player* p, const StatsConfig& stats_config)
{
  if (ImGui::IsItemHovered())
  {
    ImGui::BeginTooltip();
    ImGui::Text("Age: %d", p->getAge());
    ImGui::Text("Overall: %.1f", p->getOverall(stats_config));

    ImGui::Separator();
    for (const auto& [stat_name, value] : p->getStats())
    {
      ImGui::Text("%s: %.1f", stat_name.c_str(), value);
    }

    if (ImGui::GetIO().KeyShift)
    {
      ImGui::Separator();
      ImGui::Text("Contract: %d years", p->getContractYears());
      ImGui::Text("Wage: $%d", p->getWage());
      ImGui::Text("Height: %d cm", p->getHeight());
      ImGui::Text("Foot: %s", (p->getFoot() == Foot::Right ? "Right" : "Left"));
    }
    else
    {
      ImGui::Separator();
      ImGui::TextDisabled("(Hold SHIFT for more details)");
    }
    ImGui::EndTooltip();
  }
}
}  // namespace

LineupScene::LineupScene(GUIView* parent) : GUIScene(parent) {}

void LineupScene::onEnter() { loadLineup(); }

void LineupScene::update(float deltaTime) { (void)deltaTime; }

void LineupScene::loadLineup()
{
  auto managedTeamOpt = guiView->getController().getManagedTeam();
  if (managedTeamOpt)
  {
    Team& team = managedTeamOpt.value().get();
    current_lineup = &team.getLineup();
  }
}

void LineupScene::render()
{
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->Pos);
  ImGui::SetNextWindowSize(ImGui::GetMainViewport()->Size);
  ImGui::Begin(
      "Lineup", nullptr,
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);

  if (ImGui::Button(LOC("ROSTER_BACK"), ImVec2(GUIConstants::BUTTON_WIDTH,
                                               GUIConstants::BUTTON_HEIGHT)))
  {
    guiView->popScene();
  }
  ImGui::Separator();
  ImGui::Spacing();

  if (!current_lineup)
  {
    ImGui::Text("No lineup found.");
    ImGui::End();
    return;
  }

  // Two columns: Pitch on top, Bench on bottom? Actually let's do Pitch taking
  // up most space, Bench on right
  ImGui::Columns(2, "LineupColumns", false);
  ImGui::SetColumnWidth(0, PITCH_WIDTH + 40.0f);

  renderPitch();

  ImGui::NextColumn();

  renderBench();

  ImGui::Columns(1);
  ImGui::End();
}

void LineupScene::renderPitch()
{
  ImGui::Text("Starting XI (Drag players)");

  ImVec2 p = ImGui::GetCursorScreenPos();
  ImDrawList* draw_list = ImGui::GetWindowDrawList();

  // Draw Pitch background
  ImVec2 pitch_min = p;
  auto pitch_max = ImVec2(p.x + PITCH_WIDTH, p.y + PITCH_HEIGHT);
  draw_list->AddRectFilled(pitch_min, pitch_max,
                           IM_COL32(34, 139, 34, 255));  // Green pitch

  ImU32 line_color = IM_COL32(255, 255, 255, 200);
  draw_list->AddRect(pitch_min, pitch_max, line_color, 0.0f, 0,
                     2.0f);  // White border

  float center_y = pitch_min.y + PITCH_HEIGHT * 0.5f;
  float center_x = pitch_min.x + PITCH_WIDTH * 0.5f;

  // Center line (vertical)
  draw_list->AddLine(ImVec2(center_x, pitch_min.y),
                     ImVec2(center_x, pitch_max.y), line_color, 2.0f);

  // Center circle
  draw_list->AddCircle(ImVec2(center_x, center_y), PITCH_HEIGHT * 0.15f,
                       line_color, 32, 2.0f);
  draw_list->AddCircleFilled(ImVec2(center_x, center_y), 4.0f, line_color);

  // Penalty Boxes
  float pen_box_w = PITCH_WIDTH * 0.15f;
  float pen_box_h = PITCH_HEIGHT * 0.5f;
  float pen_box_y = center_y - pen_box_h * 0.5f;

  // Left Penalty Box
  draw_list->AddRect(ImVec2(pitch_min.x, pen_box_y),
                     ImVec2(pitch_min.x + pen_box_w, pen_box_y + pen_box_h),
                     line_color, 0.0f, 0, 2.0f);
  // Right Penalty Box
  draw_list->AddRect(ImVec2(pitch_max.x - pen_box_w, pen_box_y),
                     ImVec2(pitch_max.x, pen_box_y + pen_box_h), line_color,
                     0.0f, 0, 2.0f);

  // Goal Boxes
  float goal_box_w = PITCH_WIDTH * 0.05f;
  float goal_box_h = PITCH_HEIGHT * 0.25f;
  float goal_box_y = center_y - goal_box_h * 0.5f;

  // Left Goal Box
  draw_list->AddRect(ImVec2(pitch_min.x, goal_box_y),
                     ImVec2(pitch_min.x + goal_box_w, goal_box_y + goal_box_h),
                     line_color, 0.0f, 0, 2.0f);
  // Right Goal Box
  draw_list->AddRect(ImVec2(pitch_max.x - goal_box_w, goal_box_y),
                     ImVec2(pitch_max.x, goal_box_y + goal_box_h), line_color,
                     0.0f, 0, 2.0f);

  // Corner arcs
  float corner_r = PITCH_HEIGHT * 0.03f;
  const float PI = std::numbers::pi_v<float>;
  // Top-left
  draw_list->PathArcTo(ImVec2(pitch_min.x, pitch_min.y), corner_r, 0.0f,
                       PI * 0.5f, 10);
  draw_list->PathStroke(line_color, 0, 2.0f);
  // Top-right
  draw_list->PathArcTo(ImVec2(pitch_max.x, pitch_min.y), corner_r, PI * 0.5f,
                       PI, 10);
  draw_list->PathStroke(line_color, 0, 2.0f);
  // Bottom-left
  draw_list->PathArcTo(ImVec2(pitch_min.x, pitch_max.y), corner_r, -PI * 0.5f,
                       0.0f, 10);
  draw_list->PathStroke(line_color, 0, 2.0f);
  // Bottom-right
  draw_list->PathArcTo(ImVec2(pitch_max.x, pitch_max.y), corner_r, PI,
                       PI * 1.5f, 10);
  draw_list->PathStroke(line_color, 0, 2.0f);

  // Render Goalkeeper (Left side goal)
  if (const Player* gk = current_lineup->getGoalkeeper())
  {
    auto pos = ImVec2(pitch_min.x + PLAYER_RADIUS + 10.0f,
                      pitch_min.y + PITCH_HEIGHT * 0.5f);

    // Invisible button for Drag & Drop
    ImGui::SetCursorScreenPos(
        ImVec2(pos.x - PLAYER_RADIUS, pos.y - PLAYER_RADIUS));
    std::string btn_id = std::format("##player_{}", gk->getId());
    ImGui::InvisibleButton(btn_id.c_str(),
                           ImVec2(PLAYER_RADIUS * 2, PLAYER_RADIUS * 2));

    if (ImGui::IsItemClicked())
    {
      if (selected_player_id == 0)
      {
        selected_player_id = gk->getId();
      }
      else if (selected_player_id == gk->getId())
      {
        selected_player_id = 0;
      }
      else
      {
        current_lineup->swapPlayers(selected_player_id, gk->getId());
        selected_player_id = 0;
      }
    }

    if (selected_player_id == gk->getId())
    {
      draw_list->AddCircle(pos, PLAYER_RADIUS + 3.0f,
                           IM_COL32(255, 255, 0, 255), 0, 2.0f);
    }

    draw_list->AddCircleFilled(pos, PLAYER_RADIUS, IM_COL32(200, 200, 50, 255));

    std::string name_label =
        "[" + RoleUtils::toString(gk->getRole()) + "] " + gk->getName();
    draw_list->AddText(ImVec2(pos.x - 10.0f, pos.y + PLAYER_RADIUS + 2.0f),
                       IM_COL32(255, 255, 255, 255), name_label.c_str());

    renderPlayerTooltip(gk, guiView->getController().getStatsConfig());

    if (ImGui::BeginDragDropTarget())
    {
      if (const ImGuiPayload* payload =
              ImGui::AcceptDragDropPayload("BENCH_PLAYER"))
      {
        IM_ASSERT(payload->DataSize == sizeof(PlayerID));
        PlayerID bench_pid = *(const PlayerID*)payload->Data;
        current_lineup->swapPlayers(bench_pid, gk->getId());
      }
      ImGui::EndDragDropTarget();
    }
  }

  // Render Outfield Players
  const ImGuiIO& io = ImGui::GetIO();
  for (const auto& posPlayer : current_lineup->getOutfieldPlayers())
  {
    if (!posPlayer.player) continue;

    auto player_pos = ImVec2(pitch_min.x + posPlayer.position.x * PITCH_WIDTH,
                             pitch_min.y + posPlayer.position.y * PITCH_HEIGHT);

    // Invisible button for dragging & swapping
    ImGui::SetCursorScreenPos(
        ImVec2(player_pos.x - PLAYER_RADIUS, player_pos.y - PLAYER_RADIUS));
    std::string btn_id = std::format("##player_{}", posPlayer.player->getId());
    ImGui::InvisibleButton(btn_id.c_str(),
                           ImVec2(PLAYER_RADIUS * 2, PLAYER_RADIUS * 2));

    if (ImGui::IsItemClicked())
    {
      if (selected_player_id == 0)
      {
        selected_player_id = posPlayer.player->getId();
      }
      else if (selected_player_id == posPlayer.player->getId())
      {
        selected_player_id = 0;
      }
      else
      {
        current_lineup->swapPlayers(selected_player_id,
                                    posPlayer.player->getId());
        selected_player_id = 0;
      }
    }

    if (selected_player_id == posPlayer.player->getId())
    {
      draw_list->AddCircle(player_pos, PLAYER_RADIUS + 3.0f,
                           IM_COL32(255, 255, 0, 255), 0, 2.0f);
    }

    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
    {
      // Update position based on mouse delta
      ImVec2 delta = io.MouseDelta;
      float new_x = (player_pos.x + delta.x - pitch_min.x) / PITCH_WIDTH;
      float new_y = (player_pos.y + delta.y - pitch_min.y) / PITCH_HEIGHT;

      new_x = std::clamp(new_x, 0.0f, 1.0f);
      new_y = std::clamp(new_y, 0.0f, 1.0f);

      current_lineup->moveOutfieldPlayer(posPlayer.player->getId(),
                                         {new_x, new_y});
      player_pos = ImVec2(pitch_min.x + new_x * PITCH_WIDTH,
                          pitch_min.y + new_y * PITCH_HEIGHT);
    }

    ImU32 color = ImGui::IsItemHovered() ? IM_COL32(100, 100, 255, 255)
                                         : IM_COL32(50, 50, 200, 255);
    draw_list->AddCircleFilled(player_pos, PLAYER_RADIUS, color);

    std::string name_label = "[" +
                             RoleUtils::toString(posPlayer.player->getRole()) +
                             "] " + posPlayer.player->getName();
    draw_list->AddText(
        ImVec2(player_pos.x - 15.0f, player_pos.y + PLAYER_RADIUS + 2.0f),
        IM_COL32(255, 255, 255, 255), name_label.c_str());

    renderPlayerTooltip(posPlayer.player,
                        guiView->getController().getStatsConfig());

    if (ImGui::BeginDragDropTarget())
    {
      if (const ImGuiPayload* payload =
              ImGui::AcceptDragDropPayload("BENCH_PLAYER"))
      {
        IM_ASSERT(payload->DataSize == sizeof(PlayerID));
        PlayerID bench_pid = *(const PlayerID*)payload->Data;
        current_lineup->swapPlayers(bench_pid, posPlayer.player->getId());
      }
      ImGui::EndDragDropTarget();
    }
  }

  ImGui::Dummy(ImVec2(PITCH_WIDTH, PITCH_HEIGHT));
}

void LineupScene::renderBench()
{
  ImGui::Text("Substitutes");
  ImGui::BeginChild("BenchList", ImVec2(0, 0), true);

  for (const Player* p : current_lineup->getReserves())
  {
    if (p)
    {
      std::string label =
          std::format("[{}] {}##{}", RoleUtils::toString(p->getRole()),
                      p->getName(), p->getId());
      if (bool is_selected = (selected_player_id == p->getId());
          ImGui::Selectable(label.c_str(), is_selected))
      {
        if (selected_player_id == 0)
        {
          selected_player_id = p->getId();
        }
        else if (selected_player_id == p->getId())
        {
          selected_player_id = 0;
        }
        else
        {
          current_lineup->swapPlayers(selected_player_id, p->getId());
          selected_player_id = 0;
        }
      }
      renderPlayerTooltip(p, guiView->getController().getStatsConfig());

      if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
      {
        PlayerID pid = p->getId();
        ImGui::SetDragDropPayload("BENCH_PLAYER", &pid, sizeof(PlayerID));
        ImGui::Text("Swap %s", p->getName().c_str());
        ImGui::EndDragDropSource();
      }
      ImGui::Separator();
    }
  }

  ImGui::EndChild();
}
