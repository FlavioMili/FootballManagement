// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "strategy_scene.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

#include "global/language_manager.h"
#include "gui/gui_view.h"
#include "gui/widgets/theme.h"
#include "gui/widgets/widgets.h"
#include "model/lineup.h"
#include "model/team.h"

namespace
{
constexpr float PRESET_BUTTON_HEIGHT = 58.0f;
constexpr float SLIDER_LABEL_WIDTH = 170.0f;
constexpr float SUMMARY_MIN_WIDTH = 900.0f;
constexpr float EPSILON = 0.001f;

struct TacticalPreset
{
  const char* nameKey;
  const char* descriptionKey;
  StrategySliders sliders;
};

constexpr std::array<TacticalPreset, 4> PRESETS{{
    {"TACTIC_BALANCED",
     "TACTIC_BALANCED_HELP",
     {0.50f, 0.50f, 0.50f, 0.50f, 0.50f}},
    {"TACTIC_FRONT_FOOT",
     "TACTIC_FRONT_FOOT_HELP",
     {0.82f, 0.72f, 0.76f, 0.68f, 0.64f}},
    {"TACTIC_COUNTER",
     "TACTIC_COUNTER_HELP",
     {0.38f, 0.68f, 0.62f, 0.58f, 0.42f}},
    {"TACTIC_CONTROL",
     "TACTIC_CONTROL_HELP",
     {0.62f, 0.34f, 0.44f, 0.72f, 0.72f}},
}};

struct SliderInfo
{
  const char* key;
  const char* id;
  float StrategySliders::* value;
  const char* helpKey;
};

constexpr std::array<SliderInfo, 5> SLIDERS{{
    {"STRATEGY_PRESSING", "pressing", &StrategySliders::pressing,
     "TACTIC_PRESSING_HELP"},
    {"STRATEGY_RISK_TAKING", "risk", &StrategySliders::riskTaking,
     "TACTIC_RISK_HELP"},
    {"STRATEGY_OFFENSIVE_BIAS", "offensive", &StrategySliders::offensiveBias,
     "TACTIC_OFFENSIVE_HELP"},
    {"STRATEGY_WIDTH_USAGE", "width", &StrategySliders::widthUsage,
     "TACTIC_WIDTH_HELP"},
    {"STRATEGY_COMPACTNESS", "compactness", &StrategySliders::compactness,
     "TACTIC_COMPACTNESS_HELP"},
}};

bool sameSliders(const StrategySliders& left, const StrategySliders& right)
{
  return std::ranges::all_of(
      SLIDERS, [&](const SliderInfo& slider)
      { return std::abs(left.*slider.value - right.*slider.value) < EPSILON; });
}

constexpr std::array<const char*, 5> LEVEL_KEYS = {
    "TACTIC_LEVEL_VERY_LOW", "TACTIC_LEVEL_LOW", "TACTIC_LEVEL_MEDIUM",
    "TACTIC_LEVEL_HIGH", "TACTIC_LEVEL_VERY_HIGH"};

const char* levelKey(float value)
{
  const auto level = static_cast<size_t>(std::clamp(value, 0.0f, 0.999f) *
                                         static_cast<float>(LEVEL_KEYS.size()));
  return LEVEL_KEYS[level];
}

bool tacticSlider(const SliderInfo& slider, float& value)
{
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(LOC(slider.key));
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOC(slider.helpKey));
  ImGui::SameLine(SLIDER_LABEL_WIDTH * Theme::scale());
  // The level is written beside the slider, never on top of its handle.
  float levelWidth = 0.0f;
  for (const char* key : LEVEL_KEYS)
    levelWidth = std::max(levelWidth, ImGui::CalcTextSize(LOC(key)).x);
  ImGui::SetNextItemWidth(std::max(
      60.0f * Theme::scale(), ImGui::GetContentRegionAvail().x - levelWidth -
                                  ImGui::GetStyle().ItemSpacing.x));
  const std::string label = std::string("###") + slider.id;
  const bool changed =
      ImGui::SliderFloat(label.c_str(), &value, 0.0f, 1.0f, "");
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("%s (%.0f%%)", LOC(slider.helpKey),
                      static_cast<double>(value * 100.0f));
  ImGui::SameLine();
  ImGui::TextColored(Theme::palette().muted, "%s", LOC(levelKey(value)));
  return changed;
}

