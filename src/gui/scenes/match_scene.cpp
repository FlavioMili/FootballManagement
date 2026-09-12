// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "match_scene.h"

#include <fmt/printf.h>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <optional>
#include <string_view>

#include "database/gamedata.h"
#include "global/logger.h"
#include "global/runtime_paths.h"
#include "gui/gui_view.h"
#include "gui/player_ui.h"
#include "gui/render/match_kit_colors.h"
#include "gui/render/match_renderer_2d.h"
#include "gui/render/match_renderer_3d.h"
#include "gui/scenes/lineup_scene.h"
#include "gui/scenes/match_report_scene.h"
#include "gui/view_models/competition_view.h"
#include "gui/view_models/match_clock.h"
#include "gui/widgets/format.h"
#include "gui/widgets/theme.h"
#include "gui/widgets/widgets.h"
#include "model/injury.h"
#include "model/match.h"
#include "model/role_utils.h"
#include "model/team.h"
#include "model/world_rng.h"

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

// Playback glue: the engine's highlight playback and default viewing speed
// are used when the engine provides them (they are optional so the view
// works with every engine time model).
template <typename Engine>
concept SupportsHighlights = requires(Engine& engine, float seconds) {
  engine.advancePlayback(seconds);
  engine.setPlaybackMode(engine.getPlaybackMode());
};
constexpr bool HIGHLIGHTS_AVAILABLE = SupportsHighlights<MatchEngine>;

template <typename Engine>
void setHighlightPlayback(Engine& engine, bool enabled)
{
  if constexpr (SupportsHighlights<Engine>)
  {
    using Mode = decltype(engine.getPlaybackMode());
    engine.setPlaybackMode(enabled ? Mode::HIGHLIGHTS : Mode::FULL_MATCH);
  }
}

/// Plays the rest of the match headless (fast path when the engine has one).
template <typename Engine>
void playToFullTime(Engine& engine)
{
  if constexpr (requires { engine.simulateToEnd(); })
  {
    engine.simulateToEnd();
  }
  else
  {
    while (engine.getState() != MatchState::FULL_TIME)
      engine.update(MatchSceneTuning::Controls::HEADLESS_STEP_SECONDS);
  }
}

/// Advances the live match by real time scaled by the viewer's speed, or
/// through the highlight playback when it is on.
template <typename Engine>
void advanceLive(Engine& engine, float wallSeconds, float speed,
                 bool highlights)
{
  if constexpr (SupportsHighlights<Engine>)
  {
    if (highlights)
    {
      engine.advancePlayback(wallSeconds);
      return;
    }
  }
  engine.update(wallSeconds * speed);
}

template <typename Tuning = MatchTuning>
constexpr float defaultPlaybackSpeed()
{
  if constexpr (requires { Tuning::Playback::DEFAULT_SPEED; })
    return Tuning::Playback::DEFAULT_SPEED;
  else
    return 1.0f;
}

// The last chosen presentation and substitution policy carry over to the
// next match this session.
MatchViewMode lastViewMode = MatchViewMode::PITCH_2D;
MatchCameraMode lastCameraMode = MatchCameraMode::BROADCAST;
float lastPlaybackSpeed = defaultPlaybackSpeed();
bool lastHighlightsOnly = false;

/// FM_MATCH_VIEW=3d|2d forces the initial view (profiling, screenshots).
std::optional<MatchViewMode> configuredMatchView()
{
  const char* configuredView = std::getenv("FM_MATCH_VIEW");
  if (!configuredView) return std::nullopt;
  const std::string_view view(configuredView);
  if (view == "3d") return MatchViewMode::BROADCAST_3D;
  if (view == "2d") return MatchViewMode::PITCH_2D;
  return std::nullopt;
}

float scaled(float value) { return value * Theme::scale(); }

ImVec4 eventColor(MatchEventType type)
{
  const Theme::Palette& palette = Theme::palette();
  switch (type)
  {
    case MatchEventType::GOAL:
      return palette.positive;
    case MatchEventType::OWN_GOAL:
    case MatchEventType::RED_CARD:
    case MatchEventType::SECOND_YELLOW:
    case MatchEventType::INJURY:
      return palette.negative;
    case MatchEventType::YELLOW_CARD:
    case MatchEventType::PENALTY:
    case MatchEventType::PENALTY_MISSED:
      return palette.warning;
    case MatchEventType::SUBSTITUTION:
    case MatchEventType::ADDED_TIME:
    case MatchEventType::KICK_OFF:
    case MatchEventType::HALF_TIME:
    case MatchEventType::SECOND_HALF:
    case MatchEventType::FULL_TIME:
      return palette.info;
    default:
      return palette.text;
  }
}

/// Chances, saves, goals, cards, injuries, changes and the phases of the
/// match; routine restarts (throw-ins, goal kicks, free kicks...) are not.
bool isKeyEvent(MatchEventType type)
{
  switch (type)
  {
    case MatchEventType::KICK_OFF:
    case MatchEventType::GOAL:
    case MatchEventType::OWN_GOAL:
    case MatchEventType::SHOT:
    case MatchEventType::SAVE:
    case MatchEventType::SHOT_BLOCKED:
    case MatchEventType::SHOT_OFF_TARGET:
    case MatchEventType::WOODWORK:
    case MatchEventType::YELLOW_CARD:
    case MatchEventType::SECOND_YELLOW:
    case MatchEventType::RED_CARD:
    case MatchEventType::INJURY:
    case MatchEventType::SUBSTITUTION:
    case MatchEventType::PENALTY:
    case MatchEventType::PENALTY_MISSED:
    case MatchEventType::ADDED_TIME:
    case MatchEventType::HALF_TIME:
    case MatchEventType::SECOND_HALF:
    case MatchEventType::FULL_TIME:
      return true;
    default:
      return false;
  }
}

