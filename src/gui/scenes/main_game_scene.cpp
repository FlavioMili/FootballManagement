// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "main_game_scene.h"

#include <fmt/printf.h>
#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <format>
#include <string>

#include "database/gamedata.h"
#include "global/language_manager.h"
#include "gui/gui_view.h"
#include "gui/scenes/lineup_scene.h"
#include "gui/scenes/match_scene.h"
#include "gui/scenes/team_selection_scene.h"
#include "gui/view_models/competition_view.h"
#include "gui/view_models/formation.h"
#include "gui/widgets/format.h"
#include "gui/widgets/theme.h"
#include "gui/widgets/widgets.h"
#include "model/game.h"

namespace
{
constexpr size_t MAX_KEY_PLAYERS = 11;
constexpr size_t MAX_RECENT_RESULTS = 5;
constexpr float TWO_COLUMN_MIN_WIDTH = 860.0f;
constexpr float NEXT_MATCH_HEIGHT = 172.0f;
constexpr int WEEKS_PER_YEAR = 52;
constexpr uint8_t SEASON_START_MONTH = 7;
constexpr size_t MAX_TREND_POINTS = 120;
constexpr size_t FINANCE_MONTH_DAYS = 30;
constexpr size_t CONTINUE_LOG_LINES = 4;
constexpr double CONTINUE_OVERLAY_DELAY = 0.15;
constexpr double CONTINUE_FADE_SECONDS = 0.12;
constexpr float CONTINUE_DIM = 120.0f;

std::string ordinalPosition(size_t position)
{
  return fmt::sprintf(LOC("DASHBOARD_POSITION_VALUE"), position);
}

}  // namespace

SceneID MainGameScene::getID() const { return SceneID::GAME_MENU; }

MainGameScene::MainGameScene(GUIView* guiViewPtr) : ManagementScene(guiViewPtr)
{
}

NavSection MainGameScene::navSection() const
{
  return active_page == Page::FINANCES ? NavSection::FINANCES
                                       : NavSection::HOME;
}

void MainGameScene::onEnter()
{
  if (!guiView->getController().hasSelectedTeam())
    guiView->overlayScene(std::make_unique<TeamSelectionScene>(guiView));
  else
    refreshData();
}

void MainGameScene::update(float /*deltaTime*/)
{
  // Wait for the frozen backdrop first so it shows the screen, not the card.
  if (continuation_requested && !continuation_running &&
      !guiView->isBackdropPending())
  {
    continuation_requested = false;
    startContinuation();
  }

  if (continuation_running) trackContinueProgress();

  if (continuation_running &&
      continue_operation.wait_for(std::chrono::seconds::zero()) ==
          std::future_status::ready)
  {
    continuation_running = false;
    guiView->releaseBackdrop();
    continue_log.clear();
    continue_finished_at = continue_overlay_shown ? ImGui::GetTime() : -1.0;
    try
    {
      const int advancedDays = continue_operation.get();
      showToast(
          fmt::sprintf(Format::plural("DASHBOARD_ADVANCED_DAYS", advancedDays),
                       advancedDays));
      refreshData();
    }
    catch (const std::exception&)
    {
      showToast(LOC("DASHBOARD_ADVANCE_FAILED"), true);
    }
  }

  // Team selection is an overlay, so the dashboard does not re-enter when the
  // choice closes. Refresh once as soon as a club becomes available.
  // Also refresh when the date moved on without this scene being told.
  const GameController& controller = guiView->getController();
  if (!continuation_running && controller.hasSelectedTeam() &&
      (cached_squad.empty() || !(cached_date == controller.getCurrentDate())))
    refreshData();
}

std::string MainGameScene::continueLabel() const
{
  const GameController& controller = guiView->getController();
  if (!controller.hasSelectedTeam()) return {};
  if (cached_next && cached_next->date == controller.getCurrentDate())
    return LOC(cached_unavailable_starters > 0 ? "DASHBOARD_FIX_LINEUP"
                                               : "DASHBOARD_PLAY_MATCH");
  if (cached_next)
    return fmt::sprintf(LOC("DASHBOARD_CONTINUE_TO"),
                        Format::dayMonth(cached_next->date));
  return LOC("DASHBOARD_CONTINUE");
}

void MainGameScene::requestContinue()
{
  const GameController& controller = guiView->getController();
  if (continuation_running || !controller.hasSelectedTeam()) return;
  if (cached_next && cached_next->date == controller.getCurrentDate() &&
      cached_unavailable_starters > 0)
  {
    guiView->navigateTo(std::make_unique<LineupScene>(guiView));
    return;
  }
  if (cached_next && cached_next->date == controller.getCurrentDate())
  {
    guiView->navigateTo(std::make_unique<MatchScene>(
        guiView, cached_next->home_id, cached_next->away_id));
    return;
  }
  // Simulation starts from update() once every screen above the hub has
  // closed, so no screen reads the game state while it changes.
  continuation_requested = true;
  guiView->requestBackdropCapture();
  if (guiView->getOverlayDepth() > 0) guiView->navigateTo(nullptr);
}

