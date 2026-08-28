// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "roster_scene.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <format>
#include <string>

#include "global/language_manager.h"
#include "gui/gui_constants.h"
#include "gui/gui_view.h"
#include "gui/player_ui.h"
#include "model/role_utils.h"

namespace
{
constexpr int TABLE_COLUMNS = 6;
constexpr float AGE_COLUMN_WIDTH = 42.0f;
constexpr float OVERALL_COLUMN_WIDTH = 60.0f;
constexpr float WAGE_COLUMN_WIDTH = 85.0f;
constexpr float CONTRACT_COLUMN_WIDTH = 64.0f;

enum class RosterColumn : ImGuiID
{
  NAME,
  ROLE,
  AGE,
  OVERALL,
  WAGE,
  CONTRACT,
};

constexpr std::array<PlayerRole, 13> FILTER_ROLES = {
    PlayerRole::GK,     PlayerRole::CB, PlayerRole::LB,  PlayerRole::RB,
    PlayerRole::CDM,    PlayerRole::CM, PlayerRole::CAM, PlayerRole::LM,
    PlayerRole::RM,     PlayerRole::LW, PlayerRole::RW,  PlayerRole::ST,
    PlayerRole::UNKNOWN};

bool containsCaseInsensitive(std::string value, std::string query)
{
  const auto lower = [](unsigned char character)
  { return static_cast<char>(std::tolower(character)); };
  std::ranges::transform(value, value.begin(), lower);
  std::ranges::transform(query, query.begin(), lower);
  return value.contains(query);
}
}  // namespace

RosterScene::RosterScene(GUIView* parent) : GUIScene(parent) {}

void RosterScene::onEnter() { loadRoster(); }

void RosterScene::update(float deltaTime) { (void)deltaTime; }

