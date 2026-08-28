// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "match_scene.h"

#include <imgui.h>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <optional>
#include <string_view>

#include "global/logger.h"
#include "gui/gui_view.h"
#include "gui/player_ui.h"
#include "gui/render/match_renderer_2d.h"
#include "model/role_utils.h"
#include "model/team.h"

namespace
{
std::optional<std::uint32_t> configuredMatchSeed()
{
  const char* configuredSeed = std::getenv("FM_MATCH_SEED");
  if (!configuredSeed || !*configuredSeed) return std::nullopt;

  const std::string_view seedText(configuredSeed);
  std::uint32_t seed = 0;
  const auto [end, error] =
      std::from_chars(seedText.data(), seedText.data() + seedText.size(), seed);
  if (error != std::errc{} || end != seedText.data() + seedText.size())
    return std::nullopt;
  return seed;
}

#ifdef DEBUG
const char* passIntentLabel(PassIntent intent)
{
  switch (intent)
  {
    case PassIntent::RECYCLE:
      return "Recycle possession";
    case PassIntent::PROGRESSIVE:
      return "Progressive pass";
    case PassIntent::THROUGH_BALL:
      return "Through ball";
    case PassIntent::CROSS:
      return "Cross";
    case PassIntent::CUTBACK:
      return "Cutback";
    case PassIntent::SWITCH_PLAY:
      return "Switch play";
    case PassIntent::PRESSURE_RELEASE:
      return "Escape pressure";
    case PassIntent::SET_PIECE:
      return "Set piece";
  }
  return "Unknown";
}

const char* teamPhaseLabel(TeamPhase phase)
{
  switch (phase)
  {
    case TeamPhase::STOPPAGE:
      return "Stoppage";
    case TeamPhase::SET_PIECE:
      return "Set piece";
    case TeamPhase::DEFENSIVE_BLOCK:
      return "Defensive block";
    case TeamPhase::DEFENSIVE_TRANSITION:
      return "Defensive transition";
    case TeamPhase::ATTACKING_TRANSITION:
      return "Attacking transition";
    case TeamPhase::POSSESSION:
      return "Possession";
    case TeamPhase::FINAL_THIRD:
      return "Final third";
  }
  return "Unknown";
}
#endif
}  // namespace

MatchScene::MatchScene(GUIView* guiView_ptr, uint16_t home_id, uint16_t away_id)
    : GUIScene(guiView_ptr), home_team_id(home_id), away_team_id(away_id)
{
}

SceneID MatchScene::getID() const { return SceneID::MATCH; }

void MatchScene::onEnter()
{
  const auto startedAt = std::chrono::steady_clock::now();
  auto home_opt = guiView->getController().getTeamById(home_team_id);
  auto away_opt = guiView->getController().getTeamById(away_team_id);

  if (home_opt && away_opt)
  {
    const Team& home_team = home_opt->get();
    const Team& away_team = away_opt->get();

    home_name = home_team.getName();
    away_name = away_team.getName();

    const auto seed = configuredMatchSeed();
    if (seed)
    {
      engine = std::make_unique<MatchEngine>(
          home_team.getLineup(), away_team.getLineup(), home_team.getStrategy(),
          away_team.getStrategy(), guiView->getController().getStatsConfig(),
          *seed);
    }
    else
    {
      engine = std::make_unique<MatchEngine>(
          home_team.getLineup(), away_team.getLineup(), home_team.getStrategy(),
          away_team.getStrategy(), guiView->getController().getStatsConfig());
    }
    matchRenderer = std::make_unique<MatchRenderer2D>();
  }
  scene_entry_milliseconds = std::chrono::duration<float, std::milli>(
                                 std::chrono::steady_clock::now() - startedAt)
                                 .count();
  Logger::info(std::format("Match scene initialized in {:.2f} ms",
                           scene_entry_milliseconds));
  if (scene_entry_milliseconds >=
      MatchSceneTuning::Performance::SLOW_SCENE_ENTRY_MILLISECONDS)
  {
    debug_status = std::format("Slow match initialization: {:.1f} ms",
                               scene_entry_milliseconds);
  }
}

