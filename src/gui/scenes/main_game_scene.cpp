// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "main_game_scene.h"

#include <imgui.h>

#include <algorithm>

#include "global/language_manager.h"
#include "gui/gui_constants.h"
#include "gui/gui_view.h"
#include "gui/scenes/lineup_scene.h"
#include "gui/scenes/main_menu_scene.h"
#include "gui/scenes/match_scene.h"
#include "gui/scenes/roster_scene.h"
#include "gui/scenes/strategy_scene.h"
#include "gui/scenes/team_selection_scene.h"
#include "gui/scenes/transfer_market_scene.h"
#include "model/game.h"
#include "model/role_utils.h"

namespace
{
constexpr float SIDEBAR_WIDTH = 200.0f;
constexpr float NEXT_DAY_BUTTON_WIDTH = 150.0f;
constexpr float NEXT_DAY_BUTTON_HEIGHT = 30.0f;
constexpr float NEXT_DAY_BUTTON_OFFSET = 160.0f;
constexpr float SIDEBAR_BUTTON_HEIGHT = 40.0f;
constexpr int STANDINGS_COLUMNS = 3;
constexpr int TOP_PLAYERS_COLUMNS = 3;
constexpr float POS_COL_WIDTH = 30.0f;
constexpr float PTS_COL_WIDTH = 50.0f;
constexpr float OVR_COL_WIDTH = 40.0f;
constexpr size_t MAX_TOP_PLAYERS = 5;
}  // namespace

SceneID MainGameScene::getID() const { return SceneID::GAME_MENU; }

MainGameScene::MainGameScene(GUIView* guiView_ptr) : GUIScene(guiView_ptr) {}

void MainGameScene::onEnter()
{
  if (!guiView->getController().hasSelectedTeam())
  {
    guiView->overlayScene(std::make_unique<TeamSelectionScene>(guiView));
  }
  else
  {
    refreshData();
  }
}

void MainGameScene::update(float deltaTime) { (void)deltaTime; }

void MainGameScene::render()
{
  ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::Begin(LOC("MAIN_GAME_TITLE"), nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoBringToFrontOnFocus |
                   ImGuiWindowFlags_NoSavedSettings);

  renderTopBar();
  ImGui::Separator();

  // Split into sidebar and main area
  ImGui::Columns(2, "MainLayout", false);
  ImGui::SetColumnWidth(0, SIDEBAR_WIDTH);

  renderSidebar();

  ImGui::NextColumn();

  renderMainArea();

  ImGui::Columns(1);
  ImGui::End();
}

void MainGameScene::renderTopBar()
{
  std::string dateStr = guiView->getController().getCurrentDate().toString();
  ImGui::Text(LOC("MAIN_GAME_DATE"), dateStr.c_str());
  ImGui::SameLine(ImGui::GetWindowWidth() - NEXT_DAY_BUTTON_OFFSET);

  // Check if managed team has a match today
  bool has_match_today = false;
  uint16_t opp_id = 0;
  if (auto managed = guiView->getController().getManagedTeam())
  {
    uint16_t tid = managed->get().getId();
    const auto& matches =
        guiView->getController().getGame()->getCalendar().getMatchesForDate(
            guiView->getController().getCurrentDate());
    for (const auto& m : matches)
    {
      if (m.getHomeTeamId() == tid || m.getAwayTeamId() == tid)
      {
        has_match_today = true;
        opp_id =
            (m.getHomeTeamId() == tid) ? m.getAwayTeamId() : m.getHomeTeamId();
        break;
      }
    }
  }

  if (has_match_today)
  {
    if (ImGui::Button("Play Match",
                      ImVec2(NEXT_DAY_BUTTON_WIDTH, NEXT_DAY_BUTTON_HEIGHT)))
    {
      uint16_t tid = guiView->getController().getManagedTeam()->get().getId();
      guiView->overlayScene(std::make_unique<MatchScene>(guiView, tid, opp_id));
    }
  }
  else
  {
    if (ImGui::Button(LOC("MAIN_GAME_NEXT_DAY"),
                      ImVec2(NEXT_DAY_BUTTON_WIDTH, NEXT_DAY_BUTTON_HEIGHT)))
    {
      guiView->getController().advanceDay();
      refreshData();
    }
  }
}