void MainGameScene::startContinuation()
{
  continuation_running = true;
  continue_start_date = guiView->getController().getCurrentDate();
  continue_progress = {};
  continue_logged_day = 0;
  continue_day_matches = 0;
  continue_log.clear();
  continue_started_at = ImGui::GetTime();
  continue_overlay_shown = false;
  GameController* controllerPtr = &guiView->getController();
  continue_operation =
      std::async(std::launch::async,
                 [controllerPtr, hasFixture = cached_next.has_value()]()
                 {
                   if (hasFixture)
                     return controllerPtr->advanceToNextManagedFixture();
                   controllerPtr->advanceDay();
                   return 1;
                 });
}

void MainGameScene::trackContinueProgress()
{
  continue_progress = guiView->getController().getContinueProgress();
  // A day's match count is only known while it is being simulated; days the
  // poll never saw with matches are left out rather than guessed.
  while (continue_logged_day < continue_progress.days_done)
  {
    if (continue_day_matches > 0)
    {
      continue_log.push_back(fmt::sprintf(
          LOC("CONTINUE_LOG_MATCHES"),
          Format::dayMonth(continue_start_date +
                           static_cast<size_t>(continue_logged_day)),
          continue_day_matches));
      if (continue_log.size() > CONTINUE_LOG_LINES)
        continue_log.erase(continue_log.begin());
    }
    continue_day_matches = 0;
    ++continue_logged_day;
  }
  continue_day_matches =
      std::max(continue_day_matches, continue_progress.matches_total);
}

void MainGameScene::renderContinueOverlay()
{
  const Theme::Palette& palette = Theme::palette();
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const ImVec2 min = viewport->WorkPos;
  const ImVec2 max(min.x + viewport->WorkSize.x, min.y + viewport->WorkSize.y);
  ImDrawList* drawList = ImGui::GetWindowDrawList();
  // Short waits (a quiet day) keep showing the frozen screen only; the card
  // appears once Continue takes longer than a blink, then fades in.
  const double elapsed = ImGui::GetTime() - continue_started_at;
  if (elapsed < CONTINUE_OVERLAY_DELAY) return;
  continue_overlay_shown = true;
  const float alpha =
      Theme::reducedMotion()
          ? 1.0f
          : std::clamp(static_cast<float>((elapsed - CONTINUE_OVERLAY_DELAY) /
                                          CONTINUE_FADE_SECONDS),
                       0.0f, 1.0f);
  drawList->AddRectFilled(
      min, max, IM_COL32(0, 0, 0, static_cast<int>(CONTINUE_DIM * alpha)));
  ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);

  const float scale = Theme::scale();
  const ImVec2 cardSize(460.0f * scale, 0.0f);
  ImGui::SetCursorScreenPos(
      ImVec2(min.x + (viewport->WorkSize.x - cardSize.x) * 0.5f,
             min.y + viewport->WorkSize.y * 0.30f));
  ImGui::PushStyleColor(ImGuiCol_ChildBg, palette.surface);
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f * scale);
  ImGui::PushStyleVar(
      ImGuiStyleVar_WindowPadding,
      ImVec2(Theme::Space::XL * scale, Theme::Space::L * scale));
  ImGui::BeginChild("##continue_card", cardSize,
                    ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY |
                        ImGuiChildFlags_AlwaysUseWindowPadding,
                    ImGuiWindowFlags_NoScrollbar);
  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor();

  UI::sectionLabel(LOC("CONTINUE_TITLE"));
  const GameDateValue shownDate =
      continue_start_date +
      static_cast<size_t>(std::max(0, continue_progress.days_done));
  {
    Theme::ScopedText heading(Theme::Text::HEADING);
    ImGui::TextUnformatted(Format::date(shownDate).c_str());
  }
  const bool known = continue_progress.days_total > 0;
  const float fraction = continue_progress.fraction();
  const float barHeight = 8.0f * scale;
  if (known)
    ImGui::ProgressBar(fraction, ImVec2(-FLT_MIN, barHeight), "");
  else if (!Theme::reducedMotion())
    ImGui::ProgressBar(-1.0f * static_cast<float>(ImGui::GetTime()),
                       ImVec2(-FLT_MIN, barHeight), "");
  if (known)
  {
    const std::string days = fmt::sprintf(
        LOC("CONTINUE_DAYS"),
        std::min(continue_progress.days_done + 1, continue_progress.days_total),
        continue_progress.days_total);
    ImGui::TextColored(palette.muted, "%s", days.c_str());
    ImGui::SameLine();
    UI::textRightColored(palette.muted,
                         std::format("{:.0f}%", fraction * 100.0f).c_str());
  }
  if (continue_progress.matches_total > 0)
    ImGui::TextUnformatted(fmt::sprintf(LOC("CONTINUE_MATCHES_NOW"),
                                        continue_progress.matches_done,
                                        continue_progress.matches_total)
                               .c_str());
  if (!continue_log.empty())
  {
    ImGui::Dummy(ImVec2(0.0f, Theme::Space::XS * scale));
    for (auto line = continue_log.rbegin(); line != continue_log.rend(); ++line)
      ImGui::TextColored(palette.faint, "%s", line->c_str());
  }
  ImGui::EndChild();
  ImGui::PopStyleVar();
}