void MatchScene::update(float deltaTime)
{
  if (engine && !match_finished && !is_paused)
  {
    const auto startedAt = std::chrono::steady_clock::now();
    engine->update(deltaTime * match_speed);
    last_update_milliseconds = std::chrono::duration<float, std::milli>(
                                   std::chrono::steady_clock::now() - startedAt)
                                   .count();
    maximum_update_milliseconds =
        std::max(maximum_update_milliseconds, last_update_milliseconds);
    if (last_update_milliseconds >=
        MatchSceneTuning::Performance::SLOW_UPDATE_MILLISECONDS)
    {
      ++slow_update_count;
    }
    if (engine->getState() == MatchState::FULL_TIME)
    {
      match_finished = true;
    }
  }
}

void MatchScene::handleEvent(const SDL_Event& event)
{
  if (event.type != SDL_EVENT_KEY_DOWN) return;
#ifdef DEBUG
  if (event.key.key == SDLK_F10)
  {
    show_ai_debug = !show_ai_debug;
  }
  else if (event.key.key == SDLK_F11)
  {
    exportDebugSnapshot();
  }
#endif
}

#ifdef DEBUG
void MatchScene::exportDebugSnapshot()
{
  if (!engine) return;
  const char* configuredPath = std::getenv("FM_MATCH_SNAPSHOT_PATH");
  const std::string path = configuredPath && *configuredPath
                               ? configuredPath
                               : "/tmp/football_management_match.json";
  debug_status = engine->writeDebugSnapshot(path)
                     ? "Snapshot: " + path
                     : "Could not write snapshot: " + path;
}
#endif

