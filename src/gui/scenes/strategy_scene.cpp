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

const char* levelKey(float value)
{
  if (value < 0.34f) return "TACTIC_LEVEL_LOW";
  if (value < 0.67f) return "TACTIC_LEVEL_MEDIUM";
  return "TACTIC_LEVEL_HIGH";
}

bool tacticSlider(const SliderInfo& slider, float& value)
{
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(LOC(slider.key));
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOC(slider.helpKey));
  ImGui::SameLine(SLIDER_LABEL_WIDTH * Theme::scale());
  ImGui::SetNextItemWidth(-FLT_MIN);
  const std::string label = std::string("###") + slider.id;
  const bool changed =
      ImGui::SliderFloat(label.c_str(), &value, 0.0f, 1.0f, "%.2f");
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOC(slider.helpKey));
  return changed;
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
  const float height =
      std::max(ImGui::GetContentRegionAvail().y, 420.0f * Theme::scale());
  UI::beginCard("tactic_setup", LOC("TACTIC_PRESETS"), ImVec2(width, height));
  const float gap = ImGui::GetStyle().ItemSpacing.x;
  const float buttonWidth =
      (ImGui::GetContentRegionAvail().x - gap * (PRESETS.size() - 1)) /
      static_cast<float>(PRESETS.size());
  for (size_t index = 0; index < PRESETS.size(); ++index)
  {
    if (index > 0) ImGui::SameLine();
    ImGui::PushID(static_cast<int>(index));
    const bool active = selected_preset == static_cast<int>(index);
    if (active)
      ImGui::PushStyleColor(ImGuiCol_Button,
                            ImGui::GetStyleColorVec4(ImGuiCol_Header));
    if (ImGui::Button(
            LOC(PRESETS[index].nameKey),
            ImVec2(buttonWidth, PRESET_BUTTON_HEIGHT * Theme::scale())))
    {
      current_sliders = PRESETS[index].sliders;
      selected_preset = static_cast<int>(index);
    }
    if (active) ImGui::PopStyleColor();
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
  if (ImGui::Button(LOC("TACTIC_REVERT"))) loadStrategy();
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
  const float height =
      std::max(ImGui::GetContentRegionAvail().y, 260.0f * Theme::scale());
  UI::beginCard("tactic_summary", LOC("TACTIC_SUMMARY"), ImVec2(width, height));
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