float MainGameScene::continueFadeOut() const
{
  if (continuation_running || continue_finished_at < 0.0 ||
      Theme::reducedMotion())
    return 0.0f;
  const double since = ImGui::GetTime() - continue_finished_at;
  return std::clamp(1.0f - static_cast<float>(since / CONTINUE_FADE_SECONDS),
                    0.0f, 1.0f);
}

void MainGameScene::autoFixLineup()
{
  GameController& controller = guiView->getController();
  const auto managed = controller.getManagedTeam();
  if (!managed) return;
  Lineup& lineup = managed->get().getLineup();
  std::vector<const Player*> squad;
  for (const auto& player :
       controller.getPlayersForTeam(managed->get().getId()))
    squad.push_back(&player.get());
  const int preset = Formation::detectPreset(lineup);
  Formation::autoPickAvailable(
      lineup,
      Formation::PRESETS[preset >= 0 ? static_cast<size_t>(preset) : 0U], squad,
      cached_unavailable, controller.getStatsConfig());
  showToast(LOC("LINEUP_AUTO_PICKED"));
  refreshData();
}

void MainGameScene::renderContent()
{
  if (!guiView->getController().getManagedTeam())
  {
    UI::emptyState(LOC("DASHBOARD_CHOOSE_CLUB"), nullptr);
    return;
  }
  if (active_page == Page::FINANCES)
    renderFinances();
  else
    renderOverview();
}

void MainGameScene::renderOverview()
{
  GameController& controller = guiView->getController();
  const Team& club = controller.getManagedTeam()->get();
  const Theme::Palette& palette = Theme::palette();

  const auto league = controller.getLeagueById(club.getLeagueId());
  const std::string subtitle = fmt::sprintf(
      LOC("DASHBOARD_SUBTITLE"), league ? league->get().getName().c_str() : "",
      cached_season, Format::date(controller.getCurrentDate()).c_str());
  UI::pageHeader(club.getName().c_str(), subtitle.c_str());

  // Headline figures.
  UI::TileRow tiles(4);
  const float width = tiles.width();
  tiles.next();
  const auto rank = std::ranges::find_if(
      cached_table, [&club](const CompetitionView::StandingRow& row)
      { return row.team_id == club.getId(); });
  if (rank != cached_table.end() && rank->played > 0)
  {
    const std::string value = ordinalPosition(
        static_cast<size_t>(std::distance(cached_table.begin(), rank)) + 1);
    const std::string footnote = fmt::sprintf(LOC("DASHBOARD_POINTS_PLAYED"),
                                              rank->points, rank->played);
    UI::statTile("tile_position", LOC("DASHBOARD_TILE_POSITION"), value.c_str(),
                 footnote.c_str(), palette.text, width);
  }
  else
  {
    UI::statTile("tile_position", LOC("DASHBOARD_TILE_POSITION"), "–",
                 LOC("DASHBOARD_NO_RESULTS"), palette.muted, width);
  }
  tiles.next();
  const int64_t balance = club.getFinances().getBalance();
  const std::string balanceText = Format::money(balance);
  const std::string payrollText = fmt::sprintf(
      LOC("DASHBOARD_PAYROLL_FOOTNOTE"), Format::money(cached_payroll).c_str());
  UI::statTile("tile_balance", LOC("FINANCE_CASH"), balanceText.c_str(),
               payrollText.c_str(),
               balance < 0 ? palette.negative : palette.text, width);
  tiles.next();
  const std::string squadText = std::to_string(cached_squad.size());
  const std::string averageText =
      fmt::sprintf(LOC("DASHBOARD_AVERAGE_OVR"),
                   static_cast<double>(cached_average_overall));
  UI::statTile("tile_squad", LOC("DASHBOARD_TILE_SQUAD"), squadText.c_str(),
               averageText.c_str(), palette.text, width);
  tiles.next();
  const std::string valueText = Format::money(cached_squad_value);
  UI::statTile("tile_value", LOC("DASHBOARD_TILE_VALUE"), valueText.c_str(),
               LOC("DASHBOARD_VALUE_FOOTNOTE"), palette.text, width);

  ImGui::Dummy(ImVec2(0.0f, Theme::Space::XS * Theme::scale()));
  const float available = ImGui::GetContentRegionAvail().x;
  const float gap = ImGui::GetStyle().ItemSpacing.x;
  if (available >= TWO_COLUMN_MIN_WIDTH * Theme::scale())
  {
    const float leftWidth = std::floor((available - gap) * 0.58f);
    const float rightWidth = available - gap - leftWidth;
    const float columnHeight =
        std::max(ImGui::GetContentRegionAvail().y, 420.0f * Theme::scale());
    const float nextHeight = NEXT_MATCH_HEIGHT * Theme::scale();
    ImGui::BeginGroup();
    renderNextMatchCard(leftWidth, nextHeight);
    renderStandingsCard(
        leftWidth, columnHeight - nextHeight - ImGui::GetStyle().ItemSpacing.y);
    ImGui::EndGroup();
    ImGui::SameLine();
    ImGui::BeginGroup();
    const float recentHeight =
        std::min(columnHeight * 0.45f,
                 ImGui::GetTextLineHeightWithSpacing() *
                         (static_cast<float>(MAX_RECENT_RESULTS) + 2.5f) +
                     2.0f * Theme::Space::M * Theme::scale());
    renderRecentResultsCard(rightWidth, recentHeight);
    renderKeyPlayersCard(rightWidth, columnHeight - recentHeight -
                                         ImGui::GetStyle().ItemSpacing.y);
    ImGui::EndGroup();
  }
  else
  {
    renderNextMatchCard(available, NEXT_MATCH_HEIGHT * Theme::scale());
    renderRecentResultsCard(available, 230.0f * Theme::scale());
    renderKeyPlayersCard(available, 320.0f * Theme::scale());
    renderStandingsCard(available, 520.0f * Theme::scale());
  }
}