void MatchScene::render()
{
  ImGui::SetNextWindowPos(ImVec2{});
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
  ImGui::SetWindowFontScale(
      MatchSceneTuning::Scoreboard::EMPHASIZED_FONT_SCALE);
  ImGui::Text("%s %d - %d %s", home_name.c_str(), engine->getHomeScore(),
              engine->getAwayScore(), away_name.c_str());
  ImGui::SameLine(ImGui::GetWindowWidth() -
                  MatchSceneTuning::Scoreboard::TIME_RIGHT_MARGIN);
  const int minute = static_cast<int>(engine->getMatchTimeMinutes());
  const int second = static_cast<int>(
      (engine->getMatchTimeMinutes() - static_cast<float>(minute)) *
      MatchSceneTuning::Scoreboard::SECONDS_PER_MINUTE);
  ImGui::Text("Time: %02d:%02d", minute, second);
  ImGui::SetWindowFontScale(MatchSceneTuning::Scoreboard::NORMAL_FONT_SCALE);

  if (ImGui::Button(is_paused ? "Resume" : "Pause",
                    ImVec2(MatchSceneTuning::Controls::PAUSE_BUTTON_WIDTH,
                           MatchSceneTuning::Controls::BUTTON_HEIGHT)))
  {
    is_paused = !is_paused;
  }
  ImGui::SameLine();
  ImGui::SetNextItemWidth(MatchSceneTuning::Controls::SPEED_CONTROL_WIDTH);
  ImGui::SliderFloat("Speed", &match_speed,
                     MatchSceneTuning::Controls::MINIMUM_MATCH_SPEED,
                     MatchSceneTuning::Controls::MAXIMUM_MATCH_SPEED, "%.1fx");
  ImGui::SameLine();
  if (ImGui::Button(
          "Substitutions",
          ImVec2(MatchSceneTuning::Controls::SUBSTITUTION_BUTTON_WIDTH,
                 MatchSceneTuning::Controls::BUTTON_HEIGHT)))
  {
    show_substitutions = true;
    is_paused = true;  // Auto-pause when substituting
    selected_pitch_player = PlayerID{};
    selected_bench_player = PlayerID{};
  }
#ifdef DEBUG
  ImGui::SameLine();
  if (ImGui::Button("Export Debug (F11)",
                    ImVec2(MatchSceneTuning::Controls::DEBUG_BUTTON_WIDTH,
                           MatchSceneTuning::Controls::BUTTON_HEIGHT)))
  {
    exportDebugSnapshot();
  }
  ImGui::SameLine();
  if (ImGui::Button(show_ai_debug ? "Hide AI (F10)" : "Show AI (F10)",
                    ImVec2(MatchSceneTuning::Controls::AI_DEBUG_BUTTON_WIDTH,
                           MatchSceneTuning::Controls::BUTTON_HEIGHT)))
  {
    show_ai_debug = !show_ai_debug;
  }
#endif
  if (!debug_status.empty()) ImGui::TextUnformatted(debug_status.c_str());

  const MatchStats& match_stats = engine->getStats();
  ImGui::Text(
      "Shots %d-%d (on target %d-%d) | xG %.2f-%.2f | Possession "
      "%.0f%%-%.0f%% | Saves %d-%d",
      match_stats.homeShots, match_stats.awayShots, match_stats.homeOnTarget,
      match_stats.awayOnTarget, static_cast<double>(match_stats.homeShotXG),
      static_cast<double>(match_stats.awayShotXG),
      static_cast<double>(match_stats.homePossession),
      static_cast<double>(match_stats.awayPossession), match_stats.homeSaves,
      match_stats.awaySaves);
  ImGui::Text(
      "Passing %d/%d-%d/%d | progressive %d-%d | through balls %d-%d | "
      "switches %d-%d",
      match_stats.homePassesCompleted, match_stats.homePassesAttempted,
      match_stats.awayPassesCompleted, match_stats.awayPassesAttempted,
      match_stats.homeProgressivePasses, match_stats.awayProgressivePasses,
      match_stats.homeThroughBalls, match_stats.awayThroughBalls,
      match_stats.homeSwitchesOfPlay, match_stats.awaySwitchesOfPlay);
  ImGui::Text("Chance creation: crosses %d-%d | cutbacks %d-%d",
              match_stats.homeCrosses, match_stats.awayCrosses,
              match_stats.homeCutbacks, match_stats.awayCutbacks);
#ifdef DEBUG
  ImGui::Text(
      "Performance: entry %.2f ms | simulation %.2f ms (max %.2f ms) | "
      "slow frames %llu",
      static_cast<double>(scene_entry_milliseconds),
      static_cast<double>(last_update_milliseconds),
      static_cast<double>(maximum_update_milliseconds),
      static_cast<unsigned long long>(slow_update_count));
  if (show_ai_debug)
  {
    ImGui::Text("Team phase: %s / %s | transition %.1f s",
                teamPhaseLabel(engine->getHomePhase()),
                teamPhaseLabel(engine->getAwayPhase()),
                static_cast<double>(engine->getTransitionSecondsRemaining()));
  }
  const PassDecision& passDecision = engine->getLastPassDecision();
  if (passDecision.receiverId != 0)
  {
    ImGui::Text(
        "Last decision: %s | expected completion %.0f%% | utility %.2f",
        passIntentLabel(passDecision.intent),
        static_cast<double>(passDecision.completionProbability *
                            MatchSceneTuning::Scoreboard::PERCENT_SCALE),
        static_cast<double>(passDecision.utility));
  }
#endif

  ImGui::Separator();

  if (show_substitutions)
  {
    renderSubstitutionsModal();
  }

  // Pitch rendering is delegated to the match renderer, which draws into the
  // responsive viewport computed from the remaining window space. The pitch is
  // inset by a fixed apron so the stadium band and goal nets stay visible.
  const ImVec2 pitchOrigin = ImGui::GetCursorScreenPos();
  const float apron = MatchSceneTuning::Stadium::APRON_WIDTH;
  const float availableWidth =
      std::max(1.0f, ImGui::GetWindowWidth() - pitchOrigin.x);
  const float availableHeight =
      std::max(1.0f, ImGui::GetWindowHeight() - pitchOrigin.y);
  const MatchViewport viewport =
      computeMatchViewport(pitchOrigin.x + apron, pitchOrigin.y + apron,
                           std::max(1.0f, availableWidth - 2.0f * apron),
                           std::max(1.0f, availableHeight - 2.0f * apron));
  ImGui::Dummy(ImVec2(viewport.width, viewport.height));

  if (engine && matchRenderer)
  {
    MatchRenderOptions renderOptions;
#ifdef DEBUG
    renderOptions.showAiDebug = show_ai_debug;
#endif
    matchRenderer->render(buildMatchRenderSnapshot(*engine), renderOptions,
                          viewport);
  }

  ImGui::Separator();

  // Events Log
  ImGui::BeginChild("Events", ImVec2(0, MatchSceneTuning::Events::PANEL_HEIGHT),
                    true);
  const bool keepScrolledToLatest =
      ImGui::GetScrollY() >= ImGui::GetScrollMaxY();
  const auto& events = engine->getEvents();
  ImGuiListClipper eventClipper;
  eventClipper.Begin(static_cast<int>(events.size()));
  while (eventClipper.Step())
  {
    for (int index = eventClipper.DisplayStart; index < eventClipper.DisplayEnd;
         ++index)
    {
      const auto& event = events[static_cast<std::size_t>(index)];
      ImGui::Text("[%02d'] %s", static_cast<int>(event.timeMinute),
                  event.description.c_str());
    }
  }
  if (keepScrolledToLatest)
    ImGui::SetScrollHereY(MatchSceneTuning::Events::LATEST_SCROLL_RATIO);
  ImGui::EndChild();

  if (match_finished &&
      ImGui::Button("Finish Match",
                    ImVec2(MatchSceneTuning::Controls::FINISH_BUTTON_WIDTH,
                           MatchSceneTuning::Controls::FINISH_BUTTON_HEIGHT)))
  {
    if (guiView->getController().setMatchResult(
            guiView->getController().getCurrentDate(), home_team_id,
            away_team_id, static_cast<uint8_t>(engine->getHomeScore()),
            static_cast<uint8_t>(engine->getAwayScore())))
    {
      guiView->popScene();  // Pop MatchScene
      // Also advance day now that match is watched
      guiView->getController().advanceDay();
    }
    else
    {
      debug_status = "Could not record this match result.";
    }
  }

  ImGui::End();
}