// Illustrative shape: the current lineup moved by the instructions (pressing
// and attacking intent push the block up, width spreads it, compactness
// squeezes the lines together).
void shapePreview(const Lineup& lineup, const StrategySliders& sliders,
                  ImVec2 size)
{
  const Theme::Palette& palette = Theme::palette();
  const float scale = Theme::scale();
  const ImVec2 min = ImGui::GetCursorScreenPos();
  const ImVec2 max(min.x + size.x, min.y + size.y);
  ImDrawList* drawList = ImGui::GetWindowDrawList();
  drawList->AddRectFilled(min, max, IM_COL32(32, 106, 60, 255), 4.0f * scale);
  const ImU32 line = IM_COL32(255, 255, 255, 110);
  drawList->AddRect(min, max, line, 4.0f * scale, 0, 1.2f * scale);
  const float midX = min.x + size.x * 0.5f;
  drawList->AddLine(ImVec2(midX, min.y), ImVec2(midX, max.y), line,
                    1.2f * scale);
  drawList->AddCircle(ImVec2(midX, min.y + size.y * 0.5f), size.y * 0.16f, line,
                      32, 1.2f * scale);

  const auto& outfield = lineup.getOutfieldPlayers();
  float meanX = 0.0f;
  for (const auto& positioned : outfield) meanX += positioned.position.x;
  meanX =
      outfield.empty() ? 0.45f : meanX / static_cast<float>(outfield.size());
  const float shift = 0.14f * (sliders.pressing - 0.5f) +
                      0.10f * (sliders.offensiveBias - 0.5f);
  const float depth = 1.25f - 0.55f * sliders.compactness;
  const float spread = 0.70f + 0.45f * sliders.widthUsage;
  const float radius = 4.5f * scale;
  for (const auto& positioned : outfield)
  {
    const float x = std::clamp(
        meanX + shift + (positioned.position.x - meanX) * depth, 0.04f, 0.96f);
    const float y = std::clamp(0.5f + (positioned.position.y - 0.5f) * spread,
                               0.05f, 0.95f);
    drawList->AddCircleFilled(ImVec2(min.x + x * size.x, min.y + y * size.y),
                              radius, Theme::toU32(palette.accent));
    drawList->AddCircle(ImVec2(min.x + x * size.x, min.y + y * size.y), radius,
                        IM_COL32(255, 255, 255, 200), 0, 1.0f * scale);
  }
  drawList->AddCircleFilled(
      ImVec2(min.x + 0.04f * size.x, min.y + size.y * 0.5f), radius,
      Theme::toU32(palette.warning));
  ImGui::Dummy(size);
}
}  // namespace

StrategyScene::StrategyScene(GUIView* parent) : ManagementScene(parent) {}

void StrategyScene::update(float deltaTime) { (void)deltaTime; }

void StrategyScene::renderContent()
{
  UI::pageHeader(LOC("TACTIC_IDENTITY"), LOC("TACTIC_IDENTITY_HELP"));
  const float available = ImGui::GetContentRegionAvail().x;
  const float gap = ImGui::GetStyle().ItemSpacing.x;
  if (available >= SUMMARY_MIN_WIDTH * Theme::scale())
  {
    const float summaryWidth = std::floor((available - gap) * 0.34f);
    renderInstructions(available - gap - summaryWidth);
    ImGui::SameLine();
    renderSummary(summaryWidth);
  }
  else
  {
    renderInstructions(available);
    renderSummary(available);
  }
}