void MainGameScene::renderNextMatchCard(float width, float height)
{
  GameController& controller = guiView->getController();
  const Theme::Palette& palette = Theme::palette();
  const TeamID clubId = controller.getManagedTeam()->get().getId();
  UI::beginCard("next_match", LOC("DASHBOARD_NEXT_FIXTURE"),
                ImVec2(width, height));
  if (!cached_next)
  {
    UI::emptyState(LOC("DASHBOARD_NO_FIXTURE"), nullptr);
    UI::endCard();
    return;
  }
  const CompetitionView::FixtureRow& next = *cached_next;
  const bool home = next.home_id == clubId;
  const TeamID opponentId = home ? next.away_id : next.home_id;

  UI::badge(LOC(CompetitionView::matchTypeKey(next.type)), palette.info);
  ImGui::SameLine();
  UI::badge(LOC(home ? "FIXTURE_HOME" : "FIXTURE_AWAY"),
            home ? palette.positive : palette.warning);
  ImGui::SameLine();
  const bool today = next.date == controller.getCurrentDate();
  ImGui::TextColored(
      today ? palette.accent : palette.muted, "%s",
      today ? LOC("FIXTURE_TODAY") : Format::date(next.date).c_str());

  ImGui::Dummy(ImVec2(0.0f, Theme::Space::XS * Theme::scale()));
  {
    Theme::ScopedText heading(Theme::Text::HEADING);
    const std::string versus = LOC("FIXTURE_VS");
    const float inner = ImGui::GetContentRegionAvail().x;
    const float versusWidth = ImGui::CalcTextSize(versus.c_str()).x;
    const float side =
        (inner - versusWidth) * 0.5f - Theme::Space::M * Theme::scale();
    const float homeWidth = ImGui::CalcTextSize(next.home_name.c_str()).x;
    const float startX = ImGui::GetCursorPosX();
    ImGui::SetCursorPosX(startX + std::max(0.0f, side - homeWidth));
    ImGui::PushClipRect(ImGui::GetCursorScreenPos(),
                        ImVec2(ImGui::GetCursorScreenPos().x + side,
                               ImGui::GetCursorScreenPos().y + 100.0f),
                        true);
    ImGui::TextUnformatted(next.home_name.c_str());
    ImGui::PopClipRect();
    ImGui::SameLine(startX + side + Theme::Space::M * Theme::scale());
    ImGui::TextColored(palette.faint, "%s", versus.c_str());
    ImGui::SameLine(startX + side + versusWidth +
                    2.0f * Theme::Space::M * Theme::scale());
    ImGui::TextUnformatted(next.away_name.c_str());
  }

  // Opponent context from the cached table.
  const auto opponentRow = std::ranges::find_if(
      cached_table, [opponentId](const CompetitionView::StandingRow& row)
      { return row.team_id == opponentId; });
  if (opponentRow != cached_table.end() && opponentRow->played > 0)
  {
    const auto position = static_cast<size_t>(
        std::distance(cached_table.begin(), opponentRow) + 1);
    const std::string context =
        fmt::sprintf(LOC("DASHBOARD_OPPONENT_CONTEXT"),
                     ordinalPosition(position).c_str(), opponentRow->points);
    ImGui::TextColored(palette.muted, "%s", context.c_str());
    ImGui::SameLine();
    UI::formStrip(std::span(opponentRow->form.data(), opponentRow->form_count));
  }

  const float buttonsY =
      height - ImGui::GetFrameHeight() - ImGui::GetStyle().WindowPadding.y;
  ImGui::SetCursorPosY(std::max(ImGui::GetCursorPosY(), buttonsY));
  if (ImGui::Button(LOC("NAV_LINEUP")))
    Navigation::open(guiView, NavSection::LINEUP);
  ImGui::SameLine();
  if (ImGui::Button(LOC("NAV_TACTICS")))
    Navigation::open(guiView, NavSection::TACTICS);
  ImGui::SameLine();
  if (ImGui::Button(LOC("DASHBOARD_SCOUT_OPPONENT")))
    Navigation::openClub(guiView, opponentId);

  // Match readiness on the same row, right-aligned: players who cannot play
  // this fixture (injured, suspended) do not count as ready starters.
  const size_t ready = cached_starters - cached_unavailable_starters;
  const std::string readiness =
      cached_unavailable_starters > 0
          ? fmt::sprintf(LOC("DASHBOARD_XI_UNAVAILABLE"), ready,
                         cached_unavailable_starters)
          : fmt::sprintf(LOC("DASHBOARD_STARTING_XI"), cached_starters);
  const char* fixLabel = LOC("DASHBOARD_AUTO_FIX");
  const float fixWidth =
      cached_unavailable_starters > 0
          ? UI::buttonWidth(fixLabel) + ImGui::GetStyle().ItemSpacing.x
          : 0.0f;
  const float readinessWidth =
      ImGui::CalcTextSize(readiness.c_str()).x + fixWidth;
  UI::sameLineIfFits(readinessWidth);
  ImGui::SetCursorPosX(
      std::max(ImGui::GetCursorPosX(),
               ImGui::GetWindowContentRegionMax().x - readinessWidth));
  ImGui::AlignTextToFramePadding();
  ImGui::TextColored(cached_unavailable_starters > 0 ? palette.negative
                     : cached_starters == 11         ? palette.positive
                                                     : palette.warning,
                     "%s", readiness.c_str());
  if (cached_unavailable_starters > 0)
  {
    ImGui::SameLine();
    if (UI::primaryButton(fixLabel)) autoFixLineup();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
      ImGui::SetTooltip("%s", LOC("DASHBOARD_AUTO_FIX_HELP"));
  }
  UI::endCard();
}

