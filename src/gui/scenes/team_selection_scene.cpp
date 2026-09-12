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
#include <array>
#include <cmath>
#include <format>
#include <string>

#include "database/gamedata.h"
#include "global/global.h"
#include "global/language_manager.h"
#include "gui/gui_view.h"
#include "gui/render/match_kit_colors.h"
#include "gui/widgets/format.h"
#include "gui/widgets/theme.h"
#include "gui/widgets/widgets.h"
#include "model/board.h"
#include "model/competition.h"

namespace
{
constexpr float LEAGUE_LIST_WIDTH = 260.0f;
constexpr float CARD_WIDTH = 320.0f;
constexpr float CARD_MIN_CONTENT = 1080.0f;
constexpr float CONTENT_MAX_WIDTH = 1480.0f;
constexpr size_t KEY_PLAYERS = 3;
constexpr int STARS = 5;

enum class ClubColumn : ImGuiID
{
  NAME = 1,
  REPUTATION,
  STADIUM,
  AVERAGE,
  EXPECTATION,
  DIFFICULTY,
  BALANCE,
};

constexpr std::array<const char*, 4> DIFFICULTY_KEYS = {
    "TEAM_SELECTION_DIFFICULTY_EASY", "TEAM_SELECTION_DIFFICULTY_NORMAL",
    "TEAM_SELECTION_DIFFICULTY_HARD", "TEAM_SELECTION_DIFFICULTY_VERY_HARD"};

ImVec4 difficultyColor(int difficulty)
{
  const Theme::Palette& palette = Theme::palette();
  switch (difficulty)
  {
    case 0:
      return palette.positive;
    case 1:
      return palette.info;
    case 2:
      return palette.warning;
    default:
      return palette.negative;
  }
}

// Reputation 1-100 as five drawn stars (half stars included).
void reputationStars(uint8_t reputation)
{
  const Theme::Palette& palette = Theme::palette();
  const float size = ImGui::GetTextLineHeight() * 0.8f;
  const float gap = 2.0f * Theme::scale();
  const ImVec2 start = ImGui::GetCursorScreenPos();
  const float top = start.y + (ImGui::GetTextLineHeight() - size) * 0.5f;
  const float value =
      static_cast<float>(reputation) / 100.0f * static_cast<float>(STARS);
  ImDrawList* drawList = ImGui::GetWindowDrawList();
  for (int star = 0; star < STARS; ++star)
  {
    const ImVec2 center(
        start.x + size * 0.5f + static_cast<float>(star) * (size + gap),
        top + size * 0.5f);
    std::array<ImVec2, 10> points{};
    for (size_t index = 0; index < points.size(); ++index)
    {
      const float angle = -1.5708f + static_cast<float>(index) * 0.6283f;
      const float radius = (index % 2 == 0 ? 0.5f : 0.22f) * size;
      points[index] = ImVec2(center.x + std::cos(angle) * radius,
                             center.y + std::sin(angle) * radius);
    }
    const float fill = std::clamp(value - static_cast<float>(star), 0.0f, 1.0f);
    drawList->AddConvexPolyFilled(points.data(),
                                  static_cast<int>(points.size()),
                                  Theme::toU32(palette.raised));
    if (fill > 0.0f)
    {
      drawList->PushClipRect(
          ImVec2(center.x - size * 0.5f, top),
          ImVec2(center.x - size * 0.5f + size * fill, top + size), true);
      drawList->AddConvexPolyFilled(points.data(),
                                    static_cast<int>(points.size()),
                                    Theme::toU32(palette.warning));
      drawList->PopClipRect();
    }
  }
  ImGui::Dummy(ImVec2(static_cast<float>(STARS) * (size + gap), size));
}

void clubColours(uint32_t shirt, uint32_t trim)
{
  const float size = ImGui::GetTextLineHeight() * 0.8f;
  const ImVec2 start = ImGui::GetCursorScreenPos();
  const float top = start.y + (ImGui::GetTextLineHeight() - size) * 0.5f;
  ImDrawList* drawList = ImGui::GetWindowDrawList();
  drawList->AddRectFilled(ImVec2(start.x, top),
                          ImVec2(start.x + size * 0.5f, top + size), shirt,
                          2.0f * Theme::scale(), ImDrawFlags_RoundCornersLeft);
  drawList->AddRectFilled(ImVec2(start.x + size * 0.5f, top),
                          ImVec2(start.x + size, top + size), trim,
                          2.0f * Theme::scale(), ImDrawFlags_RoundCornersRight);
  ImGui::Dummy(ImVec2(size, size));
}
}  // namespace

