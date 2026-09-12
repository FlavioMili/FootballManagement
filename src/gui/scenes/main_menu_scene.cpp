// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "gui/scenes/main_menu_scene.h"

#include <fmt/printf.h>
#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <format>
#include <string>

#include "global/language_manager.h"
#include "global/logger.h"
#include "gui/gui_view.h"
#include "gui/scenes/main_game_scene.h"
#include "gui/scenes/settings_scene.h"
#include "gui/scenes/team_selection_scene.h"
#include "gui/widgets/theme.h"
#include "gui/widgets/widgets.h"

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
    if (!loading_operation_started)
    {
      const int slot = loading_slot;
      const bool createNewGame = is_new_game;
      GameController* controller = &guiView->getController();
      loading_operation = std::async(std::launch::async,
                                     [controller, slot, createNewGame]()
                                     {
                                       if (createNewGame)
                                       {
                                         controller->newGame(slot);
                                         return true;
                                       }
                                       return controller->loadGame(slot);
                                     });
      loading_operation_started = true;
      return;
    }

    if (loading_operation.wait_for(std::chrono::seconds::zero()) !=
        std::future_status::ready)
    {
      return;
    }

    bool succeeded = false;
    try
    {
      succeeded = loading_operation.get();
    }
    catch (const std::exception& exception)
    {
      Logger::error(std::format("Failed to initialize slot {}: {}",
                                loading_slot, exception.what()));
    }
    loading_operation_started = false;

    if (succeeded)
    {
      auto gameScene = std::make_unique<MainGameScene>(guiView);
      changeScene(std::move(gameScene));
    }
    else
    {
      Logger::error(
          std::format("Failed to load game from slot {}", loading_slot));
      loading_slot = 0;
    }
  }
}

