// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <imgui.h>

#include <algorithm>
#include <format>
#include <string>

#include "global/language_manager.h"
#include "global/stats_config.h"
#include "model/player.h"
#include "model/role_utils.h"

namespace PlayerUI
{
inline ImVec4 ratingColor(double rating)
{
  if (rating >= 70.0) return ImVec4(0.25f, 0.78f, 0.48f, 1.0f);
  if (rating >= 55.0) return ImVec4(0.95f, 0.72f, 0.25f, 1.0f);
  return ImVec4(0.92f, 0.38f, 0.36f, 1.0f);
}

inline void metricCard(const char* id, const char* label,
                       const std::string& value, const ImVec4& accent,
                       float width)
{
  ImGui::PushID(id);
  ImGui::BeginChild("card", ImVec2(width, 66.0f), true,
                    ImGuiWindowFlags_NoScrollbar);
  ImGui::TextColored(accent, "%s", value.c_str());
  ImGui::TextDisabled("%s", label);
  ImGui::EndChild();
  ImGui::PopID();
}

inline void detailPanel(const char* id, const Player* player,
                        const StatsConfig& statsConfig,
                        const Player* comparison = nullptr, float height = 0.0f)
{
  ImGui::BeginChild(id, ImVec2(0.0f, height), true);
  if (!player)
  {
    ImGui::PushStyleColor(ImGuiCol_Text,
                          ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%s", LOC("PLAYER_SELECT_PROMPT"));
    ImGui::PopStyleColor();
    ImGui::EndChild();
    return;
  }

  const double overall = player->getOverall(statsConfig);
  ImGui::TextColored(ratingColor(overall), "%.1f", overall);
  ImGui::SameLine();
  ImGui::Text("%s", player->getName().c_str());
  ImGui::PushStyleColor(ImGuiCol_Text,
                        ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
  ImGui::TextWrapped("%s  |  %s %d  |  %d cm  |  %s",
                     RoleUtils::toString(player->getRole()).c_str(),
                     LOC("PLAYER_AGE"), player->getAge(), player->getHeight(),
                     player->getFoot() == Foot::Right
                         ? LOC("PLAYER_FOOT_RIGHT")
                         : LOC("PLAYER_FOOT_LEFT"));
  ImGui::PopStyleColor();
  ImGui::Separator();
  ImGui::Text("%s: %u", LOC("PLAYER_WEEKLY_WAGE"), player->getWage());
  const ImVec4 contractColor = player->getContractYears() <= 1
                                   ? ImVec4(0.92f, 0.38f, 0.36f, 1.0f)
                                   : ImVec4(0.45f, 0.65f, 0.85f, 1.0f);
  ImGui::TextColored(contractColor, "%s: %u", LOC("PLAYER_CONTRACT"),
                     static_cast<unsigned>(player->getContractYears()));

  if (comparison)
  {
    const double delta = overall - comparison->getOverall(statsConfig);
    ImGui::TextColored(delta >= 0.0 ? ImVec4(0.25f, 0.78f, 0.48f, 1.0f)
                                    : ImVec4(0.92f, 0.38f, 0.36f, 1.0f),
                       "%s: %+.1f", LOC("PLAYER_OVERALL_CHANGE"), delta);
    if (player->getRole() == comparison->getRole())
    {
      ImGui::TextColored(ImVec4(0.25f, 0.78f, 0.48f, 1.0f), "%s",
                         LOC("PLAYER_ROLE_MATCH"));
    }
    else
    {
      ImGui::TextColored(ImVec4(0.95f, 0.72f, 0.25f, 1.0f), "%s: %s -> %s",
                         LOC("PLAYER_ROLE_CHANGE"),
                         RoleUtils::toString(comparison->getRole()).c_str(),
                         RoleUtils::toString(player->getRole()).c_str());
    }
  }

  ImGui::SeparatorText(LOC("PLAYER_ATTRIBUTES"));
  for (const auto& [name, value] : player->getStats())
  {
    const float normalized = std::clamp(value / 100.0f, 0.0f, 1.0f);
    const float comparisonValue =
        comparison && comparison->getStats().contains(name)
            ? comparison->getStats().at(name)
            : 0.0f;
    const float delta = value - comparisonValue;
    const std::string valueText = std::format("{:.0f}", value);
    const std::string deltaText = std::format("{:+.0f}", delta);
    const float valueWidth = ImGui::CalcTextSize(valueText.c_str()).x;
    const float deltaWidth =
        comparison ? ImGui::CalcTextSize(deltaText.c_str()).x : 0.0f;
    const float textSpacing =
        comparison ? ImGui::GetStyle().ItemSpacing.x : 0.0f;
    ImGui::TextUnformatted(name.c_str());
    const float rightAlignedX = ImGui::GetCursorPosX() +
                                ImGui::GetContentRegionAvail().x - valueWidth -
                                deltaWidth - textSpacing;
    ImGui::SameLine(std::max(ImGui::GetCursorPosX(), rightAlignedX));
    ImGui::TextColored(ratingColor(value), "%s", valueText.c_str());
    if (comparison)
    {
      ImGui::SameLine();
      ImGui::TextColored(delta >= 0.0f ? ImVec4(0.25f, 0.78f, 0.48f, 1.0f)
                                       : ImVec4(0.92f, 0.38f, 0.36f, 1.0f),
                         "%s", deltaText.c_str());
    }
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ratingColor(value));
    ImGui::ProgressBar(normalized, ImVec2(-1.0f, 14.0f), "");
    ImGui::PopStyleColor();
  }
  ImGui::EndChild();
}
}  // namespace PlayerUI
