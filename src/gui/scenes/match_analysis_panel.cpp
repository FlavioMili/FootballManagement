// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "gui/scenes/match_analysis_panel.h"

#include <fmt/printf.h>
#include <imgui.h>

#include <algorithm>
#include <format>

#include "controller/game_controller.h"
#include "database/gamedata.h"
#include "global/language_manager.h"
#include "gui/scenes/onboarding_overlay.h"
#include "gui/widgets/theme.h"
#include "gui/widgets/widgets.h"
#include "model/match_engine.h"

namespace
{
constexpr const char* POPUP_ID = "##match_analysis";
constexpr float DIALOG_WIDTH = 620.0f;
constexpr float VIEWPORT_WIDTH_SHARE = 0.94f;
constexpr float VIEWPORT_HEIGHT_SHARE = 0.9f;
/** The break is over for the auto-opening after this second-half minute. */
constexpr float HALF_TIME_OFFER_UNTIL = 50.0f;

float scaled(float value) { return value * Theme::scale(); }

std::string percentText(float share)
{
  return std::format("{:.0f}%", share * 100.0f);
}

const char* noteKey(PlayerNote::Kind kind)
{
  switch (kind)
  {
    case PlayerNote::Kind::Standout:
      return "ANALYSIS_NOTE_STANDOUT";
    case PlayerNote::Kind::Struggling:
      return "ANALYSIS_NOTE_STRUGGLING";
    case PlayerNote::Kind::Tired:
      break;
  }
  return "ANALYSIS_NOTE_TIRED";
}
}  // namespace

void MatchAnalysisPanel::renderForMatch(GameController& controller,
                                        const MatchEngine& engine,
                                        TeamID home_id, TeamID away_id,
                                        bool other_dialog_open)
{
  const auto team = controller.getManagedTeam();
  const TeamID managed = team ? team->get().getId() : 0;
  if (team && (managed == home_id || managed == away_id) && !visible &&
      !open_requested && !other_dialog_open)
  {
    const MatchState state = engine.getState();
    const bool break_time =
        state == MatchState::HALF_TIME ||
        (engine.getPeriod() == 2 &&
         engine.getMatchTimeMinutes() < HALF_TIME_OFFER_UNTIL);
    if (!full_offered && state == MatchState::FULL_TIME)
    {
      full_offered = true;
      half_offered = true;
      openNow(controller, engine, home_id, away_id);
    }
    else if (!half_offered && break_time)
    {
      half_offered = true;
      openNow(controller, engine, home_id, away_id);
    }
  }
  render();
}

void MatchAnalysisPanel::openNow(GameController& controller,
                                 const MatchEngine& engine, TeamID home_id,
                                 TeamID away_id)
{
  const auto team = controller.getManagedTeam();
  if (!team) return;
  const TeamID managed = team->get().getId();
  if (managed != home_id && managed != away_id) return;
  const bool home = managed == home_id;
  const auto name = [&controller](TeamID id)
  {
    const auto found = controller.getTeamById(id);
    return found ? found->get().getName() : std::string();
  };
  own_name = name(managed);
  other_name = name(home ? away_id : home_id);
  build(controller, engine, home);
  open_requested = true;
}

void MatchAnalysisPanel::build(GameController& controller,
                               const MatchEngine& engine, bool managed_home)
{
  const auto data = controller.getGameData();
  const auto name_of = [data](PlayerID id)
  {
    if (!data) return std::string();
    const auto player = std::as_const(*data).getPlayer(id);
    return player ? player->get().getName() : std::string();
  };
  const MatchAnalysis analysis =
      analyseLiveMatch(engine, managed_home, name_of);
  const MatchState state = engine.getState();
  title =
      analysis.full_time ? LOC("ANALYSIS_TITLE_FULL")
      : state == MatchState::HALF_TIME
          ? LOC("ANALYSIS_TITLE_HALF")
          : fmt::sprintf(LOC("ANALYSIS_TITLE_LIVE"),
                         static_cast<int>(engine.getMatchTimeMinutes()) + 1);

  const SideSummary& own = analysis.own;
  const SideSummary& other = analysis.opponent;
  rows.clear();
  rows.push_back({LOC("ANALYSIS_ROW_GOALS"), std::to_string(own.goals),
                  std::to_string(other.goals)});
  rows.push_back({LOC("ANALYSIS_ROW_SHOTS"),
                  std::format("{} ({})", own.shots, own.shots_on_target),
                  std::format("{} ({})", other.shots, other.shots_on_target)});
  rows.push_back({LOC("ANALYSIS_ROW_XG"), std::format("{:.2f}", own.xg),
                  std::format("{:.2f}", other.xg)});
  rows.push_back({LOC("ANALYSIS_ROW_POSSESSION"),
                  std::format("{:.0f}%", own.possession),
                  std::format("{:.0f}%", other.possession)});
  rows.push_back({LOC("ANALYSIS_ROW_PASSING"),
                  percentText(own.passCompletion()),
                  percentText(other.passCompletion())});
  rows.push_back(
      {LOC("ANALYSIS_ROW_PRESS"),
       std::format("{:.1f}",
                   own.passesAllowedPerAction(other.passes_attempted)),
       std::format("{:.1f}",
                   other.passesAllowedPerAction(own.passes_attempted))});
  // The engine only counts aerial duels in some situations; no row at 0/0.
  if (own.aerials_won + own.aerials_lost + other.aerials_won +
          other.aerials_lost >
      0)
    rows.push_back({LOC("ANALYSIS_ROW_AERIALS"),
                    std::format("{}/{}", own.aerials_won,
                                own.aerials_won + own.aerials_lost),
                    std::format("{}/{}", other.aerials_won,
                                other.aerials_won + other.aerials_lost)});

  observations.clear();
  for (const AnalysisLine& line : analysis.observations)
    observations.push_back(GuidanceUI::text(line));
  notes.clear();
  for (const PlayerNote& note : analysis.notes)
  {
    const std::string value = note.kind == PlayerNote::Kind::Tired
                                  ? percentText(note.value)
                                  : std::format("{:.1f}", note.value);
    notes.push_back(fmt::sprintf(LOC(noteKey(note.kind)),
                                 name_of(note.player).c_str(), value.c_str()));
  }
  sample = GuidanceUI::text(analysis.sample);
  suggestions.clear();
  for (const AnalysisSuggestion& suggestion : analysis.suggestions)
    suggestions.push_back({GuidanceUI::text(suggestion.action),
                           GuidanceUI::text(suggestion.reason)});
}

