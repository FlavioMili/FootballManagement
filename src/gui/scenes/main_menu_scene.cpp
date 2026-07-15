// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "gui/scenes/main_menu_scene.h"

#include <imgui.h>

#include <format>

#include "global/language_manager.h"
#include "global/logger.h"
#include "gui/gui_constants.h"
#include "gui/gui_view.h"
#include "gui/scenes/main_game_scene.h"
#include "gui/scenes/settings_scene.h"
#include "gui/scenes/team_selection_scene.h"

SceneID MainMenuScene::getID() const { return SceneID::MAIN_MENU; }

MainMenuScene::MainMenuScene(GUIView* guiView_ptr) : GUIScene(guiView_ptr) {}

void MainMenuScene::onEnter() { loadCachedMetadata(); }

void MainMenuScene::loadCachedMetadata()
{
  cached_metadata.clear();
  for (int i = 1; i <= 3; ++i)
  {
    cached_metadata.push_back(guiView->getController().getSaveSlotMetadata(i));
  }
}

void MainMenuScene::update(float deltaTime)
{
  (void)deltaTime;

  if (loading_slot > 0 && is_loading_rendered)
  {
    if (is_new_game)
    {
      guiView->getController().newGame(loading_slot);
      auto gameScene = std::make_unique<MainGameScene>(guiView);
      changeScene(std::move(gameScene));
    }
    else
    {
      if (guiView->getController().loadGame(loading_slot))
      {
        auto gameScene = std::make_unique<MainGameScene>(guiView);
        changeScene(std::move(gameScene));
      }
      else
      {
        Logger::error(
            std::format("Failed to load game from slot {}", loading_slot));
        loading_slot = 0;  // reset
      }
    }
  }
}

void MainMenuScene::render()
{
  if (loading_slot > 0)
  {
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::Begin(
        "Loading", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove);
    ImGui::Text("%s...", LOC(is_new_game ? "MENU_LOADING_INITIALIZING"
                                         : "MENU_LOADING_LOAD"));
    ImGui::End();
    is_loading_rendered = true;
    return;
  }

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
      const auto& metadata = (cached_metadata.size() >= static_cast<size_t>(i))
                                 ? cached_metadata[static_cast<size_t>(i - 1)]
                                 : GameController::SaveSlotMetadata{};
      std::string btn_label = std::format("Slot {}: ", i);
      if (!metadata.exists)
      {
        btn_label += LOC("MENU_SAVE_SLOT_EMPTY");
      }
      else
      {
        if (metadata.team_name.empty())
        {
          btn_label += LOC("MENU_SAVE_SLOT_NOT_STARTED");
        }
        else
        {
          btn_label += metadata.team_name;
          if (!metadata.game_date.empty())
          {
            btn_label += " (" + metadata.game_date + ")";
          }
        }
      }

      bool disable_button = !is_new_game && !metadata.exists;
      if (disable_button)
      {
        ImGui::BeginDisabled();
      }

      if (ImGui::Button(btn_label.c_str(), ImVec2(400, 40)))
      {
        loading_slot = i;
        is_loading_rendered = false;
        ImGui::CloseCurrentPopup();
      }

      if (disable_button)
      {
        ImGui::EndDisabled();
      }

      if (metadata.exists && !metadata.real_date.empty() &&
          ImGui::IsItemHovered())
      {
        ImGui::SetTooltip(LOC("MENU_SAVE_SLOT_LAST_SAVED"),
                          metadata.real_date.c_str());
      }
    }

    ImGui::Separator();
    if (ImGui::Button(LOC("SETTINGS_CANCEL"), ImVec2(400, 30)))
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
