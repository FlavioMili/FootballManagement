// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "roster_scene.h"

#include <fmt/printf.h>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <format>
#include <string>
#include <tuple>

#include "global/language_manager.h"
#include "gui/gui_view.h"
#include "gui/player_ui.h"
#include "gui/widgets/format.h"
#include "gui/widgets/theme.h"
#include "gui/widgets/widgets.h"
#include "model/role_utils.h"

namespace
{
constexpr float DETAIL_PANEL_WIDTH = 310.0f;
constexpr float TABLE_MIN_WIDTH = 760.0f;
// The side panel only appears when the full table still fits beside it.
constexpr float DETAIL_PANEL_MIN_CONTENT =
    TABLE_MIN_WIDTH + DETAIL_PANEL_WIDTH + 12.0f;

enum class RosterColumn : ImGuiID
{
  NAME = 1,
  ROLE,
  AGE,
  OVERALL,
  CONDITION,
  FORM,
  VALUE,
  WAGE,
  CONTRACT,
  STATUS,
};

constexpr ImGuiID columnId(RosterColumn column)
{
  return static_cast<ImGuiID>(column);
}

// Unscaled widths; the highest priority numbers hide first on narrow windows.
const std::array<UI::Column, 10>& rosterColumns()
{
  constexpr ImGuiTableColumnFlags DESCENDING =
      ImGuiTableColumnFlags_PreferSortDescending;
  static const std::array<UI::Column, 10> columns = {{
      {"ROSTER_COL_NAME", 0.0f, 0, ImGuiTableColumnFlags_DefaultSort,
       columnId(RosterColumn::NAME)},
      {"ROSTER_COL_ROLE", 56.0f, 1, ImGuiTableColumnFlags_None,
       columnId(RosterColumn::ROLE)},
      {"ROSTER_COL_AGE", 44.0f, 3, ImGuiTableColumnFlags_None,
       columnId(RosterColumn::AGE)},
      {"ROSTER_COL_OVERALL", 60.0f, 0, DESCENDING,
       columnId(RosterColumn::OVERALL)},
      {"ROSTER_COL_CONDITION", 78.0f, 2, DESCENDING,
       columnId(RosterColumn::CONDITION)},
      {"ROSTER_COL_FORM", 54.0f, 4, DESCENDING, columnId(RosterColumn::FORM)},
      {"ROSTER_COL_VALUE", 88.0f, 3, DESCENDING, columnId(RosterColumn::VALUE)},
      {"ROSTER_COL_WAGE", 84.0f, 4, DESCENDING, columnId(RosterColumn::WAGE)},
      {"ROSTER_COL_CONTRACT", 70.0f, 5, ImGuiTableColumnFlags_None,
       columnId(RosterColumn::CONTRACT)},
      {"ROSTER_COL_STATUS", 150.0f, 2, ImGuiTableColumnFlags_None,
       columnId(RosterColumn::STATUS)},
  }};
  return columns;
}

ImVec4 conditionColor(float condition)
{
  const Theme::Palette& palette = Theme::palette();
  if (condition >= 85.0f) return palette.positive;
  if (condition >= 65.0f) return palette.warning;
  return palette.negative;
}

constexpr std::array<PlayerRole, 13> FILTER_ROLES = {
    PlayerRole::GK,     PlayerRole::CB, PlayerRole::LB,  PlayerRole::RB,
    PlayerRole::CDM,    PlayerRole::CM, PlayerRole::CAM, PlayerRole::LM,
    PlayerRole::RM,     PlayerRole::LW, PlayerRole::RW,  PlayerRole::ST,
    PlayerRole::UNKNOWN};

constexpr std::array<const char*, 5> GROUP_FILTER_KEYS = {
    "ROSTER_ALL_POSITIONS", "POSITION_GROUP_GK", "POSITION_GROUP_DEF",
    "POSITION_GROUP_MID", "POSITION_GROUP_ATT"};

ImVec4 groupColor(PlayerView::PositionGroup group)
{
  const Theme::Palette& palette = Theme::palette();
  switch (group)
  {
    case PlayerView::PositionGroup::GOALKEEPER:
      return palette.warning;
    case PlayerView::PositionGroup::DEFENDER:
      return palette.info;
    case PlayerView::PositionGroup::MIDFIELDER:
      return palette.positive;
    case PlayerView::PositionGroup::FORWARD:
      return palette.negative;
  }
  return palette.muted;
}
}  // namespace

RosterScene::RosterScene(GUIView* parent) : ManagementScene(parent) {}

RosterScene::RosterScene(GUIView* parent, TeamID teamId)
    : ManagementScene(parent), team_id(teamId)
{
}

void RosterScene::update(float deltaTime) { (void)deltaTime; }

