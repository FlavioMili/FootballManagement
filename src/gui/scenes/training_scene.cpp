// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "gui/scenes/training_scene.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <format>

#include "global/language_manager.h"
#include "gui/gui_view.h"
#include "gui/widgets/format.h"
#include "gui/widgets/theme.h"
#include "gui/widgets/widgets.h"
#include "model/inbox.h"
#include "model/role_utils.h"

namespace
{
constexpr float TWO_COLUMN_MIN_WIDTH = 1000.0f;
constexpr float SCHEDULE_CARD_HEIGHT = 250.0f;
constexpr float WEEK_CELL_HEIGHT = 64.0f;
constexpr float TREND_UP = 0.03f;
constexpr int VETERAN_AGE = 31;

enum Column : int
{
  NAME = 0,
  POSITION,
  AGE,
  CONDITION,
  WORKLOAD,
  RISK,
  TREND,
  FOCUS,
  COLUMN_COUNT
};

float dpi() { return ImGui::GetStyle().FontScaleDpi; }

ImVec4 riskColor(WorkloadRisk risk)
{
  const Theme::Palette& palette = Theme::palette();
  switch (risk)
  {
    case WorkloadRisk::High:
      return palette.negative;
    case WorkloadRisk::Moderate:
      return palette.warning;
    case WorkloadRisk::Low:
      break;
  }
  return palette.positive;
}

ImVec4 severityColor(AdviceSeverity severity)
{
  const Theme::Palette& palette = Theme::palette();
  switch (severity)
  {
    case AdviceSeverity::Warning:
      return palette.negative;
    case AdviceSeverity::Suggestion:
      return palette.warning;
    case AdviceSeverity::Info:
      break;
  }
  return palette.info;
}

/** Up/down triangle or a flat bar for a development trend. */
void trendArrow(float trend, bool veteran)
{
  const Theme::Palette& palette = Theme::palette();
  const float size = ImGui::GetTextLineHeight();
  const ImVec2 start = ImGui::GetCursorScreenPos();
  ImDrawList* drawList = ImGui::GetWindowDrawList();
  const ImVec2 center(start.x + size * 0.5f, start.y + size * 0.5f);
  const float half = size * 0.3f;
  const char* tooltip = "TRAINING_TREND_STEADY";
  if (trend >= TREND_UP)
  {
    drawList->AddTriangleFilled(ImVec2(center.x, center.y - half),
                                ImVec2(center.x + half, center.y + half),
                                ImVec2(center.x - half, center.y + half),
                                Theme::toU32(palette.positive));
    tooltip = "TRAINING_TREND_UP";
  }
  else if (veteran)
  {
    drawList->AddTriangleFilled(ImVec2(center.x - half, center.y - half),
                                ImVec2(center.x + half, center.y - half),
                                ImVec2(center.x, center.y + half),
                                Theme::toU32(palette.negative));
    tooltip = "TRAINING_TREND_DOWN";
  }
  else
  {
    drawList->AddRectFilled(ImVec2(center.x - half, center.y - 1.5f * dpi()),
                            ImVec2(center.x + half, center.y + 1.5f * dpi()),
                            Theme::toU32(palette.muted));
  }
  ImGui::Dummy(ImVec2(size, size));
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOC(tooltip));
}

template <typename Enum>
bool enumCombo(const char* id, Enum& value, std::size_t count,
               const char* (*key)(Enum), float width)
{
  bool changed = false;
  ImGui::SetNextItemWidth(width);
  if (ImGui::BeginCombo(id, LOC(key(value))))
  {
    for (std::size_t index = 0; index < count; ++index)
    {
      const auto option = static_cast<Enum>(index);
      if (ImGui::Selectable(LOC(key(option)), option == value))
      {
        changed = option != value;
        value = option;
      }
    }
    ImGui::EndCombo();
  }
  return changed;
}
}  // namespace

TrainingScene::TrainingScene(GUIView* parent) : ManagementScene(parent) {}

void TrainingScene::update(float /*deltaTime*/) {}