void MatchScene::renderSubstitutionsModal()
{
  ImGui::OpenPopup(LOC("SUBSTITUTION_TITLE"));
  ImGui::SetNextWindowSize(
      ImVec2(MatchSceneTuning::Substitutions::MODAL_WIDTH,
             MatchSceneTuning::Substitutions::MODAL_HEIGHT),
      ImGuiCond_Appearing);
  if (ImGui::BeginPopupModal(LOC("SUBSTITUTION_TITLE"), &show_substitutions,
                             ImGuiWindowFlags_NoResize))
  {
    auto managed_opt = guiView->getController().getManagedTeam();
    if (!managed_opt)
    {
      ImGui::EndPopup();
      return;
    }
    Team& managed_team = managed_opt->get();
    Lineup& lineup = managed_team.getLineup();

    ImGui::TextWrapped("%s", LOC("SUBSTITUTION_HELP"));
    ImGui::Separator();

    if (ImGui::BeginTable(
            "SubstitutionChoices", 2,
            ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable))
    {
      ImGui::TableNextColumn();
      ImGui::Text("%s", LOC("SUBSTITUTION_ON_PITCH"));
      ImGui::BeginChild("PitchChoices", ImVec2(0.0f, 185.0f), true);

      if (lineup.getGoalkeeper())
      {
        bool selected =
            (selected_pitch_player == lineup.getGoalkeeper()->getId());
        std::string label =
            std::format("GK - {}##{}", lineup.getGoalkeeper()->getName(),
                        lineup.getGoalkeeper()->getId());
        if (ImGui::Selectable(label.c_str(), selected))
          selected_pitch_player =
              selected ? PlayerID{} : lineup.getGoalkeeper()->getId();
      }
      for (const auto& posPlayer : lineup.getOutfieldPlayers())
      {
        if (!posPlayer.player) continue;
        bool selected = (selected_pitch_player == posPlayer.player->getId());
        std::string label = std::format(
            "{} - {}##{}", RoleUtils::toString(posPlayer.player->getRole()),
            posPlayer.player->getName(), posPlayer.player->getId());
        if (ImGui::Selectable(label.c_str(), selected))
          selected_pitch_player =
              selected ? PlayerID{} : posPlayer.player->getId();
      }
      ImGui::EndChild();

      ImGui::TableNextColumn();
      ImGui::Text("%s", LOC("SUBSTITUTION_BENCH"));
      ImGui::BeginChild("BenchChoices", ImVec2(0.0f, 185.0f), true);

      for (const auto& res : lineup.getReserves())
      {
        if (!res) continue;
        bool selected = (selected_bench_player == res->getId());
        std::string label =
            std::format("{} - {}##{}", RoleUtils::toString(res->getRole()),
                        res->getName(), res->getId());
        if (ImGui::Selectable(label.c_str(), selected))
          selected_bench_player = selected ? PlayerID{} : res->getId();
      }
      ImGui::EndChild();
      ImGui::EndTable();
    }
    ImGui::Separator();

    const Player* outgoingPlayer = nullptr;
    const Player* incomingPlayer = nullptr;
    if (lineup.getGoalkeeper() &&
        lineup.getGoalkeeper()->getId() == selected_pitch_player)
      outgoingPlayer = lineup.getGoalkeeper();
    for (const auto& positioned : lineup.getOutfieldPlayers())
      if (positioned.player &&
          positioned.player->getId() == selected_pitch_player)
        outgoingPlayer = positioned.player;
    for (const Player* reserve : lineup.getReserves())
      if (reserve && reserve->getId() == selected_bench_player)
        incomingPlayer = reserve;

    if (ImGui::BeginTable("SubstitutionComparison", 2,
                          ImGuiTableFlags_BordersInnerV))
    {
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(LOC("SUBSTITUTION_ON_PITCH"));
      PlayerUI::detailPanel("OutgoingPlayer", outgoingPlayer,
                            guiView->getController().getStatsConfig(), nullptr,
                            250.0f);
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(LOC("SUBSTITUTION_BENCH"));
      PlayerUI::detailPanel("IncomingPlayer", incomingPlayer,
                            guiView->getController().getStatsConfig(),
                            outgoingPlayer, 250.0f);
      ImGui::EndTable();
    }

    ImGui::BeginDisabled(!outgoingPlayer || !incomingPlayer);
    if (ImGui::Button(
            LOC("SUBSTITUTION_CONFIRM"),
            ImVec2(MatchSceneTuning::Substitutions::ACTION_BUTTON_WIDTH,
                   MatchSceneTuning::Substitutions::ACTION_BUTTON_HEIGHT)) &&
        selected_pitch_player != PlayerID{} &&
        selected_bench_player != PlayerID{})
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
        selected_pitch_player = PlayerID{};
        selected_bench_player = PlayerID{};
      }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(
            LOC("SUBSTITUTION_CLOSE"),
            ImVec2(MatchSceneTuning::Substitutions::ACTION_BUTTON_WIDTH,
                   MatchSceneTuning::Substitutions::ACTION_BUTTON_HEIGHT)))
    {
      show_substitutions = false;
    }

    ImGui::EndPopup();
  }
}