/// Small glyph in front of an event: ball for goals, cards, arrows for
/// changes, a glove-like diamond for saves, rings for chances.
void drawEventIcon(ImDrawList* drawList, ImVec2 centre, float size,
                   MatchEventType type)
{
  const Theme::Palette& palette = Theme::palette();
  const float radius = size * 0.4f;
  const ImU32 yellow = IM_COL32(245, 200, 40, 255);
  const ImU32 red = IM_COL32(220, 50, 50, 255);
  const auto card = [&](float offset, ImU32 color)
  {
    drawList->AddRectFilled(
        ImVec2(centre.x - size * 0.22f + offset, centre.y - size * 0.4f),
        ImVec2(centre.x + size * 0.22f + offset, centre.y + size * 0.4f),
        color, 1.0f);
  };
  switch (type)
  {
    case MatchEventType::GOAL:
      drawList->AddCircleFilled(centre, radius, Theme::toU32(palette.text));
      drawList->AddCircleFilled(centre, radius * 0.35f,
                                Theme::toU32(palette.background));
      break;
    case MatchEventType::OWN_GOAL:
      drawList->AddCircleFilled(centre, radius, Theme::toU32(palette.negative));
      break;
    case MatchEventType::SHOT:
    case MatchEventType::SHOT_BLOCKED:
      drawList->AddCircle(centre, radius, Theme::toU32(palette.muted), 0,
                          1.5f);
      break;
    case MatchEventType::SHOT_OFF_TARGET:
    case MatchEventType::WOODWORK:
      drawList->AddCircle(centre, radius, Theme::toU32(palette.warning), 0,
                          1.5f);
      break;
    case MatchEventType::SAVE:
      drawList->AddQuadFilled(ImVec2(centre.x, centre.y - radius),
                              ImVec2(centre.x + radius, centre.y),
                              ImVec2(centre.x, centre.y + radius),
                              ImVec2(centre.x - radius, centre.y),
                              Theme::toU32(palette.info));
      break;
    case MatchEventType::YELLOW_CARD:
      card(0.0f, yellow);
      break;
    case MatchEventType::SECOND_YELLOW:
      card(-size * 0.12f, yellow);
      card(size * 0.12f, red);
      break;
    case MatchEventType::RED_CARD:
      card(0.0f, red);
      break;
    case MatchEventType::INJURY:
      drawList->AddRectFilled(ImVec2(centre.x - radius, centre.y - radius * 0.3f),
                              ImVec2(centre.x + radius, centre.y + radius * 0.3f),
                              Theme::toU32(palette.negative));
      drawList->AddRectFilled(ImVec2(centre.x - radius * 0.3f, centre.y - radius),
                              ImVec2(centre.x + radius * 0.3f, centre.y + radius),
                              Theme::toU32(palette.negative));
      break;
    case MatchEventType::SUBSTITUTION:
      drawList->AddTriangleFilled(
          ImVec2(centre.x - radius, centre.y + radius * 0.2f),
          ImVec2(centre.x - radius * 0.1f, centre.y + radius * 0.2f),
          ImVec2(centre.x - radius * 0.55f, centre.y - radius),
          Theme::toU32(palette.positive));
      drawList->AddTriangleFilled(
          ImVec2(centre.x + radius * 0.1f, centre.y - radius * 0.2f),
          ImVec2(centre.x + radius, centre.y - radius * 0.2f),
          ImVec2(centre.x + radius * 0.55f, centre.y + radius),
          Theme::toU32(palette.negative));
      break;
    case MatchEventType::PENALTY:
    case MatchEventType::PENALTY_MISSED:
      drawList->AddRect(ImVec2(centre.x - radius, centre.y - radius * 0.7f),
                        ImVec2(centre.x + radius, centre.y + radius * 0.7f),
                        Theme::toU32(palette.warning), 1.0f, 0, 1.5f);
      drawList->AddCircleFilled(centre, radius * 0.25f,
                                Theme::toU32(palette.warning));
      break;
    default:
      drawList->AddCircleFilled(centre, radius * 0.35f,
                                Theme::toU32(isKeyEvent(type) ? palette.info
                                                              : palette.faint));
      break;
  }
}

/// Home value, centred label and away value over a split bar (the match
/// report's comparison style).
void comparisonRow(const char* label, float home, float away,
                   const char* homeText, const char* awayText)
{
  const Theme::Palette& palette = Theme::palette();
  const float width = ImGui::GetContentRegionAvail().x;
  const float startX = ImGui::GetCursorPosX();
  ImGui::TextUnformatted(homeText);
  const float labelWidth = ImGui::CalcTextSize(label).x;
  ImGui::SameLine(startX + (width - labelWidth) * 0.5f);
  ImGui::TextColored(palette.muted, "%s", label);
  ImGui::SameLine(startX + width - ImGui::CalcTextSize(awayText).x);
  ImGui::TextUnformatted(awayText);

  const ImVec2 barStart = ImGui::GetCursorScreenPos();
  const float barHeight = scaled(4.0f);
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
  ImGui::Dummy(ImVec2(width, barHeight + scaled(2.0f)));
}