NavSection RosterScene::navSection() const
{
  return isManagedClub() ? NavSection::SQUAD : NavSection::NONE;
}

bool RosterScene::isManagedClub() const
{
  const auto managed = guiView->getController().getManagedTeam();
  return !team_id || (managed && managed->get().getId() == *team_id);
}

void RosterScene::renderContent()
{
  const std::string subtitle =
      isManagedClub()
          ? std::string(LOC("ROSTER_SUBTITLE"))
          : fmt::sprintf(LOC("ROSTER_OTHER_CLUB_SUBTITLE"), club_name.c_str());
  UI::pageHeader(isManagedClub() ? LOC("ROSTER_TITLE") : club_name.c_str(),
                 subtitle.c_str());
  renderSummary();
  renderFilters();

  const float available = ImGui::GetContentRegionAvail().x;
  const float height = ImGui::GetContentRegionAvail().y;
  show_details = available >= DETAIL_PANEL_MIN_CONTENT * Theme::scale();
  if (!show_details)
  {
    renderTable(height);
    return;
  }
  const float detailWidth = DETAIL_PANEL_WIDTH * Theme::scale();
  ImGui::BeginChild(
      "roster_table_region",
      ImVec2(available - detailWidth - ImGui::GetStyle().ItemSpacing.x,
             height));
  renderTable(height);
  ImGui::EndChild();
  ImGui::SameLine();
  ImGui::BeginChild("roster_detail_region", ImVec2(detailWidth, height));
  renderDetails(height);
  ImGui::EndChild();
}

void RosterScene::renderSummary()
{
  const Theme::Palette& palette = Theme::palette();
  UI::TileRow tiles(5);
  const float width = tiles.width();
  const std::string squad = std::to_string(rows.size());
  const std::string age = std::format("{:.1f}", average_age);
  const std::string overall = std::format("{:.1f}", average_overall);
  const std::string wages = Format::money(payroll);
  const std::string expiring = std::to_string(expiring_contracts);
  tiles.next();
  UI::statTile("squad", LOC("ROSTER_SUMMARY_SQUAD"), squad.c_str(), nullptr,
               palette.text, width);
  tiles.next();
  UI::statTile("age", LOC("ROSTER_SUMMARY_AVG_AGE"), age.c_str(), nullptr,
               palette.text, width);
  tiles.next();
  UI::statTile("overall", LOC("ROSTER_SUMMARY_AVG_OVR"), overall.c_str(),
               nullptr, Theme::ratingColor(average_overall), width);
  tiles.next();
  UI::statTile("payroll", LOC("ROSTER_SUMMARY_PAYROLL"), wages.c_str(), nullptr,
               palette.text, width);
  tiles.next();
  UI::statTile(
      "contracts", LOC("ROSTER_SUMMARY_EXPIRING"), expiring.c_str(), nullptr,
      expiring_contracts > 0 ? palette.negative : palette.positive, width);
}

void RosterScene::renderFilters()
{
  ImGui::SetNextItemWidth(240.0f * Theme::scale());
  ImGui::InputTextWithHint("##roster_search", LOC("ROSTER_SEARCH_HINT"),
                           search_text.data(), search_text.size());
  for (size_t index = 0; index < GROUP_FILTER_KEYS.size(); ++index)
  {
    UI::sameLineIfFits(UI::buttonWidth(LOC(GROUP_FILTER_KEYS[index])));
    const bool active = group_filter_index == static_cast<int>(index);
    ImGui::PushID(static_cast<int>(index));
    if (UI::toggleButton(LOC(GROUP_FILTER_KEYS[index]), active))
      group_filter_index = static_cast<int>(index);
    ImGui::PopID();
  }
  UI::sameLineIfFits(130.0f * Theme::scale());
  ImGui::SetNextItemWidth(130.0f * Theme::scale());
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
  if (filtered_search != search_text.data() ||
      filtered_role != role_filter_index ||
      filtered_group != group_filter_index)
    applyFilter();
}

void RosterScene::applyFilter()
{
  filtered_search = search_text.data();
  filtered_role = role_filter_index;
  filtered_group = group_filter_index;
  const std::string query = PlayerView::toLower(filtered_search);
  visible_rows.clear();
  for (size_t index = 0; index < rows.size(); ++index)
  {
    const PlayerView::PlayerRow& row = rows[index];
    if (!query.empty() && !row.name_lower.contains(query)) continue;
    if (role_filter_index > 0 &&
        row.role_id != FILTER_ROLES[static_cast<size_t>(role_filter_index - 1)])
      continue;
    if (group_filter_index > 0 &&
        static_cast<int>(row.group) != group_filter_index - 1)
      continue;
    visible_rows.push_back(index);
  }
  applySort();
}

