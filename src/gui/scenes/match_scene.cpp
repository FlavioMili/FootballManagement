// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "match_scene.h"

#include <imgui.h>

#include <format>
#include <numbers>

#include "gui/gui_view.h"
#include "model/role_utils.h"
#include "model/team.h"

MatchScene::MatchScene(GUIView* guiView_ptr, uint16_t home_id, uint16_t away_id)
    : GUIScene(guiView_ptr), home_team_id(home_id), away_team_id(away_id)
{
}

SceneID MatchScene::getID() const
{
  return SceneID::GAME_MENU; /* Re-use for now */
}

void MatchScene::onEnter()
{
  auto home_opt = guiView->getController().getTeamById(home_team_id);
  auto away_opt = guiView->getController().getTeamById(away_team_id);

  if (home_opt && away_opt)
  {
    const Team& home_team = home_opt->get();
    const Team& away_team = away_opt->get();

    home_name = home_team.getName();
    away_name = away_team.getName();

    engine = std::make_unique<MatchEngine>(
        home_team.getLineup(), away_team.getLineup(), home_team.getStrategy(),
        away_team.getStrategy(), guiView->getController().getStatsConfig());
  }
}

void MatchScene::update(float deltaTime)
{
  if (engine && !match_finished && !is_paused)
  {
    engine->update(deltaTime * match_speed);
    if (engine->getMatchTimeMinutes() >= 90.0f)
    {
      match_finished = true;
    }
  }
}