void MainGameScene::renderSidebar()
{
  ImGui::BeginChild("Sidebar", ImVec2(0, 0), true);
  if (ImGui::Button(LOC("MAIN_GAME_VIEW_ROSTER"),
                    ImVec2(-FLT_MIN, SIDEBAR_BUTTON_HEIGHT)))
  {
    guiView->overlayScene(std::make_unique<RosterScene>(guiView));
  }
  ImGui::Spacing();
  if (ImGui::Button("Lineup", ImVec2(-FLT_MIN, SIDEBAR_BUTTON_HEIGHT)))
  {
    guiView->overlayScene(std::make_unique<LineupScene>(guiView));
  }
  ImGui::Spacing();
  if (ImGui::Button(LOC("MAIN_GAME_SET_STRATEGY"),
                    ImVec2(-FLT_MIN, SIDEBAR_BUTTON_HEIGHT)))
  {
    guiView->overlayScene(std::make_unique<StrategyScene>(guiView));
  }
  ImGui::Spacing();
  if (ImGui::Button(LOC("MAIN_GAME_FINANCES"),
                    ImVec2(-FLT_MIN, SIDEBAR_BUTTON_HEIGHT)))
  {
  }
  ImGui::Spacing();
  if (ImGui::Button(LOC("MAIN_GAME_TRANSFER_MARKET"),
                    ImVec2(-FLT_MIN, SIDEBAR_BUTTON_HEIGHT)))
  {
    guiView->overlayScene(std::make_unique<TransferMarketScene>(guiView));
  }
  ImGui::Spacing();
  if (ImGui::Button(LOC("MAIN_GAME_SAVE_GAME"),
                    ImVec2(-FLT_MIN, SIDEBAR_BUTTON_HEIGHT)))
  {
    guiView->getController().saveGame();
  }
  ImGui::Spacing();
  if (ImGui::Button(LOC("MAIN_GAME_MAIN_MENU"),
                    ImVec2(-FLT_MIN, SIDEBAR_BUTTON_HEIGHT)))
  {
    changeScene(std::make_unique<MainMenuScene>(guiView));
  }
  ImGui::EndChild();
}

void MainGameScene::renderMainArea()
{
  ImGui::BeginChild("MainArea", ImVec2(0, 0), false);

  ImGui::Columns(2, "MainContent", false);

  // Left: Standings
  ImGui::Text("%s", LOC("MAIN_GAME_LEAGUE_STANDINGS"));
  ImGui::Separator();

  if (ImGui::BeginTable("Standings", STANDINGS_COLUMNS,
                        ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders))
  {
    ImGui::TableSetupColumn(LOC("MAIN_GAME_POS"),
                            ImGuiTableColumnFlags_WidthFixed, POS_COL_WIDTH);
    ImGui::TableSetupColumn(LOC("MAIN_GAME_TEAM"));
    ImGui::TableSetupColumn(LOC("MAIN_GAME_PTS"),
                            ImGuiTableColumnFlags_WidthFixed, PTS_COL_WIDTH);
    ImGui::TableHeadersRow();

    int rank = 1;
    for (const auto& [teamId, points] : cached_standings)
    {
      auto teamOpt =
          guiView->getController().getTeamById(static_cast<uint16_t>(teamId));
      if (!teamOpt.has_value())
      {
        continue;
      }

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("%d", rank);
      ImGui::TableNextColumn();
      ImGui::Text("%s", teamOpt->get().getName().c_str());
      ImGui::TableNextColumn();
      ImGui::Text("%d", points);
      rank++;
    }
    ImGui::EndTable();
  }

  ImGui::NextColumn();

  // Right: Top Players
  ImGui::Text("%s", LOC("MAIN_GAME_TOP_PLAYERS"));
  ImGui::Separator();

  if (ImGui::BeginTable("TopPlayers", TOP_PLAYERS_COLUMNS,
                        ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders))
  {
    ImGui::TableSetupColumn(LOC("MAIN_GAME_NAME"));
    ImGui::TableSetupColumn(LOC("MAIN_GAME_ROLE"));
    ImGui::TableSetupColumn(LOC("MAIN_GAME_OVR"),
                            ImGuiTableColumnFlags_WidthFixed, OVR_COL_WIDTH);
    ImGui::TableHeadersRow();

    const auto& stats_config = guiView->getController().getStatsConfig();
    for (size_t i = 0; i < MAX_TOP_PLAYERS && i < cached_top_players.size();
         ++i)
    {
      ImGui::TableNextRow();
      const auto& player = cached_top_players[i].get();
      ImGui::TableNextColumn();
      ImGui::Text("%s", player.getName().c_str());
      ImGui::TableNextColumn();
      ImGui::Text("%s", RoleUtils::toString(player.getRole()).c_str());
      ImGui::TableNextColumn();
      ImGui::Text("%.1f", player.getOverall(stats_config));
    }
    ImGui::EndTable();
  }

  ImGui::Columns(1);
  ImGui::EndChild();
}

void MainGameScene::refreshData()
{
  cached_date = guiView->getController().getCurrentDate();
  cached_season = guiView->getController().getCurrentSeason();

  auto managedTeamOpt = guiView->getController().getManagedTeam();
  if (!managedTeamOpt.has_value()) return;

  // Refresh standings
  auto leagueOpt = guiView->getController().getLeagueById(
      managedTeamOpt->get().getLeagueId());
  if (leagueOpt.has_value())
  {
    const League& league = leagueOpt->get();
    const auto& leaderboard = league.getLeaderboard();
    cached_standings.assign(leaderboard.begin(), leaderboard.end());
    std::ranges::sort(cached_standings, [](const auto& a, const auto& b)
                      { return a.second > b.second; });
  }

  // Refresh top players
  cached_top_players =
      guiView->getController().getPlayersForTeam(managedTeamOpt->get().getId());
  const auto& stats_config = guiView->getController().getStatsConfig();
  std::ranges::sort(
      cached_top_players,
      [&stats_config](const std::reference_wrapper<const Player>& a,
                      const std::reference_wrapper<const Player>& b)
      {
        return a.get().getOverall(stats_config) >
               b.get().getOverall(stats_config);
      });
}