TeamSelectionScene::TeamSelectionScene(GUIView* parent) : GUIScene(parent) {}

void TeamSelectionScene::onEnter()
{
  loadAvailableLeagues();
  if (!league_entries.empty())
  {
    selected_league_id = league_entries.front().id;
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
  const float gap = ImGui::GetStyle().ItemSpacing.x;
  const float listWidth = LEAGUE_LIST_WIDTH * Theme::scale();
  const bool showCard = width >= CARD_MIN_CONTENT * Theme::scale();
  const float cardWidth = showCard ? CARD_WIDTH * Theme::scale() : 0.0f;
  renderLeagueList(listWidth, height);
  ImGui::SameLine();
  const float tableWidth =
      width - listWidth - gap - (showCard ? cardWidth + gap : 0.0f);
  ImGui::BeginChild("##club_area", ImVec2(tableWidth, height));
  if (selected_league_id)
    renderClubTable(ImGui::GetContentRegionAvail().y -
                    (showCard ? 0.0f : 150.0f * Theme::scale()));
  else
    UI::emptyState(LOC("TEAM_SELECTION_SELECT_LEAGUE_FIRST"), nullptr);
  if (!showCard) renderSelectedClub(0.0f, ImGui::GetContentRegionAvail().y);
  ImGui::EndChild();
  if (showCard)
  {
    ImGui::SameLine();
    renderSelectedClub(cardWidth, height);
  }
  ImGui::EndGroup();
  ImGui::End();
}

void TeamSelectionScene::renderLeagueList(float width, float height)
{
  const Theme::Palette& palette = Theme::palette();
  UI::beginCard("##leagues", LOC("TEAM_SELECTION_LEAGUES_CAPTION"),
                ImVec2(width, height), true);
  for (size_t index = 0; index < league_entries.size(); ++index)
  {
    const LeagueEntry& entry = league_entries[index];
    // Countries are separated by a gap; lower divisions are indented.
    if (entry.tier == 1 && index > 0)
      ImGui::Dummy(ImVec2(0.0f, Theme::Space::XS * Theme::scale()));
    const bool selected = selected_league_id && *selected_league_id == entry.id;
    ImGui::PushID(static_cast<int>(entry.id));
    const float indent =
        static_cast<float>(entry.tier - 1) * Theme::Space::L * Theme::scale();
    if (indent > 0.0f) ImGui::Indent(indent);
    if (ImGui::Selectable("##league", selected, 0,
                          ImVec2(0.0f, ImGui::GetFrameHeight())))
    {
      selected_league_id = entry.id;
      selected_team_id.reset();
      loadAvailableTeams();
    }
    ImGui::SameLine(indent + Theme::Space::S * Theme::scale());
    ImGui::AlignTextToFramePadding();
    UI::textFitted(entry.name, ImGui::GetContentRegionAvail().x,
                   entry.tier == 1 ? palette.text : palette.muted);
    if (indent > 0.0f) ImGui::Unindent(indent);
    ImGui::PopID();
  }
  UI::endCard();
}

void TeamSelectionScene::renderClubTable(float height)
{
  const Theme::Palette& palette = Theme::palette();
  UI::beginCard("##clubs", LOC("TEAM_SELECTION_CLUBS_CAPTION"),
                ImVec2(0.0f, std::max(height, 200.0f * Theme::scale())));
  const ImGuiTableFlags flags =
      ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
      ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable |
      ImGuiTableFlags_SizingFixedFit;
  // Narrow tables drop stadium and balance (both are on the club card).
  const bool compact =
      ImGui::GetContentRegionAvail().x < 900.0f * Theme::scale();
  if (UI::beginDataTable("TeamsTable", compact ? 5 : 7, flags,
                         compact ? 600.0f : 900.0f, ImVec2(0.0f, 0.0f), 1))
  {
    ImGui::TableSetupColumn(LOC("MAIN_GAME_TEAM"),
                            ImGuiTableColumnFlags_WidthStretch, 0.0f,
                            static_cast<ImGuiID>(ClubColumn::NAME));
    // Explicit widths: the table is laid out correctly from its first frame.
    const auto column = [](const char* label, ImGuiTableColumnFlags extra,
                           float minimum, ClubColumn id)
    {
      ImGui::TableSetupColumn(
          label, ImGuiTableColumnFlags_WidthFixed | extra,
          std::max(ImGui::CalcTextSize(label).x + 20.0f * Theme::scale(),
                   minimum * Theme::scale()),
          static_cast<ImGuiID>(id));
    };
    column(LOC("TEAM_SELECTION_REPUTATION"),
           ImGuiTableColumnFlags_DefaultSort |
               ImGuiTableColumnFlags_PreferSortDescending,
           ImGui::GetTextLineHeight() * 5.0f / Theme::scale() + 14.0f,
           ClubColumn::REPUTATION);
    if (!compact)
      column(LOC("TEAM_SELECTION_STADIUM"),
             ImGuiTableColumnFlags_PreferSortDescending, 70.0f,
             ClubColumn::STADIUM);
    column(LOC("TEAM_SELECTION_AVERAGE_SHORT"),
           ImGuiTableColumnFlags_PreferSortDescending, 50.0f,
           ClubColumn::AVERAGE);
    float objectiveWidth = 0.0f;
    for (const ClubSummary& club : club_summaries)
      objectiveWidth = std::max(objectiveWidth,
                                ImGui::CalcTextSize(LOC(club.objective_key)).x);
    column(LOC("TEAM_SELECTION_EXPECTATION"), 0,
           objectiveWidth / Theme::scale(), ClubColumn::EXPECTATION);
    column(LOC("TEAM_SELECTION_DIFFICULTY"), 0, 90.0f, ClubColumn::DIFFICULTY);
    if (!compact)
      column(LOC("TEAM_SELECTION_BALANCE"),
             ImGuiTableColumnFlags_PreferSortDescending, 70.0f,
             ClubColumn::BALANCE);
    ImGui::TableHeadersRow();
    if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs();
        specs != nullptr && specs->SpecsCount > 0)
    {
      const ImGuiTableColumnSortSpecs& spec = specs->Specs[0];
      const bool ascending = spec.SortDirection == ImGuiSortDirection_Ascending;
      if (specs->SpecsDirty || spec.ColumnUserID != sort_column ||
          ascending != sort_ascending)
      {
        sort_column = spec.ColumnUserID;
        sort_ascending = ascending;
        sortClubs();
      }
      specs->SpecsDirty = false;
    }

    for (const ClubSummary& club : club_summaries)
    {
      ImGui::TableNextRow(ImGuiTableRowFlags_None,
                          ImGui::GetTextLineHeight() + 8.0f * Theme::scale());
      ImGui::TableNextColumn();
      ImGui::PushID(static_cast<int>(club.id));
      // Colour swatch drawn in front of a real, named selectable so the row
      // is findable by label (automation, screen readers of the item tree).
      const float rowStart = ImGui::GetCursorPosX();
      clubColours(club.shirt, club.trim);
      ImGui::SameLine();
      ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), rowStart));
      if (ImGui::Selectable(club.name.c_str(), selected_team_id == club.id,
                            ImGuiSelectableFlags_SpanAllColumns |
                                ImGuiSelectableFlags_AllowDoubleClick))
      {
        selected_team_id = club.id;
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
          startCareer(club.id);
      }
      ImGui::PopID();
      ImGui::TableNextColumn();
      reputationStars(club.reputation);
      if (!compact)
      {
        ImGui::TableNextColumn();
        UI::textRight(club.stadium_text.c_str());
      }
      ImGui::TableNextColumn();
      UI::ratingChip(club.average_overall);
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(LOC(club.objective_key));
      ImGui::TableNextColumn();
      ImGui::TextColored(
          difficultyColor(club.difficulty), "%s",
          LOC(DIFFICULTY_KEYS[static_cast<size_t>(club.difficulty)]));
      if (!compact)
      {
        ImGui::TableNextColumn();
        UI::textRightColored(club.balance < 0 ? palette.negative : palette.text,
                             club.balance_text.c_str());
      }
    }
    ImGui::EndTable();
  }
  UI::endCard();
}

