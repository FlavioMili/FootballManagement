// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "gui/scenes/main_menu_scene.h"

#include <imgui.h>

#include "global/language_manager.h"
#include "global/logger.h"
#include "gui/gui_constants.h"
#include "gui/gui_view.h"
#include "gui/scenes/main_game_scene.h"
#include "gui/scenes/settings_scene.h"
#include "gui/scenes/team_selection_scene.h"

SceneID MainMenuScene::getID() const { return SceneID::MAIN_MENU; }

MainMenuScene::MainMenuScene(GUIView* guiView_ptr) : GUIScene(guiView_ptr) {}

void MainMenuScene::update(float deltaTime) { (void)deltaTime; }

void MainMenuScene::render()
{
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                          ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::Begin("Football Management", nullptr,
               ImGuiWindowFlags_NoDecoration |
                   ImGuiWindowFlags_AlwaysAutoResize |
                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove);

  ImGui::Text("%s", LOC("MENU_TITLE"));
  ImGui::Separator();
  ImGui::Spacing();

  if (ImGui::Button(LOC("MENU_NEW_GAME"),
                    ImVec2(GUIConstants::MENU_BUTTON_WIDTH,
                           GUIConstants::MENU_BUTTON_HEIGHT)))
  {
    ImGui::OpenPopup("Select Save Slot");
    is_new_game = true;
  }

  if (ImGui::Button(LOC("MENU_LOAD_GAME"),
                    ImVec2(GUIConstants::MENU_BUTTON_WIDTH,
                           GUIConstants::MENU_BUTTON_HEIGHT)))
  {
    ImGui::OpenPopup("Select Save Slot");
    is_new_game = false;
  }

  if (ImGui::BeginPopupModal("Select Save Slot", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize))
  {
    ImGui::Text("%s", is_new_game
                          ? "Select a slot for your New Game (will overwrite):"
                          : "Select a slot to Load:");
    ImGui::Separator();

    for (int i = 1; i <= 3; ++i)
    {
      if (ImGui::Button(("Slot " + std::to_string(i)).c_str(), ImVec2(200, 40)))
      {
        if (is_new_game)
        {
          guiView->getController().newGame(i);
          auto gameScene = std::make_unique<MainGameScene>(guiView);
          changeScene(std::move(gameScene));
        }
        else
        {
          if (guiView->getController().loadGame(i))
          {
            auto gameScene = std::make_unique<MainGameScene>(guiView);
            changeScene(std::move(gameScene));
          }
          else
          {
            // Failed to load
            Logger::error("Failed to load game from slot " + std::to_string(i));
          }
        }
        ImGui::CloseCurrentPopup();
      }
    }

    ImGui::Separator();
    if (ImGui::Button("Cancel", ImVec2(200, 30)))
    {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (ImGui::Button(LOC("MENU_SETTINGS"),
                    ImVec2(GUIConstants::MENU_BUTTON_WIDTH,
                           GUIConstants::MENU_BUTTON_HEIGHT)))
  {
    auto settingsScene = std::make_unique<SettingsScene>(guiView);
    changeScene(std::move(settingsScene));
  }

  if (ImGui::Button(LOC("MENU_QUIT"), ImVec2(GUIConstants::MENU_BUTTON_WIDTH,
                                             GUIConstants::MENU_BUTTON_HEIGHT)))
  {
    quit();
  }

  ImGui::End();
}
