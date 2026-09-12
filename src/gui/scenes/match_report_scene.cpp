// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "gui/scenes/match_report_scene.h"

#include <fmt/printf.h>
#include <imgui.h>

#include <algorithm>
#include <format>

#include "controller/game_controller.h"
#include "database/gamedata.h"
#include "global/language_manager.h"
#include "gui/gui_view.h"
#include "gui/view_models/competition_view.h"
#include "gui/widgets/format.h"
#include "gui/widgets/theme.h"
#include "gui/widgets/widgets.h"
#include "model/competition.h"

namespace
{
constexpr float TWO_COLUMN_MIN_WIDTH = 860.0f;
constexpr float SCORE_HEIGHT = 118.0f;
constexpr float MARKER_SIZE = 10.0f;

std::string playerName(const GameData* data, PlayerID id)
{
  if (data == nullptr || id == 0) return {};
  const auto player = data->getPlayer(id);
  return player ? player->get().getName() : std::string();
}

// Small drawn marker for an event (no icon font is bundled).
void eventMarker(MatchEventKind kind)
{
  const Theme::Palette& palette = Theme::palette();
  const float size = MARKER_SIZE * Theme::scale();
  const ImVec2 start = ImGui::GetCursorScreenPos();
  const float lineHeight = ImGui::GetTextLineHeight();
  const ImVec2 center(start.x + size * 0.5f, start.y + lineHeight * 0.5f);
  ImDrawList* drawList = ImGui::GetWindowDrawList();
  switch (kind)
  {
    case MatchEventKind::GOAL:
      drawList->AddCircleFilled(center, size * 0.5f,
                                Theme::toU32(palette.text));
      break;
    case MatchEventKind::OWN_GOAL:
      drawList->AddCircleFilled(center, size * 0.5f,
                                Theme::toU32(palette.negative));
      break;
    case MatchEventKind::YELLOW_CARD:
      drawList->AddRectFilled(
          ImVec2(center.x - size * 0.32f, center.y - size * 0.5f),
          ImVec2(center.x + size * 0.32f, center.y + size * 0.5f),
          IM_COL32(245, 200, 40, 255), 1.5f);
      break;
    case MatchEventKind::SECOND_YELLOW:
      drawList->AddRectFilled(
          ImVec2(center.x - size * 0.5f, center.y - size * 0.5f),
          ImVec2(center.x + size * 0.14f, center.y + size * 0.3f),
          IM_COL32(245, 200, 40, 255), 1.5f);
      drawList->AddRectFilled(
          ImVec2(center.x - size * 0.14f, center.y - size * 0.3f),
          ImVec2(center.x + size * 0.5f, center.y + size * 0.5f),
          IM_COL32(220, 50, 50, 255), 1.5f);
      break;
    case MatchEventKind::RED_CARD:
      drawList->AddRectFilled(
          ImVec2(center.x - size * 0.32f, center.y - size * 0.5f),
          ImVec2(center.x + size * 0.32f, center.y + size * 0.5f),
          IM_COL32(220, 50, 50, 255), 1.5f);
      break;
  }
  ImGui::Dummy(ImVec2(size, lineHeight));
}

void comparisonRow(const char* label, float home, float away,
                   const std::string& homeText, const std::string& awayText)
{
  const Theme::Palette& palette = Theme::palette();
  const float width = ImGui::GetContentRegionAvail().x;
  const float startX = ImGui::GetCursorPosX();
  ImGui::TextUnformatted(homeText.c_str());
  const float labelWidth = ImGui::CalcTextSize(label).x;
  ImGui::SameLine(startX + (width - labelWidth) * 0.5f);
  ImGui::TextColored(palette.muted, "%s", label);
  const float awayWidth = ImGui::CalcTextSize(awayText.c_str()).x;
  ImGui::SameLine(startX + width - awayWidth);
  ImGui::TextUnformatted(awayText.c_str());

  const ImVec2 barStart = ImGui::GetCursorScreenPos();
  const float barHeight = 4.0f * Theme::scale();
  const float total = home + away;
  const float split = total > 0.0f ? home / total : 0.5f;
  ImDrawList* drawList = ImGui::GetWindowDrawList();
  drawList->AddRectFilled(
      barStart,
      ImVec2(barStart.x + width * split - 1.0f, barStart.y + barHeight),
      Theme::toU32(palette.accent), 2.0f);
  drawList->AddRectFilled(ImVec2(barStart.x + width * split + 1.0f, barStart.y),
                          ImVec2(barStart.x + width, barStart.y + barHeight),
                          Theme::toU32(palette.info), 2.0f);
  ImGui::Dummy(ImVec2(width, barHeight + 2.0f * Theme::scale()));
}
}  // namespace