void MatchScene::render()
{
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

  ImGui::Begin("MatchScene", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove);

  if (!engine)
  {
    ImGui::Text("Error loading match data.");
    if (ImGui::Button("Back"))
    {
      guiView->popScene();
    }
    ImGui::End();
    return;
  }

  // Scoreboard
  ImGui::SetWindowFontScale(1.5f);
  ImGui::Text("%s %d - %d %s", home_name.c_str(), engine->getHomeScore(),
              engine->getAwayScore(), away_name.c_str());
  ImGui::SameLine(ImGui::GetWindowWidth() - 150.0f);
  ImGui::Text("Time: %02d:%02d", (int)engine->getMatchTimeMinutes(),
              (int)((engine->getMatchTimeMinutes() -
                     (int)engine->getMatchTimeMinutes()) *
                    60));
  ImGui::SetWindowFontScale(1.0f);

  if (ImGui::Button(is_paused ? "Resume" : "Pause", ImVec2(100, 30)))
  {
    is_paused = !is_paused;
  }
  ImGui::SameLine();
  ImGui::SetNextItemWidth(150.0f);
  ImGui::SliderFloat("Speed", &match_speed, 0.5f, 5.0f, "%.1fx");
  ImGui::SameLine();
  if (ImGui::Button("Substitutions", ImVec2(120, 30)))
  {
    show_substitutions = true;
    is_paused = true;  // Auto-pause when substituting
    selected_pitch_player = 0;
    selected_bench_player = 0;
  }

  ImGui::Separator();

  if (show_substitutions)
  {
    renderSubstitutionsModal();
  }

  // Pitch Rendering (Vertical pitch, goals at top/bottom)
  ImVec2 p = ImGui::GetCursorScreenPos();
  float pitch_width = 800.0f;
  float pitch_height = 500.0f;

  ImDrawList* draw_list = ImGui::GetWindowDrawList();

  // Keep pitch centered horizontally if window is large enough
  if (float window_width = ImGui::GetWindowWidth(); window_width > pitch_width)
  {
    p.x += (window_width - pitch_width) * 0.5f;
  }

  ImVec2 p_min = p;
  auto p_max = ImVec2(p.x + pitch_width, p.y + pitch_height);

  ImU32 line_color = IM_COL32(255, 255, 255, 200);

  // Grass
  draw_list->AddRectFilled(p_min, p_max, IM_COL32(34, 139, 34, 255));
  // Border
  draw_list->AddRect(p_min, p_max, line_color, 0.0f, 0, 2.0f);

  float center_y = p_min.y + pitch_height * 0.5f;
  float center_x = p_min.x + pitch_width * 0.5f;
  // Center line (vertical)
  draw_list->AddLine(ImVec2(center_x, p_min.y), ImVec2(center_x, p_max.y),
                     line_color, 2.0f);

  // Center circle
  draw_list->AddCircle(ImVec2(center_x, center_y), pitch_height * 0.15f,
                       line_color, 32, 2.0f);
  draw_list->AddCircleFilled(ImVec2(center_x, center_y), 4.0f, line_color);

  // Penalty Boxes
  float pen_box_w = pitch_width * 0.15f;
  float pen_box_h = pitch_height * 0.5f;
  float pen_box_y = center_y - pen_box_h * 0.5f;

  // Left Penalty Box
  draw_list->AddRect(ImVec2(p_min.x, pen_box_y),
                     ImVec2(p_min.x + pen_box_w, pen_box_y + pen_box_h),
                     line_color, 0.0f, 0, 2.0f);
  // Right Penalty Box
  draw_list->AddRect(ImVec2(p_max.x - pen_box_w, pen_box_y),
                     ImVec2(p_max.x, pen_box_y + pen_box_h), line_color, 0.0f,
                     0, 2.0f);

  // Goal Boxes
  float goal_box_w = pitch_width * 0.05f;
  float goal_box_h = pitch_height * 0.25f;
  float goal_box_y = center_y - goal_box_h * 0.5f;

  // Left Goal Box
  draw_list->AddRect(ImVec2(p_min.x, goal_box_y),
                     ImVec2(p_min.x + goal_box_w, goal_box_y + goal_box_h),
                     line_color, 0.0f, 0, 2.0f);
  // Right Goal Box
  draw_list->AddRect(ImVec2(p_max.x - goal_box_w, goal_box_y),
                     ImVec2(p_max.x, goal_box_y + goal_box_h), line_color, 0.0f,
                     0, 2.0f);

  // Corner arcs
  float corner_r = pitch_height * 0.03f;
  const float PI = std::numbers::pi_v<float>;
  // Top-left
  draw_list->PathArcTo(ImVec2(p_min.x, p_min.y), corner_r, 0.0f, PI * 0.5f, 10);
  draw_list->PathStroke(line_color, 0, 2.0f);
  // Top-right
  draw_list->PathArcTo(ImVec2(p_max.x, p_min.y), corner_r, PI * 0.5f, PI, 10);
  draw_list->PathStroke(line_color, 0, 2.0f);
  // Bottom-left
  draw_list->PathArcTo(ImVec2(p_min.x, p_max.y), corner_r, -PI * 0.5f, 0.0f,
                       10);
  draw_list->PathStroke(line_color, 0, 2.0f);
  // Bottom-right
  draw_list->PathArcTo(ImVec2(p_max.x, p_max.y), corner_r, PI, PI * 1.5f, 10);
  draw_list->PathStroke(line_color, 0, 2.0f);

  // Players
  for (const auto& mp : engine->getPlayers())
  {
    auto pos = ImVec2(p_min.x + mp.position.x * pitch_width,
                      p_min.y + mp.position.y * pitch_height);
    ImU32 color =
        mp.isHomeTeam ? IM_COL32(50, 50, 200, 255) : IM_COL32(200, 50, 50, 255);
    draw_list->AddCircleFilled(pos, 8.0f, color);

    // Draw Name
    if (mp.player)
    {
      draw_list->AddText(ImVec2(pos.x + 10, pos.y - 5),
                         IM_COL32(255, 255, 255, 255),
                         mp.player->getName().c_str());
    }
  }

  // Ball
  Vector2F bpos = engine->getBall().position;
  auto ball_pos =
      ImVec2(p_min.x + bpos.x * pitch_width, p_min.y + bpos.y * pitch_height);
  draw_list->AddCircleFilled(ball_pos, 4.0f, IM_COL32(255, 255, 255, 255));

  ImGui::Dummy(ImVec2(pitch_width, pitch_height));

  ImGui::Separator();

  // Events Log
  ImGui::BeginChild("Events", ImVec2(0, 150), true);
  for (const auto& ev : engine->getEvents())
  {
    ImGui::Text("[%02d'] %s", (int)ev.timeMinute, ev.description.c_str());
  }
  // Auto-scroll
  if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
    ImGui::SetScrollHereY(1.0f);
  ImGui::EndChild();

  if (match_finished && ImGui::Button("Finish Match", ImVec2(150, 40)))
  {
    guiView->getController().setMatchResult(
        guiView->getController().getCurrentDate(), home_team_id, away_team_id,
        engine->getHomeScore(), engine->getAwayScore());
    guiView->popScene();  // Pop MatchScene
    // Also advance day now that match is watched
    guiView->getController().advanceDay();
  }

  ImGui::End();
}

