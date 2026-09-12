// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "team_selection_scene.h"

#include <fmt/printf.h>
#include <imgui.h>

#include <algorithm>
#include <format>
#include <string>

#include "database/gamedata.h"
#include "global/global.h"
#include "global/language_manager.h"
#include "gui/gui_view.h"
#include "gui/widgets/format.h"
#include "gui/widgets/theme.h"
#include "gui/widgets/widgets.h"

namespace
{
constexpr float LEAGUE_LIST_WIDTH = 280.0f;
constexpr float CONTENT_MAX_WIDTH = 1280.0f;
constexpr float SUMMARY_HEIGHT = 96.0f;

}  // namespace

TeamSelectionScene::TeamSelectionScene(GUIView* parent) : GUIScene(parent) {}

void TeamSelectionScene::onEnter()
{
  loadAvailableLeagues();
  if (!available_leagues.empty())
  {
    selected_league_id = available_leagues.front().get().getId();
    loadAvailableTeams();
  }
}

void TeamSelectionScene::update(float deltaTime) { (void)deltaTime; }

void TeamSelectionScene::render()
{
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                      ImVec2(Theme::Space::XL * Theme::scale(),
                             Theme::Space::XL * Theme::scale()));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::Begin("##team_selection", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoSavedSettings);
  ImGui::PopStyleVar(3);

  const float width = std::min(ImGui::GetContentRegionAvail().x,
                               CONTENT_MAX_WIDTH * Theme::scale());
  ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(),
                                (ImGui::GetWindowWidth() - width) * 0.5f));
  ImGui::BeginGroup();
  UI::pageHeader(LOC("TEAM_SELECTION_TITLE"), LOC("TEAM_SELECTION_SUBTITLE"));
  const float height = ImGui::GetContentRegionAvail().y;
  const float listWidth = LEAGUE_LIST_WIDTH * Theme::scale();
  renderLeagueList(listWidth, height);
  ImGui::SameLine();
  ImGui::BeginGroup();
  const float tableWidth = width - listWidth - ImGui::GetStyle().ItemSpacing.x;
  ImGui::BeginChild("##club_area", ImVec2(tableWidth, height));
  if (selected_league_id)
  {
    renderClubTable(ImGui::GetContentRegionAvail().y -
                    SUMMARY_HEIGHT * Theme::scale() -
                    ImGui::GetStyle().ItemSpacing.y);
    renderSelectedClub();
  }
  else
  {
    UI::emptyState(LOC("TEAM_SELECTION_SELECT_LEAGUE_FIRST"), nullptr);
  }
  ImGui::EndChild();
  ImGui::EndGroup();
  ImGui::EndGroup();
  ImGui::End();
}

void TeamSelectionScene::renderLeagueList(float width, float height)
{
  UI::beginCard("##leagues", LOC("TEAM_SELECTION_LEAGUES_CAPTION"),
                ImVec2(width, height), true);
  for (const auto& leagueRef : available_leagues)
  {
    const League& league = leagueRef.get();
    const bool selected =
        selected_league_id && *selected_league_id == league.getId();
    ImGui::PushID(static_cast<int>(league.getId()));
    if (ImGui::Selectable(league.getName().c_str(), selected, 0,
                          ImVec2(0.0f, ImGui::GetFrameHeight())))
    {
      selected_league_id = league.getId();
      selected_team_id.reset();
      loadAvailableTeams();
    }
    ImGui::PopID();
  }
  UI::endCard();
}