void MatchAnalysisPanel::render()
{
  if (open_requested)
  {
    open_requested = false;
    ImGui::OpenPopup(POPUP_ID);
  }
  visible = ImGui::IsPopupOpen(POPUP_ID);
  if (!visible) return;

  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const float width = std::min(scaled(DIALOG_WIDTH),
                               viewport->WorkSize.x * VIEWPORT_WIDTH_SHARE);
  ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always,
                          ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSizeConstraints(
      ImVec2(width, 0.0f),
      ImVec2(width, viewport->WorkSize.y * VIEWPORT_HEIGHT_SHARE));
  if (!ImGui::BeginPopupModal(POPUP_ID, nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize |
                                  ImGuiWindowFlags_NoTitleBar |
                                  ImGuiWindowFlags_NoSavedSettings))
  {
    visible = false;
    return;
  }
  const Theme::Palette& palette = Theme::palette();
  // Explicit wrap edge: an auto-resizing popup would otherwise grow instead.
  const float wrap = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
  {
    Theme::ScopedText heading(Theme::Text::TITLE);
    UI::textFitted(title, ImGui::GetContentRegionAvail().x, palette.text);
  }
  ImGui::Separator();

  // Side-by-side numbers: label, managed club, opponent.
  const float full = ImGui::GetContentRegionAvail().x;
  const float valueWidth = std::max(scaled(90.0f), full * 0.22f);
  const auto columns = [&](const char* label, const std::string& own,
                           const std::string& other, const ImVec4& color)
  {
    const float x = ImGui::GetCursorPosX();
    UI::textFitted(label, full - 2.0f * valueWidth, color);
    ImGui::SameLine(x + full - 2.0f * valueWidth);
    UI::textFitted(own, valueWidth, color);
    ImGui::SameLine(x + full - valueWidth);
    UI::textFitted(other, valueWidth, color);
  };
  columns("", own_name, other_name, palette.muted);
  for (const CompareRow& row : rows)
    columns(row.label.c_str(), row.own, row.other, palette.text);
  {
    Theme::ScopedText small(Theme::Text::SMALL);
    ImGui::PushTextWrapPos(wrap);
    ImGui::TextColored(palette.muted, "%s", LOC("ANALYSIS_PRESS_HELP"));
    ImGui::PopTextWrapPos();
  }

  ImGui::Dummy(ImVec2(0.0f, scaled(Theme::Space::XS)));
  UI::sectionLabel(LOC("ANALYSIS_OBSERVATIONS"));
  ImGui::PushTextWrapPos(wrap);
  for (const std::string& line : observations)
    ImGui::BulletText("%s", line.c_str());
  for (const std::string& line : notes) ImGui::BulletText("%s", line.c_str());
  ImGui::PopTextWrapPos();

  ImGui::Dummy(ImVec2(0.0f, scaled(Theme::Space::XS)));
  UI::sectionLabel(LOC("ANALYSIS_SUGGESTIONS"));
  {
    Theme::ScopedText small(Theme::Text::SMALL);
    ImGui::PushTextWrapPos(wrap);
    ImGui::TextColored(palette.muted, "%s", sample.c_str());
    ImGui::PopTextWrapPos();
  }
  for (std::size_t index = 0; index < suggestions.size(); ++index)
  {
    ImGui::PushTextWrapPos(wrap);
    ImGui::Text("%zu. %s", index + 1, suggestions[index].action.c_str());
    {
      Theme::ScopedText small(Theme::Text::SMALL);
      ImGui::Indent(scaled(Theme::Space::L));
      ImGui::TextColored(palette.muted, "%s",
                         suggestions[index].reason.c_str());
      ImGui::Unindent(scaled(Theme::Space::L));
    }
    ImGui::PopTextWrapPos();
  }

  ImGui::Dummy(ImVec2(0.0f, scaled(Theme::Space::S)));
  if (UI::primaryButton(LOC("ANALYSIS_CLOSE"), ImVec2(-FLT_MIN, 0.0f)))
  {
    ImGui::CloseCurrentPopup();
    visible = false;
  }
  ImGui::EndPopup();
}
