// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "gui/scenes/club_scene.h"

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
#include "model/board.h"
#include "model/competition.h"
#include "model/season_history.h"

namespace
{
constexpr float TWO_COLUMN_MIN_WIDTH = 860.0f;
constexpr float CARD_HEIGHT = 262.0f;
constexpr float KEY_WIDTH = 170.0f;
constexpr int MAX_TICKET_PRICE = 1000;

std::string teamName(const GameController& controller, TeamID id)
{
  if (id == 0) return "–";
  const auto team = controller.getTeamById(id);
  return team ? team->get().getName() : std::string("–");
}

std::string joinTeams(const GameController& controller,
                      const std::vector<TeamID>& teams)
{
  std::string joined;
  for (const TeamID id : teams)
  {
    if (!joined.empty()) joined += ", ";
    joined += teamName(controller, id);
  }
  return joined;
}
}  // namespace

ClubScene::ClubScene(GUIView* parent) : ManagementScene(parent) {}

void ClubScene::update(float /*deltaTime*/) {}

void ClubScene::refresh()
{
  GameController& controller = guiView->getController();
  const auto managed = controller.getManagedTeam();
  history.clear();
  confidence_trend.clear();
  if (!managed) return;
  const Team& club = managed->get();

  const auto table =
      CompetitionView::buildStandings(controller, club.getLeagueId());
  const auto row =
      std::ranges::find_if(table, [&club](const auto& standing)
                           { return standing.team_id == club.getId(); });
  league_position =
      row != table.end()
          ? static_cast<int>(std::distance(table.begin(), row)) + 1
          : 0;

  // Board deltas are stored newest first; the chart reads left to right.
  const BoardState& board = controller.getBoardState();
  for (size_t index = board.result_count; index > 0; --index)
    confidence_trend.push_back(board.recent_deltas[index - 1]);

  ticket_price_input = static_cast<int>(club.getProfile().ticket_price);
  fair_ticket_price = controller.getFairTicketPrice(club.getId());
  last_attendance = controller.getLastHomeAttendance(club.getId());

  // Roll of honour: the club's league and its national cup, newest first.
  const auto data = controller.getGameData();
  const LeagueID cup =
      data ? Competitions::rootLeague(*data, club.getLeagueId()) : 0;
  const auto& entries = controller.getSeasonHistory();
  for (auto entry = entries.rbegin(); entry != entries.rend(); ++entry)
  {
    const bool relevant = (entry->competition_type == MatchType::LEAGUE &&
                           entry->competition_id == club.getLeagueId()) ||
                          (entry->competition_type == MatchType::CUP &&
                           entry->competition_id == cup);
    if (!relevant) continue;
    HistoryRow line;
    line.season = std::format("{}/{:02}", entry->start_year,
                              (entry->start_year + 1) % 100);
    line.competition = entry->competition_name;
    line.champion = teamName(controller, entry->champion_id);
    line.runner_up = teamName(controller, entry->runner_up_id);
    if (entry->top_scorer_id != 0 && data)
    {
      const auto scorer = data->getPlayer(entry->top_scorer_id);
      line.top_scorer = std::format(
          "{} ({})", scorer ? scorer->get().getName() : std::string("–"),
          entry->top_scorer_goals);
    }
    if (!entry->promoted.empty())
      line.movements += fmt::sprintf(LOC("CLUB_PROMOTED"),
                                     joinTeams(controller, entry->promoted));
    if (!entry->relegated.empty())
    {
      if (!line.movements.empty()) line.movements += "  ·  ";
      line.movements += fmt::sprintf(LOC("CLUB_RELEGATED"),
                                     joinTeams(controller, entry->relegated));
    }
    history.push_back(std::move(line));
  }
}

