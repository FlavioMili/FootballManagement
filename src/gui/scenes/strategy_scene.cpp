// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "strategy_scene.h"
#include <imgui.h>
#include "global/language_manager.h"
#include "gui/gui_constants.h"
#include "gui/gui_view.h"

namespace
{
constexpr float WINDOW_WIDTH = 500.0f;
constexpr float WINDOW_HEIGHT = 400.0f;
}  // namespace

StrategyScene::StrategyScene(GUIView* parent) : GUIScene(parent) {}

void StrategyScene::onEnter()
{
  loadStrategy();
}

void StrategyScene::update(float deltaTime) { (void)deltaTime; }

void StrategyScene::render()
{
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(GUIConstants::CENTER_PIVOT, GUIConstants::CENTER_PIVOT));
  ImGui::SetNextWindowSize(ImVec2(WINDOW_WIDTH, WINDOW_HEIGHT), ImGuiCond_FirstUseEver);
  ImGui::Begin(LOC("STRATEGY_TITLE"), nullptr, ImGuiWindowFlags_NoCollapse);

  ImGui::SliderFloat(LOC("STRATEGY_PRESSING"), &current_sliders.pressing, 0.0f, 1.0f);
  ImGui::SliderFloat(LOC("STRATEGY_RISK_TAKING"), &current_sliders.riskTaking, 0.0f, 1.0f);
  ImGui::SliderFloat(LOC("STRATEGY_OFFENSIVE_BIAS"), &current_sliders.offensiveBias, 0.0f, 1.0f);
  ImGui::SliderFloat(LOC("STRATEGY_WIDTH_USAGE"), &current_sliders.widthUsage, 0.0f, 1.0f);
  ImGui::SliderFloat(LOC("STRATEGY_COMPACTNESS"), &current_sliders.compactness, 0.0f, 1.0f);

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  if (ImGui::Button(LOC("STRATEGY_BACK"), ImVec2(GUIConstants::BUTTON_WIDTH, GUIConstants::BUTTON_HEIGHT))) {
    guiView->popScene();
  }
  ImGui::SameLine();
  if (ImGui::Button(LOC("STRATEGY_APPLY"), ImVec2(GUIConstants::BUTTON_WIDTH, GUIConstants::BUTTON_HEIGHT))) {
    saveStrategy();
    guiView->popScene();
  }

  ImGui::End();
}

void StrategyScene::saveStrategy()
{
  auto managedTeamOpt = guiView->getController().getManagedTeam();
  if (managedTeamOpt.has_value()) {
    managedTeamOpt->get().getStrategy().setAllSliders(current_sliders);
    guiView->getController().saveGame();
  }
}

void StrategyScene::loadStrategy()
{
  auto managedTeamOpt = guiView->getController().getManagedTeam();
  if (managedTeamOpt.has_value()) {
    current_sliders = managedTeamOpt->get().getStrategy().getSliders();
  }
}

SceneID StrategyScene::getID() const { return SceneID::STRATEGY; }