void MatchScene::renderSubstitutionsModal()
{
  ImGui::OpenPopup("Substitutions");
  if (ImGui::BeginPopupModal("Substitutions", &show_substitutions,
                             ImGuiWindowFlags_AlwaysAutoResize))
  {
    auto managed_opt = guiView->getController().getManagedTeam();
    if (!managed_opt)
    {
      ImGui::EndPopup();
      return;
    }
    Team& managed_team = managed_opt->get();
    Lineup& lineup = managed_team.getLineup();

    ImGui::Text(
        "Select one player from the pitch and one from the bench to swap.");
    ImGui::Separator();

    ImGui::Columns(2, "SubCols", true);
    ImGui::Text("On Pitch");
    ImGui::Separator();

    // GK
    if (lineup.getGoalkeeper())
    {
      bool selected =
          (selected_pitch_player == lineup.getGoalkeeper()->getId());
      std::string label =
          std::format("GK - {}##{}", lineup.getGoalkeeper()->getName(),
                      lineup.getGoalkeeper()->getId());
      if (ImGui::Selectable(label.c_str(), selected))
      {
        selected_pitch_player = selected ? 0 : lineup.getGoalkeeper()->getId();
      }
    }
    for (const auto& posPlayer : lineup.getOutfieldPlayers())
    {
      if (!posPlayer.player) continue;
      bool selected = (selected_pitch_player == posPlayer.player->getId());
      std::string label = std::format(
          "{} - {}##{}", RoleUtils::toString(posPlayer.player->getRole()),
          posPlayer.player->getName(), posPlayer.player->getId());
      if (ImGui::Selectable(label.c_str(), selected))
      {
        selected_pitch_player = selected ? 0 : posPlayer.player->getId();
      }
    }

    ImGui::NextColumn();
    ImGui::Text("Bench");
    ImGui::Separator();

    for (const auto& res : lineup.getReserves())
    {
      if (!res) continue;
      bool selected = (selected_bench_player == res->getId());
      std::string label =
          std::format("{} - {}##{}", RoleUtils::toString(res->getRole()),
                      res->getName(), res->getId());
      if (ImGui::Selectable(label.c_str(), selected))
      {
        selected_bench_player = selected ? 0 : res->getId();
      }
    }

    ImGui::Columns(1);
    ImGui::Separator();

    if (ImGui::Button("Swap Selected", ImVec2(120, 30)) &&
        selected_pitch_player != 0 && selected_bench_player != 0)
    {
      const Player* bench_ptr = nullptr;
      for (auto p : lineup.getReserves())
      {
        if (p && p->getId() == selected_bench_player) bench_ptr = p;
      }

      if (bench_ptr &&
          lineup.swapPlayers(selected_bench_player, selected_pitch_player))
      {
        engine->substitutePlayer(selected_pitch_player, bench_ptr);
        selected_pitch_player = 0;
        selected_bench_player = 0;
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Close", ImVec2(120, 30)))
    {
      show_substitutions = false;
    }

    ImGui::EndPopup();
  }
}