void ClubScene::renderContent()
{
  GameController& controller = guiView->getController();
  const auto managed = controller.getManagedTeam();
  if (!managed) return;
  const Team& club = managed->get();
  const Theme::Palette& palette = Theme::palette();
  const BoardState& board = controller.getBoardState();
  const ClubProfile& profile = club.getProfile();

  UI::pageHeader(LOC("CLUB_TITLE"), LOC("CLUB_SUBTITLE"));

  const float gap = ImGui::GetStyle().ItemSpacing.x;
  const float tile = (ImGui::GetContentRegionAvail().x - 3.0f * gap) / 4.0f;
  const std::string confidence =
      std::format("{:.0f}%", static_cast<double>(board.confidence));
  const ImVec4 confidenceColor = board.confidence >= 60.0f   ? palette.positive
                                 : board.confidence >= 35.0f ? palette.warning
                                                             : palette.negative;
  UI::statTile("confidence", LOC("CLUB_TILE_CONFIDENCE"), confidence.c_str(),
               LOC(board.dismissed ? "CLUB_DISMISSED" : "CLUB_CONFIDENCE_NOTE"),
               confidenceColor, tile);
  ImGui::SameLine();
  const std::string target =
      fmt::sprintf(LOC("CLUB_TARGET_POSITION"), board.target_position);
  UI::statTile("objective", LOC("CLUB_TILE_OBJECTIVE"),
               LOC(BoardModel::objectiveKey(board.objective)), target.c_str(),
               palette.text, tile);
  ImGui::SameLine();
  const std::string reputation = std::format("{} / 100", profile.reputation);
  UI::statTile("reputation", LOC("CLUB_TILE_REPUTATION"), reputation.c_str(),
               LOC("CLUB_REPUTATION_NOTE"), palette.text, tile);
  ImGui::SameLine();
  const std::string capacity = Format::thousands(profile.stadium_capacity);
  UI::statTile("stadium", LOC("CLUB_TILE_STADIUM"), capacity.c_str(),
               LOC("CLUB_STADIUM_NOTE"), palette.text, tile);

  ImGui::Dummy(ImVec2(0.0f, Theme::Space::XS * Theme::scale()));
  const float available = ImGui::GetContentRegionAvail().x;
  const bool twoColumns = available >= TWO_COLUMN_MIN_WIDTH * Theme::scale();
  const float cardHeight = CARD_HEIGHT * Theme::scale();
  const float half =
      twoColumns ? std::floor((available - gap) * 0.5f) : available;
  renderBoard(half, cardHeight);
  if (twoColumns) ImGui::SameLine();
  renderStadium(twoColumns ? available - gap - half : available, cardHeight);
  renderHistory(
      std::max(ImGui::GetContentRegionAvail().y, 150.0f * Theme::scale()));
}

void ClubScene::renderBoard(float width, float height)
{
  const Theme::Palette& palette = Theme::palette();
  const BoardState& board = guiView->getController().getBoardState();
  UI::beginCard("club_board", LOC("CLUB_BOARD"), ImVec2(width, height));
  const float keyWidth = KEY_WIDTH * Theme::scale();
  UI::keyValue(LOC("CLUB_OBJECTIVE"),
               LOC(BoardModel::objectiveKey(board.objective)), keyWidth);
  const std::string expected =
      board.expected_position > 0
          ? fmt::sprintf(LOC("CLUB_POSITION_VALUE"), board.expected_position)
          : std::string("–");
  UI::keyValue(LOC("CLUB_EXPECTED_POSITION"), expected.c_str(), keyWidth);
  const std::string target =
      fmt::sprintf(LOC("CLUB_POSITION_VALUE"), board.target_position);
  UI::keyValue(LOC("CLUB_WORST_ACCEPTABLE"), target.c_str(), keyWidth);
  const std::string current =
      league_position > 0
          ? fmt::sprintf(LOC("CLUB_POSITION_VALUE"), league_position)
          : std::string("–");
  const bool onTarget =
      league_position > 0 && league_position <= board.target_position;
  ImGui::PushStyleColor(ImGuiCol_Text, league_position == 0 ? palette.text
                                       : onTarget           ? palette.positive
                                                            : palette.negative);
  UI::keyValue(LOC("CLUB_CURRENT_POSITION"), current.c_str(), keyWidth);
  ImGui::PopStyleColor();

  ImGui::Dummy(ImVec2(0.0f, Theme::Space::S * Theme::scale()));
  const std::string confidence =
      std::format("{:.0f}%", static_cast<double>(board.confidence));
  UI::meter(LOC("CLUB_CONFIDENCE"), board.confidence / 100.0f, keyWidth,
            board.confidence >= 60.0f   ? palette.positive
            : board.confidence >= 35.0f ? palette.warning
                                        : palette.negative,
            confidence.c_str());
  ImGui::Dummy(ImVec2(0.0f, Theme::Space::S * Theme::scale()));
  UI::sectionLabel(LOC("CLUB_RESULTS_VS_EXPECTATION"));
  if (confidence_trend.empty())
  {
    ImGui::TextColored(palette.faint, "%s", LOC("CLUB_NO_LEAGUE_RESULTS"));
  }
  else
  {
    UI::deltaBars(
        "board_trend", confidence_trend,
        ImVec2(ImGui::GetContentRegionAvail().x, 36.0f * Theme::scale()));
  }
  if (board.low_reviews > 0 && !board.dismissed)
    ImGui::TextColored(palette.warning, "%s", LOC("CLUB_BOARD_CONCERNED"));
  UI::endCard();
}