void MainGameScene::renderStandingsCard(float width, float height)
{
  const Theme::Palette& palette = Theme::palette();
  const TeamID clubId =
      guiView->getController().getManagedTeam()->get().getId();
  UI::beginCard("league_table", LOC("DASHBOARD_LEAGUE_TABLE"),
                ImVec2(width, height));
  if (ImGui::BeginTable("dashboard_table", 6,
                        ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                            ImGuiTableFlags_BordersInnerH |
                            ImGuiTableFlags_SizingFixedFit))
  {
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn(LOC("MAIN_GAME_POS"));
    ImGui::TableSetupColumn(LOC("MAIN_GAME_TEAM"),
                            ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn(LOC("TABLE_COL_PLAYED"));
    ImGui::TableSetupColumn(LOC("TABLE_COL_GD"));
    ImGui::TableSetupColumn(LOC("MAIN_GAME_PTS"));
    ImGui::TableSetupColumn(LOC("TABLE_COL_FORM"));
    UI::staticHeadersRow();
    for (size_t index = 0; index < cached_table.size(); ++index)
    {
      const CompetitionView::StandingRow& row = cached_table[index];
      // Before the first round the order is alphabetical: no zones yet.
      const bool promoted = row.played > 0 && index < cached_zones.promotion;
      const bool relegated =
          row.played > 0 &&
          index + cached_zones.relegation >= cached_table.size();
      ImGui::TableNextRow();
      if (row.team_id == clubId)
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1,
                               Theme::toU32(palette.accent, 0.16f));
      ImGui::TableNextColumn();
      ImGui::TextColored(promoted    ? palette.positive
                         : relegated ? palette.negative
                                     : palette.muted,
                         "%zu", index + 1);
      ImGui::TableNextColumn();
      ImGui::PushID(static_cast<int>(row.team_id));
      if (ImGui::Selectable(row.name.c_str(), false,
                            ImGuiSelectableFlags_SpanAllColumns))
        Navigation::openClub(guiView, row.team_id);
      ImGui::PopID();
      ImGui::TableNextColumn();
      ImGui::Text("%d", row.played);
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(Format::signedInt(row.goalDifference()).c_str());
      ImGui::TableNextColumn();
      ImGui::Text("%d", row.points);
      ImGui::TableNextColumn();
      UI::formStrip(std::span(row.form.data(), row.form_count));
    }
    ImGui::EndTable();
  }
  UI::endCard();
}