void TrainingScene::refresh()
{
  GameController& controller = guiView->getController();
  rows.clear();
  week.clear();
  advice.clear();
  has_plan = false;
  const auto managed = controller.getManagedTeam();
  const TeamTrainingPlan* current = controller.getTrainingPlan();
  if (!managed || !current) return;
  plan = *current;
  has_plan = true;
  const TeamID team_id = managed->get().getId();

  float condition_total = 0.0f;
  int fit = 0;
  at_risk = 0;
  for (const auto& player_ref : controller.getPlayersForTeam(team_id))
  {
    const Player& player = player_ref.get();
    const GameController::PlayerWorkload workload =
        controller.getPlayerWorkload(player.getId());
    PlayerRow row;
    row.id = player.getId();
    row.name = player.getName();
    row.role = RoleUtils::toString(player.getRole());
    row.age = player.getAge();
    row.condition = player.getDynamics().condition;
    row.ratio = workload.ratio;
    row.risk = workload.risk;
    row.trend = workload.trend;
    row.focus = workload.focus;
    row.injured = !player.isAvailable();
    row.veteran = row.age >= VETERAN_AGE && row.trend < TREND_UP;
    row.condition_text = std::format("{:.0f}", row.condition);
    row.ratio_text = std::format("{:.2f}", row.ratio);
    if (!row.injured)
    {
      condition_total += row.condition;
      ++fit;
      if (row.risk == WorkloadRisk::High) ++at_risk;
    }
    rows.push_back(std::move(row));
  }
  average_condition =
      fit > 0 ? condition_total / static_cast<float>(fit) : 0.0f;
  familiarity = controller.getTacticalFamiliarity(team_id);
  effectiveness = controller.getStaffEffects(team_id).training_quality;
  sortRows();

  for (const GameController::TrainingDayPreview& preview :
       controller.getTrainingWeekPreview())
  {
    DayCell cell;
    cell.date = Format::dayMonth(preview.date);
    cell.match_day = preview.day.match_day;
    if (cell.match_day)
    {
      const auto opponent = controller.getTeamById(preview.opponent);
      cell.title = LOC("TRAINING_MATCH_DAY");
      cell.detail = std::format(
          "{} {}", LOC(preview.home ? "TRAINING_VS" : "TRAINING_AT"),
          opponent ? opponent->get().getName() : std::string("–"));
    }
    else
    {
      cell.title = LOC(TrainingModel::sessionKey(preview.day.slot.session));
      cell.detail =
          preview.day.slot.session == SessionType::Rest
              ? std::string()
              : LOC(TrainingModel::intensityKey(preview.day.slot.intensity));
      cell.adjusted = preview.day.adjusted;
      cell.load = TrainingModel::sessionLoad(preview.day.slot, plan.intensity);
    }
    week.push_back(std::move(cell));
  }

  for (const TrainingAdvice& item : controller.getTrainingAdvice())
    advice.push_back({item.severity, formatLocalized(item.key, item.args)});
}

void TrainingScene::sortRows()
{
  const auto key = [this](const PlayerRow& row) -> float
  {
    switch (sort_column)
    {
      case AGE:
        return static_cast<float>(row.age);
      case CONDITION:
        return row.condition;
      case WORKLOAD:
        return row.ratio;
      case RISK:
        return static_cast<float>(row.risk);
      case TREND:
        return row.trend;
      case FOCUS:
        return static_cast<float>(row.focus);
      default:
        return 0.0f;
    }
  };
  std::ranges::stable_sort(rows,
                           [&](const PlayerRow& a, const PlayerRow& b)
                           {
                             int order = 0;
                             if (sort_column == NAME)
                               order = UI::compare(a.name, b.name);
                             else if (sort_column == POSITION)
                               order = UI::compare(a.role, b.role);
                             else
                               order = UI::compare(key(a), key(b));
                             return sort_ascending ? order < 0 : order > 0;
                           });
}