void ClubScene::renderStadium(float width, float height)
{
  GameController& controller = guiView->getController();
  const Theme::Palette& palette = Theme::palette();
  const ClubProfile& profile = controller.getManagedTeam()->get().getProfile();
  UI::beginCard("club_stadium", LOC("CLUB_MATCHDAY"), ImVec2(width, height));
  const float keyWidth = KEY_WIDTH * Theme::scale();
  const std::string price = Format::moneyFull(profile.ticket_price);
  UI::keyValue(LOC("CLUB_TICKET_PRICE"), price.c_str(), keyWidth);
  const std::string fair =
      fair_ticket_price > 0 ? Format::moneyFull(fair_ticket_price) : "–";
  UI::keyValue(LOC("CLUB_FAIR_PRICE"), fair.c_str(), keyWidth);
  const std::string crowd =
      last_attendance > 0
          ? fmt::sprintf(LOC("CLUB_ATTENDANCE_VALUE"),
                         Format::thousands(last_attendance).c_str(),
                         100.0 * static_cast<double>(last_attendance) /
                             static_cast<double>(std::max<uint32_t>(
                                 1, profile.stadium_capacity)))
          : std::string(LOC("CLUB_NO_HOME_MATCH"));
  UI::keyValue(LOC("CLUB_LAST_ATTENDANCE"), crowd.c_str(), keyWidth);

  ImGui::Dummy(ImVec2(0.0f, Theme::Space::XS * Theme::scale()));
  ImGui::AlignTextToFramePadding();
  ImGui::TextColored(palette.muted, "%s", LOC("CLUB_SET_PRICE"));
  ImGui::SameLine(keyWidth);
  ImGui::SetNextItemWidth(130.0f * Theme::scale());
  ImGui::InputInt("##ticket_price", &ticket_price_input, 1, 5);
  ticket_price_input = std::clamp(ticket_price_input, 1, MAX_TICKET_PRICE);
  ImGui::SameLine();
  ImGui::BeginDisabled(ticket_price_input ==
                       static_cast<int>(profile.ticket_price));
  if (UI::primaryButton(LOC("CLUB_APPLY_PRICE")) &&
      controller.setTicketPrice(static_cast<uint32_t>(ticket_price_input)))
    showToast(LOC("CLUB_PRICE_TOAST"));
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    ImGui::SetTooltip("%s", LOC("CLUB_PRICE_HELP"));

  ImGui::Dummy(ImVec2(0.0f, Theme::Space::S * Theme::scale()));
  UI::sectionLabel(LOC("CLUB_FACILITIES"));
  UI::attributeBar(LOC("CLUB_TRAINING_FACILITIES"),
                   static_cast<float>(profile.training_facilities), keyWidth);
  UI::attributeBar(LOC("CLUB_YOUTH_FACILITIES"),
                   static_cast<float>(profile.youth_facilities), keyWidth);
  UI::endCard();
}

void ClubScene::renderHistory(float height)
{
  const Theme::Palette& palette = Theme::palette();
  UI::beginCard("club_history", LOC("CLUB_HISTORY"), ImVec2(0.0f, height));
  if (history.empty())
  {
    UI::emptyState(LOC("CLUB_HISTORY_EMPTY_TITLE"),
                   LOC("CLUB_HISTORY_EMPTY_BODY"));
    UI::endCard();
    return;
  }
  if (UI::beginDataTable("history", 6,
                         ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                             ImGuiTableFlags_ScrollY |
                             ImGuiTableFlags_SizingFixedFit,
                         760.0f, ImVec2(0.0f, 0.0f), 1))
  {
    ImGui::TableSetupColumn(LOC("CLUB_COL_SEASON"));
    ImGui::TableSetupColumn(LOC("CLUB_COL_COMPETITION"));
    ImGui::TableSetupColumn(LOC("CLUB_COL_CHAMPION"));
    ImGui::TableSetupColumn(LOC("CLUB_COL_RUNNER_UP"));
    ImGui::TableSetupColumn(LOC("CLUB_COL_TOP_SCORER"));
    ImGui::TableSetupColumn(LOC("CLUB_COL_MOVEMENTS"),
                            ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();
    for (const HistoryRow& line : history)
    {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextColored(palette.muted, "%s", line.season.c_str());
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(line.competition.c_str());
      ImGui::TableNextColumn();
      ImGui::TextColored(palette.accent, "%s", line.champion.c_str());
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(line.runner_up.c_str());
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(line.top_scorer.c_str());
      ImGui::TableNextColumn();
      ImGui::TextColored(palette.muted, "%s", line.movements.c_str());
    }
    ImGui::EndTable();
  }
  UI::endCard();
}