void MainMenuScene::render()
{
  const float dpi = ImGui::GetStyle().FontScaleDpi;
  const Theme::Palette& palette = Theme::palette();
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::Begin("##main_menu", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoBringToFrontOnFocus);
  ImGui::PopStyleVar(2);

  // Subtle pitch-line motif behind the menu.
  ImDrawList* background = ImGui::GetWindowDrawList();
  const ImVec2 center = viewport->GetCenter();
  const float radius =
      std::min(viewport->WorkSize.x, viewport->WorkSize.y) * 0.34f;
  background->AddCircle(center, radius, Theme::toU32(palette.border, 0.6f), 96,
                        2.0f * dpi);
  background->AddLine(
      ImVec2(center.x, viewport->WorkPos.y),
      ImVec2(center.x, viewport->WorkPos.y + viewport->WorkSize.y),
      Theme::toU32(palette.border, 0.45f), 2.0f * dpi);

  const float buttonWidth = 320.0f * dpi;
  const float buttonHeight = 46.0f * dpi;
  if (loading_slot > 0)
  {
    {
      const char* message =
          LOC(is_new_game ? "MENU_LOADING_INITIALIZING" : "MENU_LOADING_LOAD");
      Theme::ScopedText title(Theme::Text::TITLE);
      const ImVec2 size = ImGui::CalcTextSize(message);
      ImGui::SetCursorScreenPos(
          ImVec2(center.x - size.x * 0.5f, center.y - size.y * 0.5f));
      ImGui::TextUnformatted(message);
    }
    ImGui::End();
    is_loading_rendered = true;
    return;
  }

  {
    ImGui::PushFont(nullptr, Theme::textSize(Theme::Text::DISPLAY) * 1.5f);
    const char* title = LOC("MENU_TITLE");
    const ImVec2 size = ImGui::CalcTextSize(title);
    ImGui::SetCursorScreenPos(
        ImVec2(center.x - size.x * 0.5f, center.y - 190.0f * dpi));
    ImGui::TextUnformatted(title);
    ImGui::PopFont();
  }
  {
    Theme::ScopedText small(Theme::Text::BODY);
    const char* tagline = LOC("MENU_TAGLINE");
    const ImVec2 size = ImGui::CalcTextSize(tagline);
    ImGui::SetCursorScreenPos(
        ImVec2(center.x - size.x * 0.5f, ImGui::GetCursorScreenPos().y));
    ImGui::TextColored(palette.muted, "%s", tagline);
  }

  ImGui::SetCursorScreenPos(
      ImVec2(center.x - buttonWidth * 0.5f, center.y - 70.0f * dpi));
  ImGui::BeginGroup();
  if (UI::primaryButton(LOC("MENU_NEW_GAME"),
                        ImVec2(buttonWidth, buttonHeight)))
  {
    ImGui::OpenPopup("###select_save_slot");
    is_new_game = true;
  }
  if (ImGui::Button(LOC("MENU_LOAD_GAME"), ImVec2(buttonWidth, buttonHeight)))
  {
    ImGui::OpenPopup("###select_save_slot");
    is_new_game = false;
  }
  if (ImGui::Button(LOC("MENU_SETTINGS"), ImVec2(buttonWidth, buttonHeight)))
  {
    auto settingsScene = std::make_unique<SettingsScene>(guiView);
    changeScene(std::move(settingsScene));
  }
  if (ImGui::Button(LOC("MENU_QUIT"), ImVec2(buttonWidth, buttonHeight)))
  {
    quit();
  }
  ImGui::EndGroup();

  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  const std::string popupTitle =
      std::string(LOC(is_new_game ? "MENU_NEW_GAME" : "MENU_LOAD_GAME")) +
      "###select_save_slot";
  if (ImGui::BeginPopupModal(popupTitle.c_str(), nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize))
  {
    ImGui::TextColored(
        palette.muted, "%s",
        LOC(is_new_game ? "MENU_SLOT_PROMPT_NEW" : "MENU_SLOT_PROMPT_LOAD"));
    ImGui::Spacing();

    const ImVec2 slotSize(440.0f * dpi, 58.0f * dpi);
    for (int i = 1; i <= 3; ++i)
    {
      const auto& metadata = (cached_metadata.size() >= static_cast<size_t>(i))
                                 ? cached_metadata[static_cast<size_t>(i - 1)]
                                 : GameController::SaveSlotMetadata{};
      std::string detail;
      if (!metadata.exists)
        detail = LOC("MENU_SAVE_SLOT_EMPTY");
      else if (metadata.team_name.empty())
        detail = LOC("MENU_SAVE_SLOT_NOT_STARTED");
      else
      {
        detail = metadata.team_name;
        if (!metadata.game_date.empty()) detail += "  ·  " + metadata.game_date;
      }

      const bool disableButton = !is_new_game && !metadata.exists;
      ImGui::BeginDisabled(disableButton);
      ImGui::PushID(i);
      const ImVec2 slotStart = ImGui::GetCursorScreenPos();
      if (ImGui::Button("##slot", slotSize))
      {
        loading_slot = i;
        is_loading_rendered = false;
        ImGui::CloseCurrentPopup();
      }
      const bool hovered = ImGui::IsItemHovered();
      ImGui::PopID();
      ImDrawList* drawList = ImGui::GetWindowDrawList();
      const std::string slotLabel = fmt::sprintf(LOC("MENU_SLOT_LABEL"), i);
      drawList->AddText(
          ImVec2(slotStart.x + 14.0f * dpi, slotStart.y + 8.0f * dpi),
          Theme::toU32(palette.text), slotLabel.c_str());
      drawList->AddText(ImVec2(slotStart.x + 14.0f * dpi,
                               slotStart.y + slotSize.y -
                                   ImGui::GetTextLineHeight() - 8.0f * dpi),
                        Theme::toU32(palette.muted), detail.c_str());
      ImGui::EndDisabled();

      if (metadata.exists && !metadata.real_date.empty() && hovered)
      {
        const std::string tooltip =
            fmt::sprintf(LOC("MENU_SAVE_SLOT_LAST_SAVED"), metadata.real_date);
        ImGui::SetTooltip("%s", tooltip.c_str());
      }
    }

    ImGui::Spacing();
    if (ImGui::Button(LOC("SETTINGS_CANCEL"), ImVec2(slotSize.x, 0.0f)))
    {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  ImGui::End();
}