void RosterScene::render()
{
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->Pos);
  ImGui::SetNextWindowSize(ImGui::GetMainViewport()->Size);
  ImGui::Begin(
      LOC("ROSTER_TITLE"), nullptr,
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);

  if (ImGui::Button(LOC("ROSTER_BACK"), ImVec2(GUIConstants::BUTTON_WIDTH,
                                               GUIConstants::BUTTON_HEIGHT)))
  {
    guiView->popScene();
  }
  ImGui::Separator();
  ImGui::Spacing();

  const auto& statsConfig = guiView->getController().getStatsConfig();
  double totalAge = 0.0;
  double totalOverall = 0.0;
  std::uint64_t payroll = 0;
  int expiring = 0;
  for (const auto& playerRef : roster_players)
  {
    const Player& player = playerRef.get();
    totalAge += player.getAge();
    totalOverall += player.getOverall(statsConfig);
    payroll += player.getWage();
    if (player.getContractYears() <= 1) ++expiring;
  }
  const double divisor =
      roster_players.empty() ? 1.0 : static_cast<double>(roster_players.size());
  const float cardWidth =
      std::max(135.0f, (ImGui::GetContentRegionAvail().x -
                        ImGui::GetStyle().ItemSpacing.x * 4.0f) /
                           5.0f);
  PlayerUI::metricCard("squad", LOC("ROSTER_SUMMARY_SQUAD"),
                       std::to_string(roster_players.size()),
                       ImVec4(0.40f, 0.68f, 0.92f, 1.0f), cardWidth);
  ImGui::SameLine();
  PlayerUI::metricCard("age", LOC("ROSTER_SUMMARY_AVG_AGE"),
                       std::format("{:.1f}", totalAge / divisor),
                       ImVec4(0.55f, 0.75f, 0.92f, 1.0f), cardWidth);
  ImGui::SameLine();
  PlayerUI::metricCard("overall", LOC("ROSTER_SUMMARY_AVG_OVR"),
                       std::format("{:.1f}", totalOverall / divisor),
                       PlayerUI::ratingColor(totalOverall / divisor),
                       cardWidth);
  ImGui::SameLine();
  PlayerUI::metricCard("payroll", LOC("ROSTER_SUMMARY_PAYROLL"),
                       std::format("{}", payroll),
                       ImVec4(0.55f, 0.75f, 0.92f, 1.0f), cardWidth);
  ImGui::SameLine();
  PlayerUI::metricCard("contracts", LOC("ROSTER_SUMMARY_EXPIRING"),
                       std::to_string(expiring),
                       expiring > 0 ? ImVec4(0.92f, 0.38f, 0.36f, 1.0f)
                                    : ImVec4(0.25f, 0.78f, 0.48f, 1.0f),
                       cardWidth);

  ImGui::SetNextItemWidth(260.0f);
  ImGui::InputTextWithHint("##roster_search", LOC("ROSTER_SEARCH_HINT"),
                           search_text.data(), search_text.size());
  ImGui::SameLine();
  ImGui::SetNextItemWidth(150.0f);
  const std::size_t selectedRoleIndex =
      role_filter_index > 0 ? static_cast<std::size_t>(role_filter_index - 1)
                            : 0U;
  const std::string rolePreview =
      role_filter_index == 0
          ? LOC("ROSTER_ALL_ROLES")
          : RoleUtils::toString(FILTER_ROLES[selectedRoleIndex]);
  if (ImGui::BeginCombo("##roster_role", rolePreview.c_str()))
  {
    if (ImGui::Selectable(LOC("ROSTER_ALL_ROLES"), role_filter_index == 0))
      role_filter_index = 0;
    for (std::size_t index = 0; index < FILTER_ROLES.size(); ++index)
    {
      const std::string role = RoleUtils::toString(FILTER_ROLES[index]);
      if (ImGui::Selectable(role.c_str(),
                            role_filter_index == static_cast<int>(index + 1)))
        role_filter_index = static_cast<int>(index + 1);
    }
    ImGui::EndCombo();
  }

  if (!ImGui::BeginTable(
          "RosterWorkspace", 2,
          ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
  {
    ImGui::End();
    return;
  }
  ImGui::TableSetupColumn("table", ImGuiTableColumnFlags_WidthStretch, 0.70f);
  ImGui::TableSetupColumn("details", ImGuiTableColumnFlags_WidthStretch, 0.30f);
  ImGui::TableNextColumn();

  if (ImGui::BeginTable("RosterTable", TABLE_COLUMNS,
                        ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                            ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY,
                        ImVec2(0.0f, ImGui::GetContentRegionAvail().y)))
  {
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn(LOC("ROSTER_COL_NAME"),
                            ImGuiTableColumnFlags_DefaultSort, 0.0f,
                            static_cast<ImGuiID>(RosterColumn::NAME));
    ImGui::TableSetupColumn(LOC("ROSTER_COL_ROLE"), 0, 0.0f,
                            static_cast<ImGuiID>(RosterColumn::ROLE));
    ImGui::TableSetupColumn(LOC("ROSTER_COL_AGE"),
                            ImGuiTableColumnFlags_WidthFixed, AGE_COLUMN_WIDTH,
                            static_cast<ImGuiID>(RosterColumn::AGE));
    ImGui::TableSetupColumn(
        LOC("ROSTER_COL_OVERALL"), ImGuiTableColumnFlags_WidthFixed,
        OVERALL_COLUMN_WIDTH, static_cast<ImGuiID>(RosterColumn::OVERALL));
    ImGui::TableSetupColumn(LOC("ROSTER_COL_WAGE"),
                            ImGuiTableColumnFlags_WidthFixed, WAGE_COLUMN_WIDTH,
                            static_cast<ImGuiID>(RosterColumn::WAGE));
    ImGui::TableSetupColumn(
        LOC("ROSTER_COL_CONTRACT"), ImGuiTableColumnFlags_WidthFixed,
        CONTRACT_COLUMN_WIDTH, static_cast<ImGuiID>(RosterColumn::CONTRACT));
    ImGui::TableHeadersRow();

    if (ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs();
        sort_specs != nullptr && sort_specs->SpecsCount > 0 &&
        sort_specs->SpecsDirty)
    {
      const ImGuiTableColumnSortSpecs& spec = sort_specs->Specs[0];
      const bool ascending = spec.SortDirection == ImGuiSortDirection_Ascending;
      std::ranges::sort(
          roster_players,
          [&](const std::reference_wrapper<const Player>& left,
              const std::reference_wrapper<const Player>& right)
          {
            const Player& a = left.get();
            const Player& b = right.get();
            int comparison = 0;
            switch (static_cast<RosterColumn>(spec.ColumnUserID))
            {
              case RosterColumn::NAME:
                comparison = a.getName().compare(b.getName());
                break;
              case RosterColumn::ROLE:
                comparison = RoleUtils::toString(a.getRole())
                                 .compare(RoleUtils::toString(b.getRole()));
                break;
              case RosterColumn::AGE:
                comparison = a.getAge() - b.getAge();
                break;
              case RosterColumn::OVERALL:
              {
                const double overall_a = a.getOverall(statsConfig);
                const double overall_b = b.getOverall(statsConfig);
                comparison = overall_a < overall_b
                                 ? -1
                                 : (overall_a > overall_b ? 1 : 0);
                break;
              }
              case RosterColumn::WAGE:
                comparison = a.getWage() < b.getWage()
                                 ? -1
                                 : (a.getWage() > b.getWage() ? 1 : 0);
                break;
              case RosterColumn::CONTRACT:
                comparison = static_cast<int>(a.getContractYears()) -
                             static_cast<int>(b.getContractYears());
                break;
            }
            if (comparison == 0)
            {
              comparison =
                  a.getId() < b.getId() ? -1 : (a.getId() > b.getId() ? 1 : 0);
            }
            return ascending ? comparison < 0 : comparison > 0;
          });
      sort_specs->SpecsDirty = false;
    }

    for (const auto& player_ref : roster_players)
    {
      const Player& player = player_ref.get();
      if (search_text.front() != '\0' &&
          !containsCaseInsensitive(player.getName(), search_text.data()))
        continue;
      if (role_filter_index > 0 &&
          player.getRole() != FILTER_ROLES[selectedRoleIndex])
        continue;
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      const std::string player_id_text = std::to_string(player.getId());
      ImGui::PushID(player_id_text.c_str());
      const bool selected = selected_player_id == player.getId();
      if (ImGui::Selectable(player.getName().c_str(), selected,
                            ImGuiSelectableFlags_SpanAllColumns))
      {
        selected_player_id =
            selected ? std::nullopt : std::optional<PlayerID>(player.getId());
      }
      if (ImGui::IsItemHovered())
      {
        ImGui::BeginTooltip();
        ImGui::TextUnformatted(player.getName().c_str());
        ImGui::Separator();
        ImGui::Text("%s: %d", LOC("ROSTER_TOOLTIP_AGE"), player.getAge());
        ImGui::Text("%s: %u", LOC("ROSTER_TOOLTIP_WAGE"), player.getWage());
        ImGui::Text("%s: %u", LOC("ROSTER_TOOLTIP_CONTRACT"),
                    static_cast<unsigned>(player.getContractYears()));
        ImGui::EndTooltip();
      }
      ImGui::PopID();
      ImGui::TableNextColumn();
      ImGui::Text("%s", RoleUtils::toString(player.getRole()).c_str());
      ImGui::TableNextColumn();
      ImGui::Text("%d", player.getAge());
      ImGui::TableNextColumn();
      ImGui::Text("%.1f", player.getOverall(statsConfig));
      ImGui::TableNextColumn();
      ImGui::Text("%u", player.getWage());
      ImGui::TableNextColumn();
      ImGui::Text("%u", static_cast<unsigned>(player.getContractYears()));
    }
    ImGui::EndTable();
  }

  ImGui::TableNextColumn();
  ImGui::TextUnformatted(LOC("PLAYER_DETAILS"));
  PlayerUI::detailPanel("RosterPlayerDetails", selectedPlayer(), statsConfig);
  ImGui::EndTable();

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

const Player* RosterScene::selectedPlayer() const
{
  if (!selected_player_id) return nullptr;
  const auto found = std::ranges::find_if(
      roster_players, [this](const std::reference_wrapper<const Player>& player)
      { return player.get().getId() == *selected_player_id; });
  return found == roster_players.end() ? nullptr : &found->get();
}