void TrainingScene::renderContent()
{
  if (!has_plan) return;
  const Theme::Palette& palette = Theme::palette();
  UI::pageHeader(LOC("TRAINING_TITLE"), LOC("TRAINING_SUBTITLE"));

  const float gap = ImGui::GetStyle().ItemSpacing.x;
  const float tile = (ImGui::GetContentRegionAvail().x - 3.0f * gap) / 4.0f;
  const std::string condition = std::format("{:.0f}%", average_condition);
  UI::statTile("condition", LOC("TRAINING_TILE_CONDITION"), condition.c_str(),
               LOC("TRAINING_TILE_CONDITION_NOTE"),
               average_condition >= 85.0f   ? palette.positive
               : average_condition >= 75.0f ? palette.warning
                                            : palette.negative,
               tile);
  ImGui::SameLine();
  const std::string drilled = std::format("{:.0f}%", familiarity * 100.0f);
  UI::statTile("familiarity", LOC("TRAINING_TILE_FAMILIARITY"), drilled.c_str(),
               LOC("TRAINING_TILE_FAMILIARITY_NOTE"),
               familiarity >= 0.7f    ? palette.positive
               : familiarity >= 0.45f ? palette.warning
                                      : palette.negative,
               tile);
  ImGui::SameLine();
  const std::string risk = std::to_string(at_risk);
  UI::statTile("risk", LOC("TRAINING_TILE_RISK"), risk.c_str(),
               LOC("TRAINING_TILE_RISK_NOTE"),
               at_risk == 0 ? palette.positive : palette.negative, tile);
  ImGui::SameLine();
  const std::string coaching = std::format("{:.0f}", effectiveness * 100.0f);
  UI::statTile("coaching", LOC("TRAINING_TILE_COACHING"), coaching.c_str(),
               LOC("TRAINING_TILE_COACHING_NOTE"),
               Theme::ratingColor(effectiveness * 100.0f), tile);

  ImGui::Dummy(ImVec2(0.0f, Theme::Space::XS * dpi()));
  const float available = ImGui::GetContentRegionAvail().x;
  const bool twoColumns = available >= TWO_COLUMN_MIN_WIDTH * dpi();
  const float height = SCHEDULE_CARD_HEIGHT * dpi();
  const float left =
      twoColumns ? std::floor((available - gap) * 0.64f) : available;
  renderMicrocycle(left, height);
  if (twoColumns) ImGui::SameLine();
  // Stacked on narrow windows: the advice card only takes what it needs.
  renderAdvice(twoColumns ? available - gap - left : available,
               twoColumns ? height : 0.0f);
  renderWeek(available);
  renderPlayers(std::max(ImGui::GetContentRegionAvail().y, 260.0f * dpi()));
}

void TrainingScene::renderControls()
{
  GameController& controller = guiView->getController();
  const float comboWidth = 170.0f * dpi();
  ImGui::AlignTextToFramePadding();
  ImGui::TextColored(Theme::palette().muted, "%s", LOC("TRAINING_PRESET"));
  ImGui::SameLine();
  TrainingPreset preset = plan.preset;
  if (enumCombo("##preset", preset,
                static_cast<std::size_t>(TrainingPreset::Custom),
                TrainingModel::presetKey, comboWidth) &&
      controller.setTrainingPreset(preset))
  {
    refresh();
    showToast(LOC("TRAINING_PRESET_TOAST"));
  }
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("%s", LOC("TRAINING_PRESET_HELP"));
  ImGui::SameLine(0.0f, Theme::Space::L * dpi());
  ImGui::TextColored(Theme::palette().muted, "%s", LOC("TRAINING_INTENSITY"));
  ImGui::SameLine();
  TrainingIntensity intensity = plan.intensity;
  if (enumCombo("##squad_intensity", intensity,
                static_cast<std::size_t>(TrainingIntensity::COUNT),
                TrainingModel::intensityKey, 120.0f * dpi()) &&
      controller.setTrainingIntensity(intensity))
    refresh();
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("%s", LOC("TRAINING_INTENSITY_HELP"));
  ImGui::SameLine(0.0f, Theme::Space::L * dpi());
  bool automatic = plan.auto_congestion;
  if (ImGui::Checkbox(LOC("TRAINING_AUTO_CONGESTION"), &automatic) &&
      controller.setCongestionAutoAdjust(automatic))
    refresh();
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("%s", LOC("TRAINING_AUTO_CONGESTION_HELP"));
}