void TeamSelectionScene::renderSelectedClub(float width, float height)
{
  const Theme::Palette& palette = Theme::palette();
  UI::beginCard("##selected_club", nullptr, ImVec2(width, height), true);
  const auto selected =
      std::ranges::find_if(club_summaries, [this](const ClubSummary& club)
                           { return selected_team_id == club.id; });
  if (selected == club_summaries.end())
  {
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextColored(palette.muted, "%s", LOC("TEAM_SELECTION_CHOOSE_CLUB"));
    ImGui::PopTextWrapPos();
    UI::endCard();
    return;
  }
  const ClubSummary& club = *selected;
  clubColours(club.shirt, club.trim);
  ImGui::SameLine();
  {
    Theme::ScopedText title(Theme::Text::HEADING);
    UI::textFitted(club.name, ImGui::GetContentRegionAvail().x, palette.text);
  }
  reputationStars(club.reputation);
  ImGui::Dummy(ImVec2(0.0f, Theme::Space::XS * Theme::scale()));

  const float keyWidth = 150.0f * Theme::scale();
  UI::keyValue(LOC("TEAM_SELECTION_EXPECTATION"), LOC(club.objective_key),
               keyWidth);
  const std::string difficulty =
      LOC(DIFFICULTY_KEYS[static_cast<size_t>(club.difficulty)]);
  ImGui::PushStyleColor(ImGuiCol_Text, difficultyColor(club.difficulty));
  UI::keyValue(LOC("TEAM_SELECTION_DIFFICULTY"), difficulty.c_str(), keyWidth);
  ImGui::PopStyleColor();
  UI::keyValue(LOC("TEAM_SELECTION_STADIUM"), club.stadium_text.c_str(),
               keyWidth);
  UI::keyValue(LOC("TEAM_SELECTION_BALANCE"), club.balance_text.c_str(),
               keyWidth);
  UI::keyValue(LOC("TEAM_SELECTION_WAGES"), club.wages_text.c_str(), keyWidth);
  const std::string squad = std::to_string(club.squad_size);
  UI::keyValue(LOC("ROSTER_SUMMARY_SQUAD"), squad.c_str(), keyWidth);

  ImGui::Dummy(ImVec2(0.0f, Theme::Space::S * Theme::scale()));
  UI::sectionLabel(LOC("TEAM_SELECTION_STRENGTH"));
  const float labelWidth = 120.0f * Theme::scale();
  UI::attributeBar(LOC("ROSTER_SUMMARY_AVG_OVR"), club.average_overall,
                   labelWidth);
  UI::attributeBar(LOC("TEAM_SELECTION_BEST_PLAYER"), club.best_overall,
                   labelWidth);
  ImGui::Dummy(ImVec2(0.0f, Theme::Space::S * Theme::scale()));
  UI::sectionLabel(LOC("DASHBOARD_KEY_PLAYERS"));
  for (const auto& [name, overall] : club.key_players)
  {
    UI::ratingChip(overall);
    ImGui::SameLine();
    UI::textFitted(name, ImGui::GetContentRegionAvail().x, palette.text);
  }

  ImGui::Dummy(ImVec2(0.0f, Theme::Space::M * Theme::scale()));
  if (UI::primaryButton(LOC("TEAM_SELECTION_CONFIRM"), ImVec2(-FLT_MIN, 0.0f)))
    startCareer(club.id);
  UI::endCard();
}