MatchReportScene::MatchReportScene(GUIView* parent, GameDateValue matchDate,
                                   TeamID homeId, TeamID awayId)
    : ManagementScene(parent), date(matchDate), home_id(homeId), away_id(awayId)
{
}

void MatchReportScene::update(float /*deltaTime*/) {}

void MatchReportScene::refresh()
{
  GameController& controller = guiView->getController();
  const auto home = controller.getTeamById(home_id);
  const auto away = controller.getTeamById(away_id);
  home_name = home ? home->get().getName() : std::string();
  away_name = away ? away->get().getName() : std::string();
  report = controller.getMatchReport(date, home_id, away_id);
  events.clear();
  home_players.clear();
  away_players.clear();
  result_note.clear();
  if (!report) return;

  if (report->penalties)
    result_note = fmt::sprintf(LOC("RESULT_PENALTIES"), report->home_penalties,
                               report->away_penalties);
  else if (report->extra_time)
    result_note = LOC("RESULT_AFTER_EXTRA_TIME");

  const GameData* data = controller.getGameData().get();
  for (const MatchReportEvent& event : report->events)
  {
    EventRow row;
    row.kind = event.kind;
    // An own goal is listed under the side it counts for.
    row.home =
        event.kind == MatchEventKind::OWN_GOAL ? !event.home : event.home;
    row.minute = event.added_minute > 0
                     ? std::format("{}+{}'", event.minute, event.added_minute)
                     : std::format("{}'", event.minute);
    row.text = playerName(data, event.player);
    if (row.text.empty()) row.text = LOC("REPORT_UNKNOWN_PLAYER");
    if (event.kind == MatchEventKind::OWN_GOAL)
      row.text += LOC("REPORT_OWN_GOAL_SUFFIX");
    if (event.assist != 0)
      row.text += fmt::sprintf(LOC("REPORT_ASSIST_SUFFIX"),
                               playerName(data, event.assist));
    events.push_back(std::move(row));
  }

  for (const PlayerMatchLine& line : report->players)
  {
    PlayerRow row{line.player_id, playerName(data, line.player_id), line};
    (line.team_id == home_id ? home_players : away_players)
        .push_back(std::move(row));
  }
  const auto byRole = [](const PlayerRow& left, const PlayerRow& right)
  {
    if (left.line.started != right.line.started) return left.line.started;
    return left.line.rating > right.line.rating;
  };
  std::ranges::sort(home_players, byRole);
  std::ranges::sort(away_players, byRole);
}

void MatchReportScene::renderContent()
{
  UI::pageHeader(LOC("REPORT_TITLE"), Format::date(date).c_str());
  if (!report)
  {
    UI::emptyState(LOC("REPORT_MISSING_TITLE"), LOC("REPORT_MISSING_BODY"));
    return;
  }
  renderScore();
  const float available = ImGui::GetContentRegionAvail().x;
  const float gap = ImGui::GetStyle().ItemSpacing.x;
  const bool twoColumns = available >= TWO_COLUMN_MIN_WIDTH * Theme::scale();
  const float half =
      twoColumns ? std::floor((available - gap) * 0.5f) : available;
  const float height =
      std::max(ImGui::GetContentRegionAvail().y, 480.0f * Theme::scale());
  const float topHeight = std::floor(height * 0.46f);
  const float bottomHeight =
      height - topHeight - ImGui::GetStyle().ItemSpacing.y;
  renderEvents(half, topHeight);
  if (twoColumns) ImGui::SameLine();
  renderStats(twoColumns ? available - gap - half : available, topHeight);
  renderRatings("home_ratings", home_name, home_players, half, bottomHeight);
  if (twoColumns) ImGui::SameLine();
  renderRatings("away_ratings", away_name, away_players,
                twoColumns ? available - gap - half : available, bottomHeight);
}