void TrainingScene::renderMicrocycle(float width, float height)
{
  GameController& controller = guiView->getController();
  const Theme::Palette& palette = Theme::palette();
  UI::beginCard("training_schedule", LOC("TRAINING_SCHEDULE"),
                ImVec2(width, height));
  renderControls();
  ImGui::Dummy(ImVec2(0.0f, Theme::Space::XS * dpi()));
  constexpr int DAYS = static_cast<int>(MICROCYCLE_DAYS);
  if (UI::beginDataTable(
          "microcycle", DAYS,
          ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchSame,
          640.0f, ImVec2(0.0f, 0.0f), 0))
  {
    for (int day = 0; day < DAYS; ++day)
      ImGui::TableSetupColumn(
          LOC(TrainingModel::dayKey(static_cast<MicrocycleDay>(day))));
    ImGui::TableHeadersRow();
    ImGui::TableNextRow();
    for (int day = 0; day < DAYS; ++day)
    {
      ImGui::TableNextColumn();
      ImGui::PushID(day);
      TrainingSlot slot = plan.slots[static_cast<std::size_t>(day)];
      const float cellWidth = ImGui::GetContentRegionAvail().x;
      bool changed = enumCombo("##session", slot.session, SESSION_TYPE_COUNT,
                               TrainingModel::sessionKey, cellWidth);
      ImGui::BeginDisabled(slot.session == SessionType::Rest);
      changed |= enumCombo("##intensity", slot.intensity,
                           static_cast<std::size_t>(TrainingIntensity::COUNT),
                           TrainingModel::intensityKey, cellWidth);
      ImGui::EndDisabled();
      const float load = TrainingModel::sessionLoad(slot, plan.intensity);
      UI::meter("", std::min(1.0f, load / 1.8f), 0.0f,
                load > 1.3f   ? palette.negative
                : load > 0.7f ? palette.warning
                              : palette.positive,
                "");
      if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", LOC("TRAINING_LOAD_HELP"));
      if (changed &&
          controller.setTrainingSlot(static_cast<MicrocycleDay>(day), slot))
        refresh();
      ImGui::PopID();
    }
    ImGui::EndTable();
  }
  ImGui::TextColored(palette.faint, "%s", LOC("TRAINING_SCHEDULE_HELP"));
  UI::endCard();
}