void StrategyScene::renderInstructions(float width)
{
  // Sized to content: never clips the sliders, the page scrolls if needed.
  UI::beginAutoHeightCard("tactic_setup", LOC("TACTIC_PRESETS"), width);
  const float gap = ImGui::GetStyle().ItemSpacing.x;
  const float buttonWidth =
      (ImGui::GetContentRegionAvail().x - gap * (PRESETS.size() - 1)) /
      static_cast<float>(PRESETS.size());
  for (size_t index = 0; index < PRESETS.size(); ++index)
  {
    if (index > 0) ImGui::SameLine();
    ImGui::PushID(static_cast<int>(index));
    const bool active = selected_preset == static_cast<int>(index);
    if (UI::toggleButton(
            LOC(PRESETS[index].nameKey), active,
            ImVec2(buttonWidth, PRESET_BUTTON_HEIGHT * Theme::scale())))
    {
      current_sliders = PRESETS[index].sliders;
      selected_preset = static_cast<int>(index);
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
      ImGui::SetTooltip("%s", LOC(PRESETS[index].descriptionKey));
    ImGui::PopID();
  }

  ImGui::Spacing();
  const char* description =
      selected_preset >= 0
          ? LOC(PRESETS[static_cast<size_t>(selected_preset)].descriptionKey)
          : LOC("TACTIC_CUSTOM_HELP");
  ImGui::PushTextWrapPos(0.0f);
  ImGui::TextColored(Theme::palette().muted, "%s", description);
  ImGui::PopTextWrapPos();
  ImGui::Spacing();
  ImGui::SeparatorText(LOC("TACTIC_DETAILS"));

  bool customized = false;
  for (const SliderInfo& slider : SLIDERS)
    customized |= tacticSlider(slider, current_sliders.*slider.value);
  if (customized) selected_preset = -1;

  ImGui::Spacing();
  ImGui::PushTextWrapPos(0.0f);
  ImGui::TextColored(Theme::palette().faint, "%s",
                     LOC("TACTIC_SAVE_HELP_SHELL"));
  ImGui::PopTextWrapPos();
  const bool dirty = hasUnsavedChanges();
  ImGui::BeginDisabled(!dirty);
  if (UI::secondaryButton(LOC("TACTIC_REVERT"))) loadStrategy();
  ImGui::SameLine();
  if (UI::primaryButton(LOC("STRATEGY_APPLY")))
  {
    saveStrategy();
    showToast(LOC("TACTIC_APPLIED_TOAST"));
  }
  ImGui::EndDisabled();
  if (dirty)
  {
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(Theme::palette().warning, "%s", LOC("TACTIC_UNSAVED"));
  }
  UI::endCard();
}

void StrategyScene::renderSummary(float width)
{
  const Theme::Palette& palette = Theme::palette();
  UI::beginAutoHeightCard("tactic_summary", LOC("TACTIC_SUMMARY"), width);
  if (const auto managed = guiView->getController().getManagedTeam())
  {
    const float previewWidth = ImGui::GetContentRegionAvail().x;
    shapePreview(managed->get().getLineup(), current_sliders,
                 ImVec2(previewWidth, previewWidth / 1.55f));
    ImGui::TextColored(palette.faint, "%s", LOC("TACTIC_SHAPE_PREVIEW"));
    ImGui::Dummy(ImVec2(0.0f, Theme::Space::XS * Theme::scale()));
  }
  const float labelWidth = 140.0f * Theme::scale();
  for (const SliderInfo& slider : SLIDERS)
  {
    const float value = current_sliders.*slider.value;
    UI::meter(LOC(slider.key), value, labelWidth, palette.accent,
              LOC(levelKey(value)));
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOC(slider.helpKey));
  }
  ImGui::Dummy(ImVec2(0.0f, Theme::Space::S * Theme::scale()));
  ImGui::PushTextWrapPos(0.0f);
  ImGui::TextColored(palette.faint, "%s", LOC("TACTIC_SUMMARY_HELP"));
  ImGui::PopTextWrapPos();
  UI::endCard();
}

bool StrategyScene::hasUnsavedChanges() const
{
  return !sameSliders(current_sliders, saved_sliders);
}

void StrategyScene::saveStrategy()
{
  auto managedTeam = guiView->getController().getManagedTeam();
  if (managedTeam)
  {
    managedTeam->get().getStrategy().setAllSliders(current_sliders);
    guiView->getController().saveGame();
    saved_sliders = current_sliders;
  }
}

void StrategyScene::loadStrategy()
{
  const auto managedTeam = guiView->getController().getManagedTeam();
  if (managedTeam)
    current_sliders = managedTeam->get().getStrategy().getSliders();
  saved_sliders = current_sliders;
  selected_preset = -1;
  for (size_t index = 0; index < PRESETS.size(); ++index)
  {
    if (sameSliders(current_sliders, PRESETS[index].sliders))
    {
      selected_preset = static_cast<int>(index);
      break;
    }
  }
}

SceneID StrategyScene::getID() const { return SceneID::STRATEGY; }