void MatchReportScene::renderScore()
{
  const Theme::Palette& palette = Theme::palette();
  UI::beginCard("report_score", nullptr,
                ImVec2(0.0f, SCORE_HEIGHT * Theme::scale()));
  UI::badge(LOC(CompetitionView::matchTypeKey(report->match_type)),
            palette.info);
  if (report->match_type == MatchType::CUP && report->stage > 0)
  {
    ImGui::SameLine();
    if (const auto cup =
            guiView->getController().getCupStatus(report->competition_id))
    {
      const char* stageKey =
          Competitions::cupRoundLabelKey(report->stage, cup->total_rounds);
      ImGui::TextColored(palette.muted, "%s",
                         fmt::sprintf(LOC(stageKey), report->stage).c_str());
    }
  }
  if (report->attendance > 0)
  {
    ImGui::SameLine();
    ImGui::TextColored(palette.muted, "%s",
                       fmt::sprintf(LOC("REPORT_ATTENDANCE"),
                                    Format::thousands(report->attendance))
                           .c_str());
  }

  const float width = ImGui::GetContentRegionAvail().x;
  const float startX = ImGui::GetCursorPosX();
  {
    const std::string score =
        std::format("{}  –  {}", report->home_goals, report->away_goals);
    Theme::ScopedText heading(Theme::Text::HEADING);
    const float scoreWidth = ImGui::CalcTextSize(score.c_str()).x;
    const float side =
        (width - scoreWidth) * 0.5f - Theme::Space::L * Theme::scale();
    const float homeWidth = ImGui::CalcTextSize(home_name.c_str()).x;
    ImGui::SetCursorPosX(startX + std::max(0.0f, side - homeWidth));
    if (UI::link(home_name.c_str(), "report_home"))
      Navigation::openClub(guiView, home_id);
    ImGui::SameLine(startX + (width - scoreWidth) * 0.5f);
    ImGui::TextUnformatted(score.c_str());
    ImGui::SameLine(startX + (width + scoreWidth) * 0.5f +
                    Theme::Space::L * Theme::scale());
    if (UI::link(away_name.c_str(), "report_away"))
      Navigation::openClub(guiView, away_id);
  }
  if (!result_note.empty())
  {
    Theme::ScopedText small(Theme::Text::SMALL);
    const float noteWidth = ImGui::CalcTextSize(result_note.c_str()).x;
    ImGui::SetCursorPosX(startX + (width - noteWidth) * 0.5f);
    ImGui::TextColored(palette.muted, "%s", result_note.c_str());
  }
  UI::endCard();
}