void TeamSelectionScene::renderClubTable(float height)
{
  const Theme::Palette& palette = Theme::palette();
  UI::beginCard("##clubs", LOC("TEAM_SELECTION_CLUBS_CAPTION"),
                ImVec2(0.0f, height));
  if (ImGui::BeginTable("TeamsTable", 5,
                        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                            ImGuiTableFlags_ScrollY |
                            ImGuiTableFlags_SizingFixedFit))
  {
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn(LOC("MAIN_GAME_TEAM"),
                            ImGuiTableColumnFlags_WidthStretch);
    // Explicit widths: the table is visible from its very first frame.
    const auto fixedColumn = [](const char* label, float minimum)
    {
      ImGui::TableSetupColumn(
          label, ImGuiTableColumnFlags_WidthFixed,
          std::max(ImGui::CalcTextSize(label).x, minimum * Theme::scale()));
    };
    fixedColumn(LOC("ROSTER_SUMMARY_SQUAD"), 60.0f);
    fixedColumn(LOC("ROSTER_SUMMARY_AVG_OVR"), 60.0f);
    fixedColumn(LOC("TEAM_SELECTION_BEST_PLAYER"), 60.0f);
    fixedColumn(LOC("TEAM_SELECTION_BALANCE"), 90.0f);
    ImGui::TableHeadersRow();
    for (const ClubSummary& club : club_summaries)
    {
      ImGui::TableNextRow(ImGuiTableRowFlags_None,
                          ImGui::GetTextLineHeight() + 8.0f * Theme::scale());
      ImGui::TableNextColumn();
      ImGui::PushID(static_cast<int>(club.id));
      if (ImGui::Selectable(club.name.c_str(), selected_team_id == club.id,
                            ImGuiSelectableFlags_SpanAllColumns |
                                ImGuiSelectableFlags_AllowDoubleClick))
      {
        selected_team_id = club.id;
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
          guiView->getController().selectManagedTeam(club.id);
          guiView->popScene();
        }
      }
      ImGui::PopID();
      ImGui::TableNextColumn();
      ImGui::Text("%zu", club.squad_size);
      ImGui::TableNextColumn();
      UI::ratingChip(club.average_overall);
      ImGui::TableNextColumn();
      UI::ratingChip(club.best_overall);
      ImGui::TableNextColumn();
      UI::textRightColored(club.balance < 0 ? palette.negative : palette.text,
                           club.balance_text.c_str());
    }
    ImGui::EndTable();
  }
  UI::endCard();
}

void TeamSelectionScene::renderSelectedClub()
{
  UI::beginCard("##selected_club", nullptr,
                ImVec2(0.0f, SUMMARY_HEIGHT * Theme::scale()));
  const auto selected =
      std::ranges::find_if(club_summaries, [this](const ClubSummary& club)
                           { return selected_team_id == club.id; });
  if (selected == club_summaries.end())
  {
    ImGui::TextColored(Theme::palette().muted, "%s",
                       LOC("TEAM_SELECTION_CHOOSE_CLUB"));
    UI::endCard();
    return;
  }
  const float buttonWidth = 300.0f * Theme::scale();
  ImGui::BeginGroup();
  {
    Theme::ScopedText title(Theme::Text::HEADING);
    ImGui::TextUnformatted(selected->name.c_str());
  }
  const std::string snapshot =
      fmt::sprintf(LOC("TEAM_SELECTION_SNAPSHOT"), selected->squad_size,
                   selected->balance_text.c_str());
  ImGui::TextColored(Theme::palette().muted, "%s", snapshot.c_str());
  ImGui::EndGroup();
  ImGui::SameLine(std::max(ImGui::GetCursorPosX(),
                           ImGui::GetWindowContentRegionMax().x - buttonWidth));
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() +
                       Theme::Space::S * Theme::scale());
  if (UI::primaryButton(LOC("TEAM_SELECTION_CONFIRM"),
                        ImVec2(buttonWidth, 0.0f)))
  {
    guiView->getController().selectManagedTeam(selected->id);
    guiView->popScene();
  }
  UI::endCard();
}

void TeamSelectionScene::loadAvailableLeagues()
{
  available_leagues = guiView->getController().getLeagues();
}

void TeamSelectionScene::loadAvailableTeams()
{
  if (!selected_league_id) return;
  auto all_teams = guiView->getController().getTeams();
  available_teams.clear();
  for (const auto& team_ref : all_teams)
  {
    const Team& team = team_ref.get();
    if (team.getLeagueId() == selected_league_id.value() &&
        team.getName() != FREE_AGENTS_TEAM_NAME)
    {
      available_teams.push_back(team_ref);
    }
  }
  if (!available_teams.empty())
    selected_team_id = available_teams.front().get().getId();

  const auto& controller = guiView->getController();
  const auto& statsConfig = controller.getStatsConfig();
  club_summaries.clear();
  club_summaries.reserve(available_teams.size());
  for (const auto& teamRef : available_teams)
  {
    const Team& team = teamRef.get();
    ClubSummary summary{team.getId(),
                        team.getName(),
                        0,
                        0.0f,
                        0.0f,
                        team.getFinances().getBalance(),
                        {}};
    double total = 0.0;
    for (const auto& player : controller.getPlayersForTeam(team.getId()))
    {
      const auto overall =
          static_cast<float>(player.get().getOverall(statsConfig));
      total += static_cast<double>(overall);
      summary.best_overall = std::max(summary.best_overall, overall);
      ++summary.squad_size;
    }
    summary.average_overall =
        summary.squad_size > 0
            ? static_cast<float>(total /
                                 static_cast<double>(summary.squad_size))
            : 0.0f;
    summary.balance_text = Format::money(summary.balance);
    club_summaries.push_back(std::move(summary));
  }
}

SceneID TeamSelectionScene::getID() const { return SceneID::TEAM_SELECTION; }