void RosterScene::applySort()
{
  const auto column = static_cast<RosterColumn>(sort_column);
  std::ranges::sort(
      visible_rows,
      [this, column](size_t leftIndex, size_t rightIndex)
      {
        const PlayerView::PlayerRow& a = rows[leftIndex];
        const PlayerView::PlayerRow& b = rows[rightIndex];
        int comparison = 0;
        switch (column)
        {
          case RosterColumn::NAME:
            comparison = a.name.compare(b.name);
            break;
          case RosterColumn::ROLE:
            comparison = UI::compare(a.role_id, b.role_id);
            break;
          case RosterColumn::AGE:
            comparison = UI::compare(a.age, b.age);
            break;
          case RosterColumn::OVERALL:
            comparison = UI::compare(a.overall, b.overall);
            break;
          case RosterColumn::CONDITION:
            comparison = UI::compare(a.condition, b.condition);
            break;
          case RosterColumn::FORM:
            comparison = UI::compare(a.form, b.form);
            break;
          case RosterColumn::VALUE:
            comparison = UI::compare(a.market_value, b.market_value);
            break;
          case RosterColumn::WAGE:
            comparison = UI::compare(a.wage, b.wage);
            break;
          case RosterColumn::CONTRACT:
            comparison = UI::compare(a.contract_years, b.contract_years);
            break;
          case RosterColumn::STATUS:
            comparison =
                UI::compare(std::tuple(a.injury_days > 0, a.suspension > 0,
                                       starters.contains(a.id)),
                            std::tuple(b.injury_days > 0, b.suspension > 0,
                                       starters.contains(b.id)));
            break;
        }
        if (comparison == 0) comparison = UI::compare(a.id, b.id);
        return sort_ascending ? comparison < 0 : comparison > 0;
      });
}