void MatchReportScene::renderEvents(float width, float height)
{
  const Theme::Palette& palette = Theme::palette();
  UI::beginCard("report_events", LOC("REPORT_EVENTS"), ImVec2(width, height),
                true);
  if (events.empty())
  {
    ImGui::TextColored(palette.faint, "%s", LOC("REPORT_NO_EVENTS"));
    UI::endCard();
    return;
  }
  if (ImGui::BeginTable("events", 3, ImGuiTableFlags_SizingStretchProp))
  {
    ImGui::TableSetupColumn("home", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableSetupColumn("minute", ImGuiTableColumnFlags_WidthFixed,
                            80.0f * Theme::scale());
    ImGui::TableSetupColumn("away", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    for (const EventRow& event : events)
    {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      if (event.home) UI::textRight(event.text.c_str());
      ImGui::TableNextColumn();
      const float cellWidth = ImGui::GetContentRegionAvail().x;
      const float minuteWidth = ImGui::CalcTextSize(event.minute.c_str()).x;
      const float blockWidth = minuteWidth + MARKER_SIZE * Theme::scale() +
                               ImGui::GetStyle().ItemSpacing.x;
      ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                           std::max(0.0f, (cellWidth - blockWidth) * 0.5f));
      ImGui::TextColored(palette.muted, "%s", event.minute.c_str());
      ImGui::SameLine();
      eventMarker(event.kind);
      ImGui::TableNextColumn();
      if (!event.home) ImGui::TextUnformatted(event.text.c_str());
    }
    ImGui::EndTable();
  }
  UI::endCard();
}

void MatchReportScene::renderStats(float width, float height)
{
  UI::beginCard("report_stats", LOC("REPORT_STATS"), ImVec2(width, height),
                true);
  const TeamMatchStats& home = report->home_stats;
  const TeamMatchStats& away = report->away_stats;
  // Simulated (not watched) matches only record the score.
  if (home.shots + away.shots + home.passes_attempted + away.passes_attempted ==
      0)
  {
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextColored(Theme::palette().faint, "%s",
                       LOC("REPORT_NO_TEAM_STATS"));
    ImGui::PopTextWrapPos();
    UI::endCard();
    return;
  }
  const auto count = [](uint16_t value) { return std::to_string(value); };
  comparisonRow(LOC("REPORT_POSSESSION"), home.possession, away.possession,
                std::format("{:.0f}%", static_cast<double>(home.possession)),
                std::format("{:.0f}%", static_cast<double>(away.possession)));
  comparisonRow(LOC("REPORT_SHOTS"), home.shots, away.shots, count(home.shots),
                count(away.shots));
  comparisonRow(LOC("REPORT_ON_TARGET"), home.shots_on_target,
                away.shots_on_target, count(home.shots_on_target),
                count(away.shots_on_target));
  comparisonRow(
      LOC("REPORT_XG"), home.expected_goals, away.expected_goals,
      std::format("{:.2f}", static_cast<double>(home.expected_goals)),
      std::format("{:.2f}", static_cast<double>(away.expected_goals)));
  comparisonRow(
      LOC("REPORT_PASSES"), home.passes_completed, away.passes_completed,
      std::format("{}/{}", home.passes_completed, home.passes_attempted),
      std::format("{}/{}", away.passes_completed, away.passes_attempted));
  comparisonRow(LOC("REPORT_CORNERS"), home.corners, away.corners,
                count(home.corners), count(away.corners));
  comparisonRow(LOC("REPORT_FOULS"), home.fouls, away.fouls, count(home.fouls),
                count(away.fouls));
  comparisonRow(LOC("REPORT_OFFSIDES"), home.offsides, away.offsides,
                count(home.offsides), count(away.offsides));
  comparisonRow(LOC("REPORT_SAVES"), home.saves, away.saves, count(home.saves),
                count(away.saves));
  comparisonRow(LOC("REPORT_CARDS"),
                static_cast<float>(home.yellow_cards + home.red_cards),
                static_cast<float>(away.yellow_cards + away.red_cards),
                std::format("{} / {}", home.yellow_cards, home.red_cards),
                std::format("{} / {}", away.yellow_cards, away.red_cards));
  UI::endCard();
}

void MatchReportScene::renderRatings(const char* id, const std::string& club,
                                     const std::vector<PlayerRow>& rows,
                                     float width, float height)
{
  const Theme::Palette& palette = Theme::palette();
  UI::beginCard(id, club.c_str(), ImVec2(width, height), true);
  if (rows.empty())
  {
    ImGui::TextColored(palette.faint, "%s", LOC("REPORT_NO_PLAYERS"));
    UI::endCard();
    return;
  }
  if (ImGui::BeginTable(id, 5,
                        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                            ImGuiTableFlags_ScrollY |
                            ImGuiTableFlags_SizingFixedFit))
  {
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn(LOC("ROSTER_COL_NAME"),
                            ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn(LOC("STATS_COL_MINUTES"));
    ImGui::TableSetupColumn(LOC("STATS_COL_GOALS"));
    ImGui::TableSetupColumn(LOC("STATS_COL_ASSISTS"));
    ImGui::TableSetupColumn(LOC("STATS_COL_RATING"));
    ImGui::TableHeadersRow();
    for (const PlayerRow& row : rows)
    {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::PushID(static_cast<int>(row.id));
      if (UI::link(row.name.empty() ? LOC("REPORT_UNKNOWN_PLAYER")
                                    : row.name.c_str(),
                   "player"))
        Navigation::openPlayer(guiView, row.id);
      ImGui::PopID();
      if (!row.line.started)
      {
        ImGui::SameLine();
        ImGui::TextColored(palette.faint, "%s", LOC("REPORT_SUBSTITUTE"));
      }
      ImGui::TableNextColumn();
      ImGui::Text("%u'", row.line.minutes);
      ImGui::TableNextColumn();
      ImGui::Text("%u", row.line.goals);
      ImGui::TableNextColumn();
      ImGui::Text("%u", row.line.assists);
      ImGui::TableNextColumn();
      if (row.line.rating > 0.0f)
        ImGui::TextColored(
            Theme::ratingColor(static_cast<double>(row.line.rating) * 10.0),
            "%.1f", static_cast<double>(row.line.rating));
      else
        ImGui::TextColored(palette.faint, "–");
    }
    ImGui::EndTable();
  }
  UI::endCard();
}