void TeamSelectionScene::startCareer(TeamID teamId)
{
  guiView->getController().selectManagedTeam(teamId);
  guiView->popScene();
}

void TeamSelectionScene::loadAvailableLeagues()
{
  const auto& controller = guiView->getController();
  league_entries.clear();
  const auto data = controller.getGameData();
  if (!data) return;
  // Grouped by country, top division first.
  for (const LeagueID root : Competitions::countryRoots(*data))
  {
    for (const LeagueID id : Competitions::countryLeagues(*data, root))
    {
      const auto league = controller.getLeagueById(id);
      if (!league) continue;
      league_entries.push_back(
          {id, league->get().getName(), controller.getLeagueTier(id)});
    }
  }
}

void TeamSelectionScene::loadAvailableTeams()
{
  if (!selected_league_id) return;
  const auto& controller = guiView->getController();
  const auto& statsConfig = controller.getStatsConfig();
  available_teams.clear();
  for (const auto& teamRef : controller.getTeams())
  {
    const Team& team = teamRef.get();
    if (team.getLeagueId() == selected_league_id.value() &&
        team.getName() != FREE_AGENTS_TEAM_NAME)
      available_teams.push_back(teamRef);
  }

  club_summaries.clear();
  club_summaries.reserve(available_teams.size());
  for (const auto& teamRef : available_teams)
  {
    const Team& team = teamRef.get();
    ClubSummary summary;
    summary.id = team.getId();
    summary.name = team.getName();
    summary.reputation = team.getReputation();
    summary.stadium = team.getStadiumCapacity();
    summary.balance = team.getFinances().getBalance();
    const KitColors kit = chooseMatchKits(team.getId(), 0).home;
    summary.shirt = kit.shirt;
    summary.trim = kit.trim;
    std::vector<std::pair<std::string, float>> players;
    double total = 0.0;
    for (const auto& playerRef : controller.getPlayersForTeam(team.getId()))
    {
      const Player& player = playerRef.get();
      const auto overall = static_cast<float>(player.getOverall(statsConfig));
      total += static_cast<double>(overall);
      summary.best_overall = std::max(summary.best_overall, overall);
      summary.weekly_wages += player.getWage();
      players.emplace_back(player.getName(), overall);
      ++summary.squad_size;
    }
    summary.average_overall =
        summary.squad_size > 0
            ? static_cast<float>(total /
                                 static_cast<double>(summary.squad_size))
            : 0.0f;
    const size_t keyCount = std::min(KEY_PLAYERS, players.size());
    std::ranges::partial_sort(
        players, players.begin() + static_cast<std::ptrdiff_t>(keyCount),
        [](const auto& left, const auto& right)
        { return left.second > right.second; });
    players.resize(keyCount);
    summary.key_players = std::move(players);
    summary.balance_text = Format::money(summary.balance);
    summary.wages_text = Format::money(summary.weekly_wages);
    summary.stadium_text = Format::thousands(summary.stadium);
    club_summaries.push_back(std::move(summary));
  }

  // Board expectation follows the wage-bill rank (as the board model does);
  // difficulty compares squad strength with the rest of the league.
  const int size = static_cast<int>(club_summaries.size());
  std::vector<size_t> order(club_summaries.size());
  for (size_t index = 0; index < order.size(); ++index) order[index] = index;
  std::ranges::sort(order,
                    [this](size_t left, size_t right)
                    {
                      return club_summaries[left].weekly_wages >
                             club_summaries[right].weekly_wages;
                    });
  for (size_t rank = 0; rank < order.size(); ++rank)
  {
    ClubSummary& club = club_summaries[order[rank]];
    club.expected_position = static_cast<int>(rank) + 1;
    club.objective_key = BoardModel::objectiveKey(
        BoardModel::objectiveFor(club.expected_position, size));
  }
  std::ranges::sort(order,
                    [this](size_t left, size_t right)
                    {
                      return club_summaries[left].average_overall >
                             club_summaries[right].average_overall;
                    });
  for (size_t rank = 0; rank < order.size(); ++rank)
  {
    const float percentile =
        size > 1 ? static_cast<float>(rank) / static_cast<float>(size - 1)
                 : 0.0f;
    club_summaries[order[rank]].difficulty = percentile < 0.25f   ? 0
                                             : percentile < 0.6f  ? 1
                                             : percentile < 0.85f ? 2
                                                                  : 3;
  }

  if (sort_column == 0)
  {
    sort_column = static_cast<ImGuiID>(ClubColumn::REPUTATION);
    sort_ascending = false;
  }
  sortClubs();
  if (!club_summaries.empty() && !selected_team_id)
    selected_team_id = club_summaries.front().id;
}