void RosterScene::renderTable(float height)
{
  const Theme::Palette& palette = Theme::palette();
  if (visible_rows.empty())
  {
    UI::emptyState(LOC("ROSTER_EMPTY_TITLE"), LOC("ROSTER_EMPTY_BODY"));
    return;
  }
  // Fills the page height (vertical scroll only); columns drop by priority
  // instead of scrolling sideways.
  std::array<UI::Column, 10> columns = rosterColumns();
  for (UI::Column& column : columns) column.label = LOC(column.label);
  const UI::ColumnMask mask =
      UI::fitColumns(columns, ImGui::GetContentRegionAvail().x);
  const ImGuiTableFlags flags =
      ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
      ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY;
  if (!UI::beginResponsiveTable("RosterTable", columns, mask, flags,
                                UI::TableHeader::SORTABLE,
                                ImVec2(0.0f, height)))
    return;

  // The table remembers its sort across screen visits while this scene is
  // new, so follow the header state every frame, not only when it changes.
  if (ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs();
      sortSpecs != nullptr && sortSpecs->SpecsCount > 0)
  {
    const ImGuiTableColumnSortSpecs& spec = sortSpecs->Specs[0];
    const bool ascending = spec.SortDirection == ImGuiSortDirection_Ascending;
    if (sortSpecs->SpecsDirty || spec.ColumnUserID != sort_column ||
        ascending != sort_ascending)
    {
      sort_column = spec.ColumnUserID;
      sort_ascending = ascending;
      applySort();
    }
    sortSpecs->SpecsDirty = false;
  }

  ImGuiListClipper clipper;
  clipper.Begin(static_cast<int>(visible_rows.size()));
  while (clipper.Step())
  {
    for (int line = clipper.DisplayStart; line < clipper.DisplayEnd; ++line)
    {
      const PlayerView::PlayerRow& row =
          rows[visible_rows[static_cast<size_t>(line)]];
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::PushID(static_cast<int>(row.id));
      const bool selected = selected_player_id == row.id;
      if (ImGui::Selectable(row.name.c_str(), selected,
                            ImGuiSelectableFlags_SpanAllColumns |
                                ImGuiSelectableFlags_AllowDoubleClick |
                                ImGuiSelectableFlags_AllowOverlap))
      {
        selected_player_id = row.id;
        // Without the side panel a single click goes straight to the profile.
        if (!show_details || ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
          Navigation::openPlayer(guiView, row.id);
      }
      if (show_details && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
        ImGui::SetTooltip("%s", LOC("ROSTER_OPEN_PROFILE_HINT"));
      ImGui::PopID();
      if (UI::cell(mask, 1))
        ImGui::TextColored(groupColor(row.group), "%s", row.role.c_str());
      if (UI::cell(mask, 2)) ImGui::Text("%d", row.age);
      if (UI::cell(mask, 3)) UI::ratingChip(row.overall);
      if (UI::cell(mask, 4))
        ImGui::TextColored(conditionColor(row.condition), "%.0f%%",
                           static_cast<double>(row.condition));
      if (UI::cell(mask, 5))
      {
        if (row.form > 0.0f)
          ImGui::Text("%.1f", static_cast<double>(row.form));
        else
          ImGui::TextColored(palette.faint, "–");
      }
      if (UI::cell(mask, 6)) UI::textRight(row.value_text.c_str());
      if (UI::cell(mask, 7)) UI::textRight(row.wage_text.c_str());
      if (UI::cell(mask, 8))
        ImGui::TextColored(
            row.contract_years <= 1 ? palette.negative : palette.text, "%d",
            row.contract_years);
      if (!UI::cell(mask, 9)) continue;
      if (row.injury_days > 0)
      {
        UI::badge(LOC("ROSTER_BADGE_INJURED"), palette.negative);
        if (ImGui::IsItemHovered())
          ImGui::SetTooltip(
              "%s", fmt::sprintf(LOC("ROSTER_INJURY_TOOLTIP"), row.injury_days)
                        .c_str());
        ImGui::SameLine();
      }
      if (row.suspension > 0)
      {
        UI::badge(LOC("ROSTER_BADGE_SUSPENDED"), palette.warning);
        if (ImGui::IsItemHovered())
          ImGui::SetTooltip("%s", fmt::sprintf(LOC("PLAYER_SUSPENDED_MATCHES"),
                                               row.suspension)
                                      .c_str());
        ImGui::SameLine();
      }
      if (starters.contains(row.id))
      {
        UI::badge(LOC("ROSTER_BADGE_STARTER"), palette.accent);
        ImGui::SameLine();
      }
      if (row.listed) UI::badge(LOC("ROSTER_BADGE_LISTED"), palette.info);
    }
  }
  ImGui::EndTable();
}

void RosterScene::renderDetails(float height)
{
  const Player* player = selectedPlayer();
  const float buttonsHeight =
      player != nullptr
          ? (UI::buttonHeight() + ImGui::GetStyle().ItemSpacing.y) * 2.0f
          : 0.0f;
  PlayerUI::detailPanel("RosterPlayerDetails", player,
                        guiView->getController().getStatsConfig(), nullptr,
                        height - buttonsHeight);
  if (player == nullptr) return;
  if (UI::primaryButton(LOC("ROSTER_OPEN_PROFILE"), ImVec2(-FLT_MIN, 0.0f)))
    Navigation::openPlayer(guiView, player->getId());
  if (!isManagedClub()) return;
  GameController& controller = guiView->getController();
  const bool listed = controller.isPlayerListed(player->getId());
  if (UI::secondaryButton(
          LOC(listed ? "TRANSFER_UNLIST" : "PROFILE_TRANSFER_LIST"),
          ImVec2(-FLT_MIN, 0.0f)))
  {
    if (listed)
      controller.removePlayerFromTransfer(player->getId());
    else
      controller.listPlayerForTransfer(
          player->getId(), controller.getPlayerMarketValue(player->getId()));
    showToast(LOC(listed ? "PROFILE_UNLISTED_TOAST" : "PROFILE_LISTED_TOAST"));
    loadRoster();
  }
}

void RosterScene::loadRoster()
{
  GameController& controller = guiView->getController();
  const auto managed = controller.getManagedTeam();
  const std::optional<TeamID> clubId =
      team_id ? team_id
              : (managed ? std::optional<TeamID>(managed->get().getId())
                         : std::nullopt);
  roster_players.clear();
  rows.clear();
  starters.clear();
  if (!clubId) return;
  const auto club = controller.getTeamById(*clubId);
  club_name = club ? club->get().getName() : std::string();
  roster_players = controller.getPlayersForTeam(*clubId);
  // U18 players and intake trialists belong to the academy, not the squad.
  std::erase_if(roster_players, [&controller](const auto& player)
                { return controller.isAcademyPlayer(player.get().getId()); });

  if (club)
  {
    const Lineup& lineup = club->get().getLineup();
    if (const Player* goalkeeper = lineup.getGoalkeeper())
      starters.insert(goalkeeper->getId());
    for (const auto& positioned : lineup.getOutfieldPlayers())
      if (positioned.player) starters.insert(positioned.player->getId());
  }

  rows.reserve(roster_players.size());
  double totalAge = 0.0;
  double totalOverall = 0.0;
  payroll = 0;
  expiring_contracts = 0;
  for (const auto& playerRef : roster_players)
  {
    PlayerView::PlayerRow row =
        PlayerView::makeRow(controller, playerRef.get());
    totalAge += row.age;
    totalOverall += static_cast<double>(row.overall);
    payroll += row.wage;
    if (row.contract_years <= 1) ++expiring_contracts;
    rows.push_back(std::move(row));
  }
  const double divisor = rows.empty() ? 1.0 : static_cast<double>(rows.size());
  average_age = totalAge / divisor;
  average_overall = totalOverall / divisor;
  applyFilter();
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