void MainGameScene::renderRecentResultsCard(float width, float height)
{
  const Theme::Palette& palette = Theme::palette();
  const TeamID clubId =
      guiView->getController().getManagedTeam()->get().getId();
  UI::beginCard("recent_results", LOC("DASHBOARD_RECENT_RESULTS"),
                ImVec2(width, height));
  if (cached_recent.empty())
  {
    UI::emptyState(LOC("DASHBOARD_NO_RESULTS"), nullptr);
    UI::endCard();
    return;
  }
  if (ImGui::BeginTable("recent", 4, ImGuiTableFlags_SizingFixedFit))
  {
    ImGui::TableSetupColumn("date");
    ImGui::TableSetupColumn("opponent", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("score");
    ImGui::TableSetupColumn("outcome");
    for (const CompetitionView::FixtureRow& fixture : cached_recent)
    {
      const bool home = fixture.home_id == clubId;
      const UI::Outcome outcome = CompetitionView::outcomeFor(fixture, clubId);
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextColored(palette.muted, "%s",
                         Format::dayMonth(fixture.date).c_str());
      ImGui::TableNextColumn();
      ImGui::TextColored(
          palette.faint, "%s",
          LOC(home ? "FIXTURE_HOME_SHORT" : "FIXTURE_AWAY_SHORT"));
      ImGui::SameLine();
      ImGui::PushID(&fixture);
      if (ImGui::Selectable(
              home ? fixture.away_name.c_str() : fixture.home_name.c_str(),
              false, ImGuiSelectableFlags_SpanAllColumns))
        Navigation::openMatchReport(guiView, fixture.date, fixture.home_id,
                                    fixture.away_id);
      ImGui::PopID();
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
        ImGui::SetTooltip("%s", LOC("FIXTURES_OPEN_REPORT_HINT"));
      ImGui::TableNextColumn();
      ImGui::Text("%u – %u", fixture.home_score, fixture.away_score);
      ImGui::TableNextColumn();
      UI::formStrip(std::span(&outcome, 1));
    }
    ImGui::EndTable();
  }
  UI::endCard();
}

void MainGameScene::renderKeyPlayersCard(float width, float height)
{
  const Theme::Palette& palette = Theme::palette();
  UI::beginCard("key_players", LOC("DASHBOARD_KEY_PLAYERS"),
                ImVec2(width, height));
  if (ImGui::BeginTable(
          "key_players_table", 3,
          ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY))
  {
    ImGui::TableSetupColumn("name", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("role");
    ImGui::TableSetupColumn("ovr");
    for (size_t index = 0;
         index < MAX_KEY_PLAYERS && index < cached_squad.size(); ++index)
    {
      const PlayerView::PlayerRow& player = cached_squad[index];
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::PushID(static_cast<int>(player.id));
      if (ImGui::Selectable(player.name.c_str(), false,
                            ImGuiSelectableFlags_SpanAllColumns))
        Navigation::openPlayer(guiView, player.id);
      ImGui::PopID();
      ImGui::TableNextColumn();
      ImGui::TextColored(palette.muted, "%s", player.role.c_str());
      ImGui::TableNextColumn();
      UI::ratingChip(player.overall);
    }
    ImGui::EndTable();
  }
  UI::endCard();
}

void MainGameScene::renderFinances()
{
  GameController& controller = guiView->getController();
  const Team& club = controller.getManagedTeam()->get();
  const Finances& finances = club.getFinances();
  const Theme::Palette& palette = Theme::palette();

  UI::pageHeader(LOC("FINANCE_TITLE"), LOC("FINANCE_SUBTITLE"));

  UI::TileRow tiles(4);
  const float width = tiles.width();
  tiles.next();
  const std::string cash = Format::money(finances.getBalance());
  const std::string cashFull = Format::moneyFull(finances.getBalance());
  UI::statTile("fin_cash", LOC("FINANCE_CASH"), cash.c_str(), cashFull.c_str(),
               finances.getBalance() < 0 ? palette.negative : palette.text,
               width);
  tiles.next();
  const std::string budget = Format::money(finances.getTransferBudget());
  UI::statTile("fin_budget", LOC("FINANCE_TRANSFER_BUDGET"), budget.c_str(),
               LOC("FINANCE_BUDGET_FOOTNOTE"), palette.text, width);
  tiles.next();
  const int64_t wageBudget = finances.getWageBudget();
  const std::string payroll = Format::money(cached_payroll);
  const std::string payrollNote =
      wageBudget > 0
          ? fmt::sprintf(LOC("FINANCE_WAGE_BUDGET_NOTE"),
                         Format::money(wageBudget).c_str(),
                         100.0 * static_cast<double>(cached_payroll) /
                             static_cast<double>(wageBudget))
          : std::string(LOC("FINANCE_ANNUAL_FOOTNOTE"));
  UI::statTile("fin_payroll", LOC("FINANCE_PAYROLL"), payroll.c_str(),
               payrollNote.c_str(),
               wageBudget > 0 && cached_payroll > wageBudget ? palette.negative
                                                             : palette.text,
               width);
  tiles.next();
  // The opening balance is a carried-over figure, not this season's result.
  const int64_t seasonNet =
      cached_season_summary.net() -
      cached_season_summary
          .by_category[static_cast<size_t>(FinanceCategory::OpeningBalance)];
  const std::string net = Format::money(seasonNet);
  UI::statTile("fin_net", LOC("FINANCE_SEASON_NET"), net.c_str(),
               LOC("FINANCE_SEASON_NET_NOTE"),
               seasonNet < 0 ? palette.negative : palette.positive, width);

  ImGui::Dummy(ImVec2(0.0f, Theme::Space::XS * Theme::scale()));
  const float available = ImGui::GetContentRegionAvail().x;
  const float gap = ImGui::GetStyle().ItemSpacing.x;
  const bool twoColumns = available >= TWO_COLUMN_MIN_WIDTH * Theme::scale();
  const float leftWidth =
      twoColumns ? std::floor((available - gap) * 0.45f) : available;
  const float rightWidth = twoColumns ? available - gap - leftWidth : available;
  const float cardHeight =
      twoColumns
          ? std::max(ImGui::GetContentRegionAvail().y, 320.0f * Theme::scale())
          : 320.0f * Theme::scale();

  // Income and expenses by category, this season or the last 30 days.
  UI::beginCard("fin_breakdown", nullptr, ImVec2(leftWidth, cardHeight), true);
  UI::sectionLabel(LOC(finance_show_month ? "FINANCE_BREAKDOWN_MONTH"
                                          : "FINANCE_BREAKDOWN"));
  ImGui::SameLine();
  const char* periodToggle =
      LOC(finance_show_month ? "FINANCE_SHOW_SEASON" : "FINANCE_SHOW_MONTH");
  ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(),
                                ImGui::GetWindowContentRegionMax().x -
                                    ImGui::CalcTextSize(periodToggle).x -
                                    2.0f * ImGui::GetStyle().FramePadding.x));
  if (ImGui::SmallButton(periodToggle))
    finance_show_month = !finance_show_month;
  const std::vector<FinanceBar>& periodBars =
      finance_show_month ? cached_month_bars : cached_finance_bars;
  if (periodBars.empty())
  {
    ImGui::TextColored(palette.faint, "%s", LOC("FINANCE_NO_TRANSACTIONS"));
  }
  else
  {
    std::vector<UI::BarDatum> bars;
    bars.reserve(periodBars.size());
    for (const FinanceBar& bar : periodBars)
      bars.push_back({LOC(bar.label_key),
                      static_cast<float>(std::llabs(bar.amount)),
                      bar.amount >= 0 ? palette.positive : palette.negative,
                      bar.amount_text});
    UI::barChart("fin_bars", bars, ImGui::GetContentRegionAvail().x,
                 130.0f * Theme::scale());
  }
  ImGui::Dummy(ImVec2(0.0f, Theme::Space::S * Theme::scale()));
  UI::sectionLabel(LOC("FINANCE_BUDGET_ADJUST"));
  ImGui::PushTextWrapPos(0.0f);
  ImGui::TextColored(palette.faint, "%s", LOC("FINANCE_BUDGET_ADJUST_HELP"));
  ImGui::PopTextWrapPos();
  const float step = 100'000.0f;
  ImGui::SetNextItemWidth(std::min(260.0f * Theme::scale(),
                                   ImGui::GetContentRegionAvail().x * 0.6f));
  const std::string adjustLabel =
      Format::money(static_cast<int64_t>(budget_shift));
  ImGui::SliderFloat("##budget_shift", &budget_shift,
                     -static_cast<float>(std::min<int64_t>(
                         wageBudget * WEEKS_PER_YEAR, 50'000'000)),
                     static_cast<float>(finances.getTransferBudget()),
                     adjustLabel.c_str());
  budget_shift = std::round(budget_shift / step) * step;
  ImGui::SameLine();
  ImGui::BeginDisabled(budget_shift == 0.0f);
  if (UI::primaryButton(LOC("FINANCE_BUDGET_APPLY")))
  {
    if (controller.moveTransferToWageBudget(static_cast<int64_t>(budget_shift)))
    {
      showToast(LOC("FINANCE_BUDGET_TOAST"));
      budget_shift = 0.0f;
      refreshData();
    }
    else
    {
      showToast(LOC("FINANCE_BUDGET_REFUSED"), true);
    }
  }
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    ImGui::SetTooltip("%s", LOC("FINANCE_BUDGET_SLIDER_HELP"));
  UI::endCard();
  if (twoColumns) ImGui::SameLine();

  // Balance trend and the ledger itself.
  UI::beginCard("fin_ledger", LOC("FINANCE_LEDGER"),
                ImVec2(rightWidth, cardHeight), true);
  if (cached_balance_trend.size() >= 2)
  {
    UI::sparkline(
        "fin_trend", cached_balance_trend,
        ImVec2(ImGui::GetContentRegionAvail().x, 46.0f * Theme::scale()),
        palette.info);
    ImGui::Dummy(ImVec2(0.0f, Theme::Space::XS * Theme::scale()));
  }
  if (cached_ledger.empty())
  {
    ImGui::TextColored(palette.faint, "%s", LOC("FINANCE_NO_TRANSACTIONS"));
  }
  else if (ImGui::BeginTable(
               "ledger", 3,
               ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                   ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit))
  {
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn(LOC("FIXTURES_COL_DATE"));
    ImGui::TableSetupColumn(LOC("FINANCE_COL_CATEGORY"),
                            ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn(LOC("FINANCE_COL_AMOUNT"));
    UI::staticHeadersRow();
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(cached_ledger.size()));
    while (clipper.Step())
    {
      for (int line = clipper.DisplayStart; line < clipper.DisplayEnd; ++line)
      {
        const LedgerRow& entry = cached_ledger[static_cast<size_t>(line)];
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(palette.muted, "%s", entry.date.c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(LOC(entry.category_key));
        ImGui::TableNextColumn();
        UI::textRightColored(
            entry.amount >= 0 ? palette.positive : palette.negative,
            entry.amount_text.c_str());
      }
    }
    ImGui::EndTable();
  }
  UI::endCard();
}

void MainGameScene::refreshData()
{
  GameController& controller = guiView->getController();
  cached_date = controller.getCurrentDate();
  cached_season = controller.getCurrentSeason();
  const auto managed = controller.getManagedTeam();
  if (!managed) return;
  const Team& club = managed->get();

  cached_table =
      CompetitionView::buildStandings(controller, club.getLeagueId());
  cached_zones = CompetitionView::zonesFor(controller, club.getLeagueId());
  cached_unavailable = PlayerView::unavailablePlayers(
      controller, club.getId(),
      cached_next ? cached_next->type : MatchType::LEAGUE);
  const Lineup& lineup = club.getLineup();
  cached_starters = lineup.getOutfieldPlayers().size() +
                    (lineup.getGoalkeeper() != nullptr ? 1U : 0U);
  cached_unavailable_starters =
      Formation::unavailableStarters(lineup, cached_unavailable);
  cached_next = CompetitionView::nextFixture(controller, club.getId());

  cached_recent.clear();
  const auto fixtures =
      CompetitionView::buildClubFixtures(controller, club.getId());
  for (auto fixture = fixtures.rbegin();
       fixture != fixtures.rend() && cached_recent.size() < MAX_RECENT_RESULTS;
       ++fixture)
  {
    if (fixture->played) cached_recent.push_back(*fixture);
  }

  cached_squad.clear();
  cached_payroll = 0;
  cached_squad_value = 0;
  double overallSum = 0.0;
  for (const auto& playerRef : controller.getPlayersForTeam(club.getId()))
  {
    PlayerView::PlayerRow row =
        PlayerView::makeRow(controller, playerRef.get());
    cached_payroll += row.wage;
    cached_squad_value += row.market_value;
    overallSum += static_cast<double>(row.overall);
    cached_squad.push_back(std::move(row));
  }
  cached_average_overall =
      cached_squad.empty()
          ? 0.0f
          : static_cast<float>(overallSum /
                               static_cast<double>(cached_squad.size()));
  std::ranges::sort(cached_squad, [](const PlayerView::PlayerRow& left,
                                     const PlayerView::PlayerRow& right)
                    { return left.overall > right.overall; });

  // Finance view models: season summary, ledger rows and balance trend.
  const GameDateValue today = controller.getCurrentDate();
  const GameDateValue seasonStart(
      static_cast<uint16_t>(today.month >= SEASON_START_MONTH ? today.year
                                                              : today.year - 1),
      SEASON_START_MONTH, 1);
  cached_season_summary =
      controller.getFinanceSummary(club.getId(), seasonStart, today);
  const auto buildBars = [](const FinanceSummary& summary)
  {
    std::vector<FinanceBar> bars;
    for (size_t index = 0; index < FINANCE_CATEGORY_COUNT; ++index)
    {
      const auto category = static_cast<FinanceCategory>(index);
      const int64_t amount = summary.by_category[index];
      if (amount == 0 || category == FinanceCategory::OpeningBalance) continue;
      bars.push_back(
          {financeCategoryKey(category), amount, Format::money(amount)});
    }
    std::ranges::sort(bars, [](const auto& left, const auto& right)
                      { return left.amount > right.amount; });
    return bars;
  };
  cached_finance_bars = buildBars(cached_season_summary);
  cached_month_bars = buildBars(controller.getFinanceSummary(
      club.getId(), today - FINANCE_MONTH_DAYS, today));
  const auto& ledger = controller.getFinanceLedger(club.getId());
  cached_ledger.clear();
  cached_ledger.reserve(ledger.size());
  for (auto entry = ledger.rbegin(); entry != ledger.rend(); ++entry)
    cached_ledger.push_back({Format::date(entry->date),
                             financeCategoryKey(entry->category), entry->amount,
                             Format::money(entry->amount)});
  cached_balance_trend.clear();
  int64_t running = 0;
  const size_t trendStart =
      ledger.size() > MAX_TREND_POINTS ? ledger.size() - MAX_TREND_POINTS : 0;
  for (size_t index = 0; index < ledger.size(); ++index)
  {
    running += ledger[index].amount;
    if (index >= trendStart)
      cached_balance_trend.push_back(static_cast<float>(running));
  }
}