void TeamSelectionScene::sortClubs()
{
  const auto column = static_cast<ClubColumn>(sort_column);
  std::ranges::stable_sort(
      club_summaries,
      [this, column](const ClubSummary& a, const ClubSummary& b)
      {
        int comparison = 0;
        switch (column)
        {
          case ClubColumn::NAME:
            comparison = a.name.compare(b.name);
            break;
          case ClubColumn::REPUTATION:
            comparison = UI::compare(a.reputation, b.reputation);
            break;
          case ClubColumn::STADIUM:
            comparison = UI::compare(a.stadium, b.stadium);
            break;
          case ClubColumn::AVERAGE:
            comparison = UI::compare(a.average_overall, b.average_overall);
            break;
          case ClubColumn::EXPECTATION:
            comparison = UI::compare(a.expected_position, b.expected_position);
            break;
          case ClubColumn::DIFFICULTY:
            comparison = UI::compare(a.difficulty, b.difficulty);
            break;
          case ClubColumn::BALANCE:
            comparison = UI::compare(a.balance, b.balance);
            break;
        }
        if (comparison == 0) comparison = a.name.compare(b.name);
        return sort_ascending ? comparison < 0 : comparison > 0;
      });
}

SceneID TeamSelectionScene::getID() const { return SceneID::TEAM_SELECTION; }