/// Comparison row of two counts.
void countRow(const char* label, int home, int away)
{
  std::array<char, 16> homeText{};
  std::array<char, 16> awayText{};
  std::snprintf(homeText.data(), homeText.size(), "%d", home);
  std::snprintf(awayText.data(), awayText.size(), "%d", away);
  comparisonRow(label, static_cast<float>(home), static_cast<float>(away),
                homeText.data(), awayText.data());
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
  GameController& controller = guiView->getController();
  auto home_opt = controller.getTeamById(home_team_id);
  auto away_opt = controller.getTeamById(away_team_id);

  if (home_opt && away_opt)
  {
    home_name = home_opt->get().getName();
    away_name = away_opt->get().getName();

    if (const auto managed = controller.getManagedTeam())
    {
      const TeamID managedId = managed->get().getId();
      if (managedId == home_team_id)
        managed_is_home = true;
      else if (managedId == away_team_id)
        managed_is_home = false;
    }
    if (const Game* game = controller.getGame())
    {
      if (const Match* fixture = game->getCalendar().findMatch(
              controller.getCurrentDate(), home_team_id, away_team_id))
        fixture_type = fixture->getMatchType();
    }

    renderer_2d = std::make_unique<MatchRenderer2D>();
    renderer_3d = std::make_unique<MatchRenderer3D>();
    if (const auto view = configuredMatchView()) lastViewMode = *view;
    view_mode = lastViewMode;
    camera_mode = lastCameraMode;

    refreshLineupProblems();
    if (!lineup_problems.empty() && controller.getAssistantFixesLineup())
      applyLineupFix();
    if (lineup_problems.empty()) startMatch();
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

void MatchScene::refreshLineupProblems()
{
  lineup_problems.clear();
  // Only a real fixture of the managed club has eligibility rules; tooling
  // matches (profiling, tests) play the selections as they are.
  if (!managed_is_home || !fixture_type) return;
  GameController& controller = guiView->getController();
  const auto data = controller.getGameData();
  const TeamID managedId = *managed_is_home ? home_team_id : away_team_id;
  const auto fixes = controller.previewLineupFix(managedId, *fixture_type);
  const auto nameOf = [&data](PlayerID id)
  {
    const auto player = data && id != 0 ? data->getPlayer(id) : std::nullopt;
    return player ? player->get().getName() : std::string();
  };
  for (const PlayerID id :
       controller.getIneligibleSelections(managedId, *fixture_type))
  {
    LineupProblem problem{id, {}, {}, {}};
    if (const auto fix = std::ranges::find_if(
            fixes, [id](const auto& change) { return change.first == id; });
        fix != fixes.end())
      problem.replacement = nameOf(fix->second);
    const auto player = data ? data->getPlayer(id) : std::nullopt;
    if (player)
    {
      const Player& selected = player->get();
      problem.name = selected.getName();
      const PlayerDynamics& dynamics = selected.getDynamics();
      if (!selected.isAvailable())
      {
        const int days = dynamics.injury_days;
        problem.reason = fmt::sprintf(
            Format::plural("MATCH_LINEUP_INJURED", days),
            LOC(InjuryModel::nameKey(dynamics.injury)), days);
      }
      else
      {
        const int matches =
            controller.getSuspensionMatches(id, *fixture_type);
        problem.reason = fmt::sprintf(
            Format::plural("MATCH_LINEUP_SUSPENDED", matches), matches);
      }
    }
    lineup_problems.push_back(std::move(problem));
  }
}

void MatchScene::applyLineupFix()
{
  const TeamID managedId = *managed_is_home ? home_team_id : away_team_id;
  std::string note;
  for (const LineupProblem& problem : lineup_problems)
  {
    if (!note.empty()) note += "  ·  ";
    note += problem.replacement.empty()
                ? fmt::sprintf(LOC("MATCH_LINEUP_LEFT_OUT"),
                               problem.name.c_str())
                : fmt::sprintf(LOC("MATCH_LINEUP_REPLACED"),
                               problem.replacement.c_str(),
                               problem.name.c_str());
  }
  guiView->getController().autoFixLineup(managedId, *fixture_type);
  refreshLineupProblems();
  if (lineup_problems.empty())
  {
    lineup_status.clear();
    pre_match_note = fmt::sprintf(LOC("MATCH_ASSISTANT_FIXED"), note.c_str());
  }
  else
  {
    lineup_status = LOC("MATCH_LINEUP_FIX_FAILED");
  }
}

void MatchScene::startMatch()
{
  GameController& controller = guiView->getController();
  auto home_opt = controller.getTeamById(home_team_id);
  auto away_opt = controller.getTeamById(away_team_id);
  if (!home_opt || !away_opt) return;
  const Team& home_team = home_opt->get();
  const Team& away_team = away_opt->get();

  // Deterministic per save and fixture (FM_MATCH_SEED overrides it), so a
  // reloaded save replays the same match until a manual change is made.
  std::uint32_t seed = 0;
  if (const auto configured = configuredMatchSeed())
  {
    seed = *configured;
  }
  else
  {
    const Match identity(home_team_id, away_team_id,
                         controller.getCurrentDate(),
                         fixture_type.value_or(MatchType::FRIENDLY));
    seed = static_cast<std::uint32_t>(
        mixHash(controller.getWorldSeed(), identity.getSeed()));
  }
  engine = std::make_unique<MatchEngine>(
      home_team.getLineup(), away_team.getLineup(), home_team.getStrategy(),
      away_team.getStrategy(), controller.getStatsConfig(), seed);
  // Fatigue carried over from recent matches and training.
  MatchdaySquad::carryCondition(*engine, home_team.getLineup());
  MatchdaySquad::carryCondition(*engine, away_team.getLineup());
  applySubstitutionPolicy();
  setPlaybackSpeed(lastPlaybackSpeed);
  setHighlightsOnly(lastHighlightsOnly);
  match_finished = false;
}

void MatchScene::setPlaybackSpeed(float speed)
{
  match_speed = speed;
  lastPlaybackSpeed = speed;
}

void MatchScene::setHighlightsOnly(bool enabled)
{
  highlights_only = enabled && HIGHLIGHTS_AVAILABLE;
  lastHighlightsOnly = highlights_only;
  if (engine) setHighlightPlayback(*engine, highlights_only);
}

void MatchScene::applySubstitutionPolicy()
{
  if (!engine) return;
  const bool homeAuto = !managed_is_home.value_or(false) ||
                        (*managed_is_home && assistant_substitutions);
  const bool awayAuto = managed_is_home.value_or(true) ||
                        (!*managed_is_home && assistant_substitutions);
  engine->setAutoSubstitutions(homeAuto, awayAuto);
}

std::string MatchScene::substitutionBlockReason() const
{
  if (!engine || !managed_is_home) return LOC("SUBSTITUTION_REFUSED_NO_TEAM");
  if (match_finished) return LOC("SUBSTITUTION_REFUSED_FINISHED");
  const bool home = *managed_is_home;
  if (engine->canSubstitute(home)) return {};
  if (engine->getSubstitutionsUsed(home) >=
      MatchTuning::Rules::MAX_SUBSTITUTIONS_PER_TEAM)
    return fmt::sprintf(LOC("SUBSTITUTION_REFUSED_LIMIT"),
                        MatchTuning::Rules::MAX_SUBSTITUTIONS_PER_TEAM);
  if (engine->getSubstitutionWindowsUsed(home) >=
      MatchTuning::Substitution::MAX_WINDOWS)
    return fmt::sprintf(LOC("SUBSTITUTION_REFUSED_WINDOWS"),
                        MatchTuning::Substitution::MAX_WINDOWS);
  return LOC("SUBSTITUTION_REFUSED_NOW");
}

bool MatchScene::substitute(PlayerID outgoing, PlayerID incoming)
{
  substitution_refused = true;
  if (std::string reason = substitutionBlockReason(); !reason.empty())
  {
    substitution_status = std::move(reason);
    return false;
  }
  const bool home = *managed_is_home;
  const auto team =
      guiView->getController().getTeamById(home ? home_team_id : away_team_id);
  const Player* incomingPlayer = nullptr;
  if (team)
  {
    for (const Player* reserve : team->get().getLineup().getReserves())
      if (reserve && reserve->getId() == incoming) incomingPlayer = reserve;
  }
  const Player* outgoingPlayer = nullptr;
  for (const MatchPlayer& player : engine->getPlayers())
    if (player.player && player.player->getId() == outgoing)
      outgoingPlayer = player.player;

  // The engine owns the rules (limit, windows, no re-entry); the saved
  // lineup is left untouched by in-match changes.
  if (!incomingPlayer || !outgoingPlayer ||
      !engine->substitutePlayer(outgoing, incomingPlayer))
  {
    substitution_status = LOC("SUBSTITUTION_REFUSED_PLAYER");
    return false;
  }
  substitution_refused = false;
  substitution_status =
      fmt::sprintf(LOC("SUBSTITUTION_DONE"), incomingPlayer->getName().c_str(),
                   outgoingPlayer->getName().c_str());
  selected_pitch_player = PlayerID{};
  selected_bench_player = PlayerID{};
  return true;
}

bool MatchScene::finishMatch()
{
  GameController& controller = guiView->getController();
  if (!engine || !controller.setMatchResult(controller.getCurrentDate(),
                                            home_team_id, away_team_id,
                                            *engine))
  {
    debug_status = LOC("MATCH_RESULT_FAILED");
    return false;
  }
  // The report replaces the match; the match day is over once it is in.
  const GameDateValue date = controller.getCurrentDate();
  guiView->navigateTo(std::make_unique<MatchReportScene>(
      guiView, date, home_team_id, away_team_id));
  controller.advanceDay();
  return true;
}

bool MatchScene::quickResult()
{
  if (!engine || match_finished) return false;
  playToFullTime(*engine);
  match_finished = true;
  return finishMatch();
}

std::string MatchScene::clockText() const
{
  switch (engine->getState())
  {
    case MatchState::HALF_TIME:
      return LOC("MATCH_CLOCK_HALF_TIME");
    case MatchState::FULL_TIME:
      return LOC("MATCH_CLOCK_FULL_TIME");
    default:
      return MatchClock::minuteLabel(engine->getMatchTimeMinutes(), engine->getPeriod(),
                         engine->isInAddedTime());
  }
}

ImU32 MatchScene::teamColor(bool home) const
{
  if (view_mode == MatchViewMode::BROADCAST_3D)
  {
    const MatchKits kits = chooseMatchKits(home_team_id, away_team_id);
    return home ? kits.home.shirt : kits.away.shirt;
  }
  return home ? MatchSceneTuning::Marker::HOME_COLOR
              : MatchSceneTuning::Marker::AWAY_COLOR;
}

void MatchScene::update(float deltaTime)
{
  frame_seconds = deltaTime;
  if (engine && !match_finished && !is_paused)
  {
    const auto startedAt = std::chrono::steady_clock::now();
    advanceLive(*engine, deltaTime, match_speed, highlights_only);
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

void MatchScene::setViewMode(MatchViewMode mode)
{
  view_mode = mode;
  lastViewMode = mode;
}

void MatchScene::setCameraMode(MatchCameraMode mode)
{
  camera_mode = mode;
  lastCameraMode = mode;
  setViewMode(MatchViewMode::BROADCAST_3D);
}

void MatchScene::handleEvent(const SDL_Event& event)
{
  if (event.type != SDL_EVENT_KEY_DOWN) return;
  if (!event.key.repeat && !ImGui::GetIO().WantTextInput)
  {
    switch (event.key.key)
    {
      case SDLK_V:
        setViewMode(view_mode == MatchViewMode::PITCH_2D
                        ? MatchViewMode::BROADCAST_3D
                        : MatchViewMode::PITCH_2D);
        return;
      case SDLK_1:
        setCameraMode(MatchCameraMode::BROADCAST);
        return;
      case SDLK_2:
        setCameraMode(MatchCameraMode::TACTICAL);
        return;
      case SDLK_3:
        setCameraMode(MatchCameraMode::END);
        return;
      case SDLK_4:
        setCameraMode(MatchCameraMode::PLAYER_FOLLOW);
        return;
      case SDLK_SPACE:
        if (engine && !match_finished) is_paused = !is_paused;
        return;
      default:
        break;
    }
  }
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
  const std::string path =
      configuredPath && *configuredPath
          ? configuredPath
          : RuntimePaths::capturePath("match.json").string();
  debug_status = engine->writeDebugSnapshot(path)
                     ? "Snapshot: " + path
                     : "Could not write snapshot: " + path;
}
#endif

void MatchScene::render()
{
  const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(mainViewport->WorkPos);
  ImGui::SetNextWindowSize(mainViewport->WorkSize);
  ImGui::PushStyleVar(
      ImGuiStyleVar_WindowPadding,
      ImVec2(scaled(Theme::Space::L), scaled(Theme::Space::M)));
  ImGui::Begin("MatchScene", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoScrollWithMouse);
  ImGui::PopStyleVar();

  if (!engine)
  {
    if (!lineup_problems.empty())
    {
      renderLineupGate();
    }
    else
    {
      UI::emptyState(LOC("MATCH_LOAD_ERROR"), "");
      if (ImGui::Button(LOC("NAV_BACK"))) guiView->popScene();
    }
    ImGui::End();
    return;
  }

  renderScoreboard();
  renderControls();
  if (view_mode == MatchViewMode::BROADCAST_3D) renderViewControls();
#ifdef DEBUG
  renderDebugLines();
#endif

  if (show_substitutions) renderSubstitutionsModal();

  // The pitch takes the free space; statistics and events sit beside it on
  // wide windows and below it (tabbed) on narrow ones.
  const ImGuiStyle& style = ImGui::GetStyle();
  const ImVec2 available = ImGui::GetContentRegionAvail();
  const bool sidePanel =
      available.x >=
      scaled(MatchSceneTuning::Panel::SIDE_PANEL_MIN_CONTENT_WIDTH);
  if (sidePanel)
  {
    const float panelWidth = std::clamp(
        std::floor(available.x *
                   MatchSceneTuning::Panel::SIDE_PANEL_WIDTH_RATIO),
        scaled(MatchSceneTuning::Panel::SIDE_PANEL_MIN_WIDTH),
        scaled(MatchSceneTuning::Panel::SIDE_PANEL_MAX_WIDTH));
    const float viewWidth =
        std::max(1.0f, available.x - panelWidth - style.ItemSpacing.x);
    const float viewHeight =
        std::max(scaled(MatchSceneTuning::View::MIN_HEIGHT), available.y);
    renderPitch(ImVec2(viewWidth, viewHeight));
    ImGui::SameLine();
    ImGui::BeginGroup();
    const float statisticsHeight = std::floor(
        (viewHeight - style.ItemSpacing.y) *
        MatchSceneTuning::Panel::STATISTICS_HEIGHT_RATIO);
    renderStatistics(ImVec2(panelWidth, statisticsHeight));
    renderEvents(ImVec2(
        panelWidth, viewHeight - statisticsHeight - style.ItemSpacing.y));
    ImGui::EndGroup();
  }
  else
  {
    const float panelHeight = scaled(MatchSceneTuning::Events::PANEL_HEIGHT);
    const float viewHeight =
        std::max(scaled(MatchSceneTuning::View::MIN_HEIGHT),
                 available.y - panelHeight - style.ItemSpacing.y);
    renderPitch(ImVec2(std::max(1.0f, available.x), viewHeight));
    const float halfWidth = std::floor((available.x - style.ItemSpacing.x) *
                                       0.5f);
    renderEvents(ImVec2(halfWidth, panelHeight));
    ImGui::SameLine();
    renderStatistics(
        ImVec2(available.x - halfWidth - style.ItemSpacing.x, panelHeight));
  }

  ImGui::End();
}

void MatchScene::renderLineupGate()
{
  const Theme::Palette& palette = Theme::palette();
  // A centred column in the upper part of the screen.
  const ImVec2 available = ImGui::GetContentRegionAvail();
  const float width = std::min(
      available.x, scaled(MatchSceneTuning::Panel::LINEUP_GATE_WIDTH));
  const float top =
      available.y * MatchSceneTuning::Panel::LINEUP_GATE_TOP_RATIO;
  ImGui::SetCursorPos(
      ImVec2(ImGui::GetCursorPosX() + (available.x - width) * 0.5f,
             ImGui::GetCursorPosY() + top));
  ImGui::BeginChild("##lineup_gate_column",
                    ImVec2(width, std::max(1.0f, available.y - top)),
                    ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);
  const std::string subtitle =
      std::format("{}  –  {}", home_name, away_name);
  UI::pageHeader(LOC("MATCH_LINEUP_BLOCKED_TITLE"), subtitle.c_str());
  UI::beginAutoHeightCard("##lineup_gate", nullptr);
  ImGui::PushTextWrapPos(0.0f);
  ImGui::TextUnformatted(LOC("MATCH_LINEUP_BLOCKED_BODY"));
  ImGui::PopTextWrapPos();
  ImGui::Dummy(ImVec2(0.0f, scaled(Theme::Space::S)));
  for (const LineupProblem& problem : lineup_problems)
  {
    UI::badge(problem.reason.c_str(), palette.negative);
    ImGui::SameLine();
    ImGui::TextUnformatted(problem.name.c_str());
    ImGui::SameLine();
    // What the assistant would do about it.
    if (problem.replacement.empty())
      ImGui::TextColored(palette.faint, "%s", LOC("MATCH_LINEUP_NO_SUGGESTION"));
    else
      ImGui::TextColored(
          palette.positive, "%s",
          fmt::sprintf(LOC("MATCH_LINEUP_SUGGESTION"),
                       problem.replacement.c_str())
              .c_str());
  }
  if (!lineup_status.empty())
  {
    ImGui::Dummy(ImVec2(0.0f, scaled(Theme::Space::XS)));
    ImGui::TextColored(palette.warning, "%s", lineup_status.c_str());
  }
  ImGui::Dummy(ImVec2(0.0f, scaled(Theme::Space::S)));
  if (UI::primaryButton(LOC("MATCH_LINEUP_AUTOFIX")))
  {
    applyLineupFix();
    if (lineup_problems.empty()) startMatch();
  }
  ImGui::SameLine();
  if (ImGui::Button(LOC("MATCH_LINEUP_EDIT")))
    guiView->navigateTo(std::make_unique<LineupScene>(guiView));
  ImGui::SameLine();
  if (ImGui::Button(LOC("NAV_BACK"))) guiView->popScene();
  ImGui::Dummy(ImVec2(0.0f, scaled(Theme::Space::XS)));
  if (bool fixes = guiView->getController().getAssistantFixesLineup();
      ImGui::Checkbox(LOC("MATCH_ASSISTANT_LINEUP"), &fixes))
    guiView->getController().setAssistantFixesLineup(fixes);
  UI::endCard();
  ImGui::EndChild();
}

void MatchScene::renderScoreboard()
{
  const Theme::Palette& palette = Theme::palette();
  UI::beginCard("##match_scoreboard", nullptr,
                ImVec2(0.0f, scaled(MatchSceneTuning::Scoreboard::HEIGHT)));
  const ImVec2 origin = ImGui::GetCursorPos();
  const float width = ImGui::GetContentRegionAvail().x;
  const float centreX = origin.x + width * 0.5f;

  std::array<char, 24> score{};
  std::snprintf(score.data(), score.size(), "%d  -  %d",
                engine->getHomeScore(), engine->getAwayScore());
  ImVec2 scoreSize;
  {
    Theme::ScopedText display(Theme::Text::DISPLAY);
    scoreSize = ImGui::CalcTextSize(score.data());
    ImGui::SetCursorPos(ImVec2(centreX - scoreSize.x * 0.5f, origin.y));
    ImGui::TextUnformatted(score.data());
  }

  // Clock pill and period under the score.
  const std::string clock = clockText();
  const MatchState state = engine->getState();
  const char* periodKey = state == MatchState::FULL_TIME ? "MATCH_PERIOD_FULL"
                          : state == MatchState::HALF_TIME
                              ? "MATCH_PERIOD_HALF"
                          : engine->getPeriod() >= 2 ? "MATCH_PERIOD_SECOND"
                                                     : "MATCH_PERIOD_FIRST";
  const int announced = engine->getAddedMinutes(engine->getPeriod());
  std::array<char, 16> addedText{};
  if (announced > 0 && state != MatchState::HALF_TIME &&
      state != MatchState::FULL_TIME)
    std::snprintf(addedText.data(), addedText.size(), "+%d", announced);
  {
    Theme::ScopedText caption(Theme::Text::CAPTION);
    const ImVec2 padding(scaled(6.0f), scaled(2.0f));
    const float clockWidth =
        ImGui::CalcTextSize(clock.c_str()).x + 2.0f * padding.x;
    const float periodWidth = ImGui::CalcTextSize(LOC(periodKey)).x;
    const float addedWidth =
        addedText[0] != '\0'
            ? ImGui::CalcTextSize(addedText.data()).x + 2.0f * padding.x +
                  ImGui::GetStyle().ItemSpacing.x
            : 0.0f;
    const float rowWidth = clockWidth + ImGui::GetStyle().ItemSpacing.x +
                           periodWidth + addedWidth;
    ImGui::SetCursorPos(ImVec2(centreX - rowWidth * 0.5f,
                               origin.y + scoreSize.y + scaled(2.0f)));
    const ImVec2 pillMin = ImGui::GetCursorScreenPos();
    const ImVec2 pillMax(pillMin.x + clockWidth,
                         pillMin.y + ImGui::GetTextLineHeight() +
                             2.0f * padding.y);
    ImGui::GetWindowDrawList()->AddRectFilled(
        pillMin, pillMax, Theme::toU32(palette.accent), scaled(3.0f));
    ImGui::GetWindowDrawList()->AddText(
        ImVec2(pillMin.x + padding.x, pillMin.y + padding.y),
        Theme::toU32(palette.on_accent), clock.c_str());
    ImGui::Dummy(ImVec2(clockWidth, pillMax.y - pillMin.y));
    ImGui::SameLine();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + padding.y);
    ImGui::TextColored(palette.muted, "%s", LOC(periodKey));
    if (addedText[0] != '\0')
    {
      ImGui::SameLine();
      UI::badge(addedText.data(), palette.warning);
    }
  }

  // Team names either side of the score, with their kit colour.
  {
    Theme::ScopedText heading(Theme::Text::HEADING);
    const float gap = scaled(MatchSceneTuning::Scoreboard::NAME_GAP);
    const float swatch = scaled(MatchSceneTuning::Scoreboard::KIT_SWATCH_SIZE);
    const float nameY =
        origin.y + (scoreSize.y - ImGui::GetTextLineHeight()) * 0.5f;
    const float homeWidth = ImGui::CalcTextSize(home_name.c_str()).x;
    const float homeX = std::max(
        origin.x + swatch + gap * 0.5f,
        centreX - scoreSize.x * 0.5f - gap - homeWidth);
    const float awayX = centreX + scoreSize.x * 0.5f + gap;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 windowPos = ImGui::GetWindowPos();
    const float swatchY = windowPos.y + nameY +
                          (ImGui::GetTextLineHeight() - swatch) * 0.5f -
                          ImGui::GetScrollY();
    const float homeSwatchX = windowPos.x + homeX - gap * 0.5f - swatch;
    drawList->AddRectFilled(ImVec2(homeSwatchX, swatchY),
                            ImVec2(homeSwatchX + swatch, swatchY + swatch),
                            teamColor(true), scaled(2.0f));
    const float awayWidth = ImGui::CalcTextSize(away_name.c_str()).x;
    const float awaySwatchX = windowPos.x + awayX + awayWidth + gap * 0.5f;
    drawList->AddRectFilled(ImVec2(awaySwatchX, swatchY),
                            ImVec2(awaySwatchX + swatch, swatchY + swatch),
                            teamColor(false), scaled(2.0f));
    ImGui::SetCursorPos(ImVec2(homeX, nameY));
    ImGui::TextUnformatted(home_name.c_str());
    ImGui::SetCursorPos(ImVec2(awayX, nameY));
    ImGui::TextUnformatted(away_name.c_str());
  }

  // Competition on the left edge.
  if (fixture_type)
  {
    ImGui::SetCursorPos(origin);
    UI::badge(LOC(CompetitionView::matchTypeKey(*fixture_type)),
              palette.info);
  }
  renderTimeline();
  UI::endCard();
}

void MatchScene::renderTimeline()
{
  // A strip along the bottom of the scoreboard: time played, half-time and
  // the key moments (goals above/below for home/away, cards).
  const Theme::Palette& palette = Theme::palette();
  const float half = MatchTuning::Timing::HALF_TIME_MINUTE;
  const float firstHalf =
      half + static_cast<float>(engine->getAddedMinutes(1));
  const float total = firstHalf + half +
                      static_cast<float>(engine->getAddedMinutes(2));
  const auto position = [&](float minute, int period)
  {
    return period >= 2 ? firstHalf + std::max(0.0f, minute - half)
                       : std::min(minute, firstHalf);
  };
  const float played =
      engine->getState() == MatchState::FULL_TIME
          ? total
          : position(engine->getMatchTimeMinutes(), engine->getPeriod());

  const ImVec2 windowPos = ImGui::GetWindowPos();
  const float padding = ImGui::GetStyle().WindowPadding.x;
  const float trackHeight = scaled(MatchSceneTuning::Scoreboard::TRACK_HEIGHT);
  const float markerSize =
      scaled(MatchSceneTuning::Scoreboard::TIMELINE_MARKER_SIZE);
  const float left = windowPos.x + padding;
  const float right = windowPos.x + ImGui::GetWindowWidth() - padding;
  const float trackY = windowPos.y + ImGui::GetWindowHeight() -
                       ImGui::GetStyle().WindowPadding.y - markerSize -
                       trackHeight * 0.5f;
  const auto xAt = [&](float value)
  { return left + (right - left) * std::clamp(value / total, 0.0f, 1.0f); };
  ImDrawList* drawList = ImGui::GetWindowDrawList();
  drawList->AddRectFilled(ImVec2(left, trackY - trackHeight * 0.5f),
                          ImVec2(right, trackY + trackHeight * 0.5f),
                          Theme::toU32(palette.raised), trackHeight);
  drawList->AddRectFilled(ImVec2(left, trackY - trackHeight * 0.5f),
                          ImVec2(xAt(played), trackY + trackHeight * 0.5f),
                          Theme::toU32(palette.accent, 0.7f), trackHeight);
  drawList->AddLine(ImVec2(xAt(firstHalf), trackY - markerSize),
                    ImVec2(xAt(firstHalf), trackY + markerSize),
                    Theme::toU32(palette.faint), 1.0f);
  for (const MatchEvent& event : engine->getEvents())
  {
    const bool goal = event.type == MatchEventType::GOAL ||
                      event.type == MatchEventType::OWN_GOAL;
    const bool card = event.type == MatchEventType::YELLOW_CARD ||
                      event.type == MatchEventType::SECOND_YELLOW ||
                      event.type == MatchEventType::RED_CARD;
    if (!goal && !card) continue;
    // An own goal counts for the other side.
    const bool home = event.type == MatchEventType::OWN_GOAL
                          ? !event.isHomeTeam
                          : event.isHomeTeam;
    const float x = xAt(position(event.timeMinute, event.period));
    const float y = home ? trackY - markerSize * 0.5f - trackHeight
                         : trackY + markerSize * 0.5f + trackHeight;
    if (goal)
    {
      drawList->AddCircleFilled(ImVec2(x, y), markerSize * 0.5f,
                                teamColor(home));
      drawList->AddCircle(ImVec2(x, y), markerSize * 0.5f,
                          Theme::toU32(palette.text), 0, 1.0f);
    }
    else
    {
      const ImU32 color = event.type == MatchEventType::YELLOW_CARD
                              ? IM_COL32(245, 200, 40, 255)
                              : IM_COL32(220, 50, 50, 255);
      drawList->AddRectFilled(
          ImVec2(x - markerSize * 0.3f, y - markerSize * 0.45f),
          ImVec2(x + markerSize * 0.3f, y + markerSize * 0.45f), color, 1.0f);
    }
  }
}

void MatchScene::renderControls()
{
  const Theme::Palette& palette = Theme::palette();
  const ImVec2 pauseSize(
      scaled(MatchSceneTuning::Controls::PAUSE_BUTTON_WIDTH), 0.0f);
  if (match_finished)
  {
    if (UI::primaryButton(LOC("MATCH_FINISH"), pauseSize)) finishMatch();
  }
  else
  {
    if (is_paused ? UI::primaryButton(LOC("MATCH_RESUME"), pauseSize)
                  : ImGui::Button(LOC("MATCH_PAUSE"), pauseSize))
      is_paused = !is_paused;
    UI::sameLineIfFits(UI::buttonWidth(LOC("MATCH_QUICK_RESULT")));
    if (ImGui::Button(LOC("MATCH_QUICK_RESULT"))) quickResult();
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("%s", LOC("MATCH_QUICK_RESULT_HINT"));
  }

  // Segmented speed control: real time up to 16x, or highlights only.
  const auto& speeds = MatchSceneTuning::Controls::SPEED_STEPS;
  std::array<std::array<char, 16>, speeds.size()> speedLabels{};
  const ImGuiStyle& style = ImGui::GetStyle();
  const float gap = scaled(MatchSceneTuning::Controls::SPEED_BUTTON_GAP);
  float segmentWidth =
      ImGui::CalcTextSize(LOC("MATCH_SPEED")).x + style.ItemSpacing.x +
      (HIGHLIGHTS_AVAILABLE ? ImGui::CalcTextSize(LOC("MATCH_HIGHLIGHTS")).x +
                                  2.0f * style.FramePadding.x + gap
                            : 0.0f);
  for (std::size_t index = 0; index < speeds.size(); ++index)
  {
    std::snprintf(speedLabels[index].data(), speedLabels[index].size(), "%gx",
                  static_cast<double>(speeds[index]));
    segmentWidth += ImGui::CalcTextSize(speedLabels[index].data()).x +
                    2.0f * style.FramePadding.x + gap;
  }
  UI::sameLineIfFits(segmentWidth);
  ImGui::AlignTextToFramePadding();
  ImGui::TextColored(palette.muted, "%s", LOC("MATCH_SPEED"));
  ImGui::SameLine();
  ImGui::PushID("match_speed");
  for (std::size_t index = 0; index < speeds.size(); ++index)
  {
    if (index > 0) ImGui::SameLine(0.0f, gap);
    const bool active = !highlights_only && match_speed == speeds[index];
    if (active ? UI::primaryButton(speedLabels[index].data())
               : ImGui::Button(speedLabels[index].data()))
    {
      setHighlightsOnly(false);
      setPlaybackSpeed(speeds[index]);
    }
  }
  if (HIGHLIGHTS_AVAILABLE)
  {
    ImGui::SameLine(0.0f, gap);
    if (highlights_only ? UI::primaryButton(LOC("MATCH_HIGHLIGHTS"))
                        : ImGui::Button(LOC("MATCH_HIGHLIGHTS")))
      setHighlightsOnly(!highlights_only);
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("%s", LOC("MATCH_HIGHLIGHTS_HINT"));
  }
  ImGui::PopID();

  const int used =
      managed_is_home ? engine->getSubstitutionsUsed(*managed_is_home) : 0;
  // Fixed buffer: the HUD formats no heap strings per frame.
  std::array<char, 96> subsLabel{};
  std::snprintf(subsLabel.data(), subsLabel.size(),
                "%s %d/%d###match_substitutions", LOC("SUBSTITUTION_TITLE"),
                used, MatchTuning::Rules::MAX_SUBSTITUTIONS_PER_TEAM);
  const float subsWidth =
      scaled(MatchSceneTuning::Controls::SUBSTITUTION_BUTTON_WIDTH);
  UI::sameLineIfFits(subsWidth);
  ImGui::BeginDisabled(!managed_is_home || match_finished);
  if (ImGui::Button(subsLabel.data(), ImVec2(subsWidth, 0.0f)))
  {
    show_substitutions = true;
    is_paused = true;  // Auto-pause when substituting
    selected_pitch_player = PlayerID{};
    selected_bench_player = PlayerID{};
    substitution_status.clear();
  }
  ImGui::EndDisabled();

  const float viewWidth = scaled(MatchSceneTuning::Controls::VIEW_BUTTON_WIDTH);
  UI::sameLineIfFits(viewWidth);
  if (ImGui::Button(view_mode == MatchViewMode::PITCH_2D ? LOC("MATCH_VIEW_3D")
                                                         : LOC("MATCH_VIEW_2D"),
                    ImVec2(viewWidth, 0.0f)))
  {
    setViewMode(view_mode == MatchViewMode::PITCH_2D
                    ? MatchViewMode::BROADCAST_3D
                    : MatchViewMode::PITCH_2D);
  }

  // What the assistant manager may decide on the manager's behalf.
  UI::sameLineIfFits(UI::buttonWidth(LOC("MATCH_ASSISTANT")));
  ImGui::BeginDisabled(!managed_is_home);
  if (ImGui::Button(LOC("MATCH_ASSISTANT"))) ImGui::OpenPopup("##assistant");
  ImGui::EndDisabled();
  if (ImGui::BeginPopup("##assistant"))
  {
    if (ImGui::Checkbox(LOC("MATCH_ASSISTANT_SUBS"), &assistant_substitutions))
      applySubstitutionPolicy();
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("%s", LOC("MATCH_ASSISTANT_SUBS_HINT"));
    if (bool fixes = guiView->getController().getAssistantFixesLineup();
        ImGui::Checkbox(LOC("MATCH_ASSISTANT_LINEUP"), &fixes))
      guiView->getController().setAssistantFixesLineup(fixes);
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("%s", LOC("MATCH_ASSISTANT_LINEUP_HINT"));
    ImGui::EndPopup();
  }

  // Latest message: debug, then substitutions, then pre-match changes.
  const bool refused = substitution_refused && !substitution_status.empty();
  const std::string& status = !debug_status.empty()        ? debug_status
                              : !substitution_status.empty() ? substitution_status
                                                             : pre_match_note;
  if (!status.empty())
  {
    UI::sameLineIfFits(ImGui::CalcTextSize(status.c_str()).x);
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(refused || !debug_status.empty() ? palette.warning
                       : &status == &pre_match_note     ? palette.info
                                                        : palette.positive,
                       "%s", status.c_str());
  }
}

void MatchScene::renderViewControls()
{
  ImGui::AlignTextToFramePadding();
  ImGui::TextColored(Theme::palette().muted, "%s", LOC("MATCH_CAMERA"));
  const auto cameraButton = [this](const char* label, MatchCameraMode mode)
  {
    UI::sameLineIfFits(ImGui::CalcTextSize(label).x +
                       ImGui::GetFrameHeight() * 1.5f);
    if (ImGui::RadioButton(label, camera_mode == mode)) setCameraMode(mode);
  };
  cameraButton(LOC("MATCH_CAMERA_BROADCAST"), MatchCameraMode::BROADCAST);
  cameraButton(LOC("MATCH_CAMERA_TACTICAL"), MatchCameraMode::TACTICAL);
  cameraButton(LOC("MATCH_CAMERA_END"), MatchCameraMode::END);
  cameraButton(LOC("MATCH_CAMERA_FOLLOW"), MatchCameraMode::PLAYER_FOLLOW);
  UI::sameLineIfFits(ImGui::CalcTextSize(LOC("MATCH_SHOW_NAMES")).x +
                     ImGui::GetFrameHeight() * 1.5f);
  ImGui::Checkbox(LOC("MATCH_SHOW_NAMES"), &show_player_names);
  UI::sameLineIfFits(ImGui::CalcTextSize(LOC("MATCH_ZOOM_HINT")).x);
  ImGui::AlignTextToFramePadding();
  ImGui::TextDisabled("%s", LOC("MATCH_ZOOM_HINT"));
}

#ifdef DEBUG
void MatchScene::renderDebugLines()
{
  ImGui::Text(
      "Performance: entry %.2f ms | simulation %.2f ms (max %.2f ms) | "
      "slow frames %llu | %s view %.2f ms (avg %.2f ms)",
      static_cast<double>(scene_entry_milliseconds),
      static_cast<double>(last_update_milliseconds),
      static_cast<double>(maximum_update_milliseconds),
      static_cast<unsigned long long>(slow_update_count),
      view_mode == MatchViewMode::PITCH_2D ? "2D" : "3D",
      static_cast<double>(last_render_milliseconds),
      static_cast<double>(average_render_milliseconds));
  if (ImGui::SmallButton("Export Debug (F11)")) exportDebugSnapshot();
  ImGui::SameLine();
  if (ImGui::SmallButton(show_ai_debug ? "Hide AI (F10)" : "Show AI (F10)"))
    show_ai_debug = !show_ai_debug;
  if (show_ai_debug)
  {
    ImGui::SameLine();
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
}
#endif

void MatchScene::renderPitch(ImVec2 size)
{
  // Both views consume the same snapshot through the renderer interface.
  const ImVec2 viewOrigin = ImGui::GetCursorScreenPos();
  MatchViewport viewport;
  IMatchRenderer* renderer = nullptr;
  MatchRenderOptions renderOptions;
  renderOptions.frameSeconds = frame_seconds;
  if (view_mode == MatchViewMode::BROADCAST_3D)
  {
    viewport = {viewOrigin.x, viewOrigin.y, size.x, size.y};
    ImGui::InvisibleButton("MatchView3D", size);
    // The wheel zooms the camera instead of scrolling the scene.
    if (ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY))
      pending_zoom_steps += ImGui::GetIO().MouseWheel;
    renderer = renderer_3d.get();
    renderOptions.cameraMode = camera_mode;
    renderOptions.showPlayerNames = show_player_names;
    renderOptions.zoomSteps = pending_zoom_steps;
    // The HUD scoreboard above the view replaces the in-view score bug.
    pending_zoom_steps = 0.0f;
  }
  else
  {
    // The 2D pitch is inset by an apron so the stadium band and goal nets
    // stay visible.
    const float apron = scaled(MatchSceneTuning::Stadium::APRON_WIDTH);
    viewport = computeMatchViewport(viewOrigin.x + apron, viewOrigin.y + apron,
                                    std::max(1.0f, size.x - 2.0f * apron),
                                    std::max(1.0f, size.y - 2.0f * apron));
    // Centred in the view area when the aspect ratios differ.
    viewport.x = viewOrigin.x + std::max(apron, (size.x - viewport.width) * 0.5f);
    viewport.y =
        viewOrigin.y + std::max(apron, (size.y - viewport.height) * 0.5f);
    ImGui::Dummy(size);
    renderer = renderer_2d.get();
  }

  if (renderer)
  {
#ifdef DEBUG
    renderOptions.showAiDebug = show_ai_debug;
#endif
    const auto renderStartedAt = std::chrono::steady_clock::now();
    renderer->render(buildMatchRenderSnapshot(*engine), renderOptions,
                     viewport);
    last_render_milliseconds =
        std::chrono::duration<float, std::milli>(
            std::chrono::steady_clock::now() - renderStartedAt)
            .count();
    average_render_milliseconds +=
        (last_render_milliseconds - average_render_milliseconds) *
        MatchSceneTuning::View::RENDER_TIME_SMOOTHING;
  }
}

void MatchScene::renderStatistics(ImVec2 size)
{
  UI::beginCard("##match_statistics", LOC("REPORT_STATS"), size, true);
  const MatchStats& stats = engine->getStats();
  std::array<char, 16> homeText{};
  std::array<char, 16> awayText{};

  std::snprintf(homeText.data(), homeText.size(), "%.0f%%",
                static_cast<double>(stats.homePossession));
  std::snprintf(awayText.data(), awayText.size(), "%.0f%%",
                static_cast<double>(stats.awayPossession));
  comparisonRow(LOC("REPORT_POSSESSION"), stats.homePossession,
                stats.awayPossession, homeText.data(), awayText.data());
  countRow(LOC("REPORT_SHOTS"), stats.homeShots, stats.awayShots);
  countRow(LOC("REPORT_ON_TARGET"), stats.homeOnTarget, stats.awayOnTarget);
  std::snprintf(homeText.data(), homeText.size(), "%.2f",
                static_cast<double>(stats.homeShotXG));
  std::snprintf(awayText.data(), awayText.size(), "%.2f",
                static_cast<double>(stats.awayShotXG));
  comparisonRow(LOC("REPORT_XG"), stats.homeShotXG, stats.awayShotXG,
                homeText.data(), awayText.data());
  std::snprintf(homeText.data(), homeText.size(), "%d/%d",
                stats.homePassesCompleted, stats.homePassesAttempted);
  std::snprintf(awayText.data(), awayText.size(), "%d/%d",
                stats.awayPassesCompleted, stats.awayPassesAttempted);
  comparisonRow(LOC("REPORT_PASSES"),
                static_cast<float>(stats.homePassesCompleted),
                static_cast<float>(stats.awayPassesCompleted), homeText.data(),
                awayText.data());
  countRow(LOC("REPORT_CORNERS"), stats.homeCorners, stats.awayCorners);
  countRow(LOC("REPORT_FOULS"), stats.homeFouls, stats.awayFouls);
  countRow(LOC("REPORT_OFFSIDES"), stats.homeOffsides, stats.awayOffsides);
  countRow(LOC("REPORT_SAVES"), stats.homeSaves, stats.awaySaves);
  std::snprintf(homeText.data(), homeText.size(), "%d / %d",
                stats.homeYellowCards, stats.homeRedCards);
  std::snprintf(awayText.data(), awayText.size(), "%d / %d",
                stats.awayYellowCards, stats.awayRedCards);
  comparisonRow(
      LOC("REPORT_CARDS"),
      static_cast<float>(stats.homeYellowCards + stats.homeRedCards),
      static_cast<float>(stats.awayYellowCards + stats.awayRedCards),
      homeText.data(), awayText.data());
  UI::endCard();
}

void MatchScene::refreshVisibleEvents()
{
  const auto& events = engine->getEvents();
  if (indexed_show_all != show_all_events || indexed_events > events.size())
  {
    visible_events.clear();
    indexed_events = 0;
    indexed_show_all = show_all_events;
  }
  for (; indexed_events < events.size(); ++indexed_events)
  {
    if (show_all_events || isKeyEvent(events[indexed_events].type))
      visible_events.push_back(indexed_events);
  }
}

void MatchScene::renderEvents(ImVec2 size)
{
  const Theme::Palette& palette = Theme::palette();
  UI::beginCard("##match_events", LOC("MATCH_EVENTS"), size);
  ImGui::Checkbox(LOC("MATCH_EVENTS_SHOW_ALL"), &show_all_events);
  refreshVisibleEvents();
  ImGui::BeginChild("##match_event_list", ImVec2(0.0f, 0.0f));
  const bool keepScrolledToLatest =
      ImGui::GetScrollY() >= ImGui::GetScrollMaxY();
  const float minuteWidth =
      scaled(MatchSceneTuning::Panel::MINUTE_COLUMN_WIDTH);
  const float iconSize = scaled(MatchSceneTuning::Panel::EVENT_ICON_SIZE);
  const auto& events = engine->getEvents();
  ImGuiListClipper eventClipper;
  eventClipper.Begin(static_cast<int>(visible_events.size()));
  while (eventClipper.Step())
  {
    for (int row = eventClipper.DisplayStart; row < eventClipper.DisplayEnd;
         ++row)
    {
      const MatchEvent& event =
          events[visible_events[static_cast<std::size_t>(row)]];
      const float startX = ImGui::GetCursorPosX();
      const std::string minute = MatchClock::minuteLabel(
          event.timeMinute, event.period, event.addedMinute > 0.0f);
      ImGui::TextColored(palette.muted, "%s", minute.c_str());
      ImGui::SameLine(startX + minuteWidth);
      const ImVec2 iconOrigin = ImGui::GetCursorScreenPos();
      drawEventIcon(ImGui::GetWindowDrawList(),
                    ImVec2(iconOrigin.x + iconSize * 0.5f,
                           iconOrigin.y + ImGui::GetTextLineHeight() * 0.5f),
                    iconSize, event.type);
      ImGui::Dummy(ImVec2(iconSize, ImGui::GetTextLineHeight()));
      ImGui::SameLine();
      UI::textFitted(event.description, ImGui::GetContentRegionAvail().x,
                     eventColor(event.type));
    }
  }
  if (keepScrolledToLatest)
    ImGui::SetScrollHereY(MatchSceneTuning::Events::LATEST_SCROLL_RATIO);
  ImGui::EndChild();
  UI::endCard();
}

void MatchScene::renderSubstitutionsModal()
{
  const std::string title =
      std::string(LOC("SUBSTITUTION_TITLE")) + "###match_substitutions_modal";
  ImGui::OpenPopup(title.c_str());
  const ImVec2 display = ImGui::GetMainViewport()->WorkSize;
  const float fraction = MatchSceneTuning::Substitutions::MODAL_VIEWPORT_FRACTION;
  ImGui::SetNextWindowSize(
      ImVec2(std::min(scaled(MatchSceneTuning::Substitutions::MODAL_WIDTH),
                      display.x * fraction),
             std::min(scaled(MatchSceneTuning::Substitutions::MODAL_HEIGHT),
                      display.y * fraction)),
      ImGuiCond_Appearing);
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                          ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  if (!ImGui::BeginPopupModal(title.c_str(), &show_substitutions,
                              ImGuiWindowFlags_NoResize))
    return;

  const Theme::Palette& palette = Theme::palette();
  const auto managedTeam =
      managed_is_home ? guiView->getController().getTeamById(
                            *managed_is_home ? home_team_id : away_team_id)
                      : std::nullopt;
  if (!managedTeam)
  {
    ImGui::EndPopup();
    return;
  }
  const bool home = *managed_is_home;
  const Lineup& lineup = managedTeam->get().getLineup();

  ImGui::TextWrapped("%s", LOC("SUBSTITUTION_HELP"));
  ImGui::TextColored(
      palette.muted, "%s",
      fmt::sprintf(LOC("SUBSTITUTION_USAGE"), engine->getSubstitutionsUsed(home),
                   MatchTuning::Rules::MAX_SUBSTITUTIONS_PER_TEAM,
                   engine->getSubstitutionWindowsUsed(home),
                   MatchTuning::Substitution::MAX_WINDOWS)
          .c_str());
  const std::string blockReason = substitutionBlockReason();
  if (!blockReason.empty())
    ImGui::TextColored(palette.warning, "%s", blockReason.c_str());
  ImGui::Separator();

  // Who is on the pitch comes from the engine (injuries, red cards and
  // earlier changes included); the bench is the matchday squad's reserves.
  const Player* outgoingPlayer = nullptr;
  const Player* incomingPlayer = nullptr;
  const float listHeight = scaled(MatchSceneTuning::Substitutions::LIST_HEIGHT);
  if (ImGui::BeginTable("SubstitutionChoices", 2,
                        ImGuiTableFlags_BordersInnerV |
                            ImGuiTableFlags_Resizable))
  {
    ImGui::TableNextColumn();
    UI::sectionLabel(LOC("SUBSTITUTION_ON_PITCH"));
    ImGui::BeginChild("PitchChoices", ImVec2(0.0f, listHeight),
                      ImGuiChildFlags_Borders);
    for (const MatchPlayer& onPitch : engine->getPlayers())
    {
      if (!onPitch.player || onPitch.isHomeTeam != home || !onPitch.onPitch)
        continue;
      const Player* player = onPitch.player;
      const bool selected = selected_pitch_player == player->getId();
      if (selected) outgoingPlayer = player;
      const std::string label = std::format(
          "{} - {}  ({:.0f}%){}##{}", RoleUtils::toString(player->getRole()),
          player->getName(),
          onPitch.stamina * MatchSceneTuning::Scoreboard::PERCENT_SCALE,
          onPitch.isInjured ? "  +" : "", player->getId());
      if (ImGui::Selectable(label.c_str(), selected))
        selected_pitch_player = selected ? PlayerID{} : player->getId();
    }
    ImGui::EndChild();

    ImGui::TableNextColumn();
    UI::sectionLabel(LOC("SUBSTITUTION_BENCH"));
    ImGui::BeginChild("BenchChoices", ImVec2(0.0f, listHeight),
                      ImGuiChildFlags_Borders);
    for (const Player* reserve : lineup.getReserves())
    {
      if (!reserve) continue;
      // Players who already took part (or were replaced) cannot come on.
      const bool used = engine->findPlayerStats(reserve->getId()) != nullptr;
      const bool selected = !used && selected_bench_player == reserve->getId();
      if (selected) incomingPlayer = reserve;
      const std::string label = std::format(
          "{} - {}{}##{}", RoleUtils::toString(reserve->getRole()),
          reserve->getName(),
          used ? std::string("  · ") + LOC("SUBSTITUTION_ALREADY_PLAYED")
               : std::string(),
          reserve->getId());
      if (ImGui::Selectable(label.c_str(), selected,
                            used ? ImGuiSelectableFlags_Disabled : 0))
        selected_bench_player = selected ? PlayerID{} : reserve->getId();
    }
    ImGui::EndChild();
    ImGui::EndTable();
  }
  ImGui::Separator();

  const float detailHeight =
      scaled(MatchSceneTuning::Substitutions::DETAIL_HEIGHT);
  if (ImGui::BeginTable("SubstitutionComparison", 2,
                        ImGuiTableFlags_BordersInnerV))
  {
    ImGui::TableNextColumn();
    UI::sectionLabel(LOC("SUBSTITUTION_ON_PITCH"));
    PlayerUI::detailPanel("OutgoingPlayer", outgoingPlayer,
                          guiView->getController().getStatsConfig(), nullptr,
                          detailHeight);
    ImGui::TableNextColumn();
    UI::sectionLabel(LOC("SUBSTITUTION_BENCH"));
    PlayerUI::detailPanel("IncomingPlayer", incomingPlayer,
                          guiView->getController().getStatsConfig(),
                          outgoingPlayer, detailHeight);
    ImGui::EndTable();
  }

  if (!substitution_status.empty())
    ImGui::TextColored(
        substitution_refused ? palette.negative : palette.positive, "%s",
        substitution_status.c_str());

  const ImVec2 actionSize(
      scaled(MatchSceneTuning::Substitutions::ACTION_BUTTON_WIDTH), 0.0f);
  ImGui::BeginDisabled(!outgoingPlayer || !incomingPlayer ||
                       !blockReason.empty());
  if (UI::primaryButton(LOC("SUBSTITUTION_CONFIRM"), actionSize))
    substitute(outgoingPlayer->getId(), incomingPlayer->getId());
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (ImGui::Button(LOC("SUBSTITUTION_CLOSE"), actionSize))
    show_substitutions = false;

  ImGui::EndPopup();
}