void TrainingScene::renderWeek(float width)
{
  const Theme::Palette& palette = Theme::palette();
  UI::beginAutoHeightCard("training_week", LOC("TRAINING_WEEK"));
  const int count = static_cast<int>(week.size());
  if (count > 0)
  {
    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const float inner = ImGui::GetContentRegionAvail().x;
    const float cellWidth = std::max(
        1.0f, (std::min(inner, width) - gap * static_cast<float>(count - 1)) /
                  static_cast<float>(count));
    const float cellHeight = WEEK_CELL_HEIGHT * dpi();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    for (int index = 0; index < count; ++index)
    {
      const DayCell& cell = week[static_cast<std::size_t>(index)];
      if (index > 0) ImGui::SameLine();
      const ImVec2 start = ImGui::GetCursorScreenPos();
      const ImVec2 end(start.x + cellWidth, start.y + cellHeight);
      const ImVec4& tone = cell.match_day ? palette.accent : palette.raised;
      drawList->AddRectFilled(start, end,
                              Theme::toU32(tone, cell.match_day ? 0.25f : 1.0f),
                              4.0f * dpi());
      if (!cell.match_day && cell.load > 0.0f)
      {
        const float bar = std::min(1.0f, cell.load / 1.8f);
        drawList->AddRectFilled(
            ImVec2(start.x, end.y - 3.0f * dpi()),
            ImVec2(start.x + cellWidth * bar, end.y),
            Theme::toU32(bar > 0.72f ? palette.negative : palette.warning));
      }
      const float pad = Theme::Space::S * dpi();
      drawList->PushClipRect(start, end, true);
      drawList->AddText(ImVec2(start.x + pad, start.y + pad * 0.5f),
                        Theme::toU32(palette.muted), cell.date.c_str());
      drawList->AddText(ImVec2(start.x + pad, start.y + pad * 0.5f +
                                                  ImGui::GetTextLineHeight()),
                        Theme::toU32(palette.text), cell.title.c_str());
      drawList->AddText(
          ImVec2(start.x + pad,
                 start.y + pad * 0.5f + 2.0f * ImGui::GetTextLineHeight()),
          Theme::toU32(cell.adjusted ? palette.warning : palette.faint),
          cell.detail.c_str());
      drawList->PopClipRect();
      ImGui::Dummy(ImVec2(cellWidth, cellHeight));
      if (cell.adjusted && ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", LOC("TRAINING_ADJUSTED_HELP"));
    }
  }
  UI::endCard();
}

void TrainingScene::renderAdvice(float width, float height)
{
  if (height > 0.0f)
    UI::beginCard("training_advice", LOC("TRAINING_ADVICE"),
                  ImVec2(width, height), true);
  else
    UI::beginAutoHeightCard("training_advice", LOC("TRAINING_ADVICE"));
  for (const AdviceLine& line : advice)
  {
    const ImVec4 color = severityColor(line.severity);
    const float size = 8.0f * dpi();
    const ImVec2 start = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddCircleFilled(
        ImVec2(start.x + size * 0.5f,
               start.y + ImGui::GetTextLineHeight() * 0.5f),
        size * 0.5f, Theme::toU32(color));
    ImGui::Dummy(ImVec2(size, 0.0f));
    ImGui::SameLine();
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(line.text.c_str());
    ImGui::PopTextWrapPos();
    ImGui::Dummy(ImVec2(0.0f, Theme::Space::XS * dpi()));
  }
  UI::endCard();
}

void TrainingScene::renderPlayers(float height)
{
  GameController& controller = guiView->getController();
  const Theme::Palette& palette = Theme::palette();
  UI::beginCard("training_players", LOC("TRAINING_INDIVIDUAL"),
                ImVec2(0.0f, height));
  if (rows.empty())
  {
    UI::emptyState(LOC("TRAINING_NO_PLAYERS_TITLE"),
                   LOC("TRAINING_NO_PLAYERS_BODY"));
    UI::endCard();
    return;
  }
  if (UI::beginDataTable("training_table", COLUMN_COUNT,
                         ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                             ImGuiTableFlags_ScrollY |
                             ImGuiTableFlags_Sortable |
                             ImGuiTableFlags_SizingFixedFit,
                         820.0f, ImVec2(0.0f, 0.0f), 1))
  {
    ImGui::TableSetupColumn(
        LOC("TRAINING_COL_PLAYER"),
        ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_DefaultSort);
    ImGui::TableSetupColumn(LOC("TRAINING_COL_POSITION"));
    ImGui::TableSetupColumn(LOC("TRAINING_COL_AGE"));
    ImGui::TableSetupColumn(LOC("TRAINING_COL_CONDITION"),
                            ImGuiTableColumnFlags_WidthFixed, 130.0f * dpi());
    ImGui::TableSetupColumn(LOC("TRAINING_COL_WORKLOAD"));
    ImGui::TableSetupColumn(LOC("TRAINING_COL_RISK"));
    ImGui::TableSetupColumn(LOC("TRAINING_COL_TREND"));
    ImGui::TableSetupColumn(LOC("TRAINING_COL_FOCUS"),
                            ImGuiTableColumnFlags_WidthFixed, 190.0f * dpi());
    ImGui::TableSetupScrollFreeze(1, 1);
    ImGui::TableHeadersRow();
    if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs();
        specs && specs->SpecsDirty && specs->SpecsCount > 0)
    {
      sort_column = specs->Specs[0].ColumnIndex;
      sort_ascending =
          specs->Specs[0].SortDirection == ImGuiSortDirection_Ascending;
      sortRows();
      specs->SpecsDirty = false;
    }

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(rows.size()));
    while (clipper.Step())
    {
      for (int index = clipper.DisplayStart; index < clipper.DisplayEnd;
           ++index)
      {
        PlayerRow& row = rows[static_cast<std::size_t>(index)];
        ImGui::PushID(static_cast<int>(row.id));
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        if (UI::link(row.name.c_str(), "##player"))
          Navigation::openPlayer(guiView, row.id);
        ImGui::TableNextColumn();
        ImGui::TextColored(palette.muted, "%s", row.role.c_str());
        ImGui::TableNextColumn();
        ImGui::Text("%d", row.age);
        ImGui::TableNextColumn();
        if (row.injured)
        {
          UI::badge(LOC("TRAINING_INJURED"), palette.negative);
        }
        else
        {
          UI::meter("", row.condition / 100.0f, 0.0f,
                    row.condition >= 85.0f   ? palette.positive
                    : row.condition >= 70.0f ? palette.warning
                                             : palette.negative,
                    row.condition_text.c_str());
        }
        ImGui::TableNextColumn();
        ImGui::TextColored(row.ratio > 1.3f ? palette.warning : palette.text,
                           "%s", row.ratio_text.c_str());
        if (ImGui::IsItemHovered())
          ImGui::SetTooltip("%s", LOC("TRAINING_WORKLOAD_HELP"));
        ImGui::TableNextColumn();
        UI::badge(LOC(TrainingModel::riskKey(row.risk)), riskColor(row.risk));
        ImGui::TableNextColumn();
        trendArrow(row.trend, row.veteran);
        ImGui::TableNextColumn();
        if (enumCombo("##focus", row.focus, TRAINING_FOCUS_COUNT,
                      TrainingModel::focusKey, -FLT_MIN) &&
            controller.setPlayerTrainingFocus(row.id, row.focus))
          showToast(LOC("TRAINING_FOCUS_TOAST"));
        ImGui::PopID();
      }
    }
    ImGui::EndTable();
  }
  UI::endCard();
}
