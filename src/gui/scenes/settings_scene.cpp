// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "settings_scene.h"

#include <imgui.h>

#include <algorithm>

#include "global/paths.h"
#include "global/language_manager.h"
#include <filesystem>
#include "gui/gui_constants.h"
#include "gui/scenes/main_menu_scene.h"
#include "settings_manager.h"

SceneID SettingsScene::getID() const { return SceneID::SETTINGS; }

SettingsScene::SettingsScene(GUIView* guiView_ptr) : GUIScene(guiView_ptr) {}

void SettingsScene::onEnter()
{
  languageOptions.clear();
  availableLanguageEnums.clear();
  for (const auto& [lang, str] : languageToString)
  {
    std::string filePath = std::string(PROJECT_ROOT) + "assets/lang/" + str + ".json";
    if (std::filesystem::exists(filePath))
    {
      languageOptions.push_back(str);
      availableLanguageEnums.push_back(lang);
    }
  }
  for (const auto& res : GUIConstants::RESOLUTIONS)
  {
    resolutionOptions.push_back(std::to_string(res.width) + "x" +
                                std::to_string(res.height));
  }
  for (const auto& fps : GUIConstants::FPS_OPTIONS)
  {
    fpsOptionsStrings.push_back(std::to_string(fps));
  }

  SettingsManager* settingsManager = SettingsManager::instance();
  const auto& settings = settingsManager->getSettings();

  auto itLang = std::find(availableLanguageEnums.begin(),
                          availableLanguageEnums.end(), settings.language);
  selectedLanguage = (itLang != availableLanguageEnums.end())
                         ? static_cast<int>(std::distance(
                               availableLanguageEnums.begin(), itLang))
                         : 0;

  auto itFps = std::find(GUIConstants::FPS_OPTIONS.begin(),
                         GUIConstants::FPS_OPTIONS.end(), settings.fps_limit);
  selectedFPS = (itFps != GUIConstants::FPS_OPTIONS.end())
                    ? static_cast<int>(std::distance(
                          GUIConstants::FPS_OPTIONS.begin(), itFps))
                    : 0;

  auto itRes = std::find_if(GUIConstants::RESOLUTIONS.begin(),
                            GUIConstants::RESOLUTIONS.end(),
                            [&](const auto& r)
                            {
                              return r.width == settings.resolution_width &&
                                     r.height == settings.resolution_height;
                            });
  selectedResolution = (itRes != GUIConstants::RESOLUTIONS.end())
                           ? static_cast<int>(std::distance(
                                 GUIConstants::RESOLUTIONS.begin(), itRes))
                           : 0;

  fullscreen = settings.fullscreen;
}

void SettingsScene::update(float deltaTime) { (void)deltaTime; }

void SettingsScene::render()
{
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                          ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::Begin(
      "Settings", nullptr,
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize);

  ImGui::Text("%s", LOC("SETTINGS_TITLE"));
  ImGui::Separator();
  ImGui::Spacing();

  if (ImGui::BeginCombo(LOC("SETTINGS_LANGUAGE"), languageOptions[selectedLanguage].c_str()))
  {
    for (size_t i = 0; i < languageOptions.size(); i++)
    {
      bool is_selected = (static_cast<size_t>(selectedLanguage) == i);
      if (ImGui::Selectable(languageOptions[i].c_str(), is_selected))
        selectedLanguage = static_cast<int>(i);
      if (is_selected) ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }

  if (ImGui::BeginCombo(LOC("SETTINGS_RESOLUTION"),
                        resolutionOptions[selectedResolution].c_str()))
  {
    for (size_t i = 0; i < resolutionOptions.size(); i++)
    {
      bool is_selected = (static_cast<size_t>(selectedResolution) == i);
      if (ImGui::Selectable(resolutionOptions[i].c_str(), is_selected))
        selectedResolution = static_cast<int>(i);
      if (is_selected) ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }

  if (ImGui::BeginCombo(LOC("SETTINGS_REFRESH_RATE"), fpsOptionsStrings[selectedFPS].c_str()))
  {
    for (size_t i = 0; i < fpsOptionsStrings.size(); i++)
    {
      bool is_selected = (static_cast<size_t>(selectedFPS) == i);
      if (ImGui::Selectable(fpsOptionsStrings[i].c_str(), is_selected))
        selectedFPS = static_cast<int>(i);
      if (is_selected) ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }

  ImGui::Checkbox(LOC("SETTINGS_FULLSCREEN"), &fullscreen);

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  if (ImGui::Button(LOC("SETTINGS_CANCEL"), ImVec2(GUIConstants::BUTTON_WIDTH,
                                     GUIConstants::BUTTON_HEIGHT)))
  {
    changeScene(std::make_unique<MainMenuScene>(guiView));
  }
  ImGui::SameLine();
  if (ImGui::Button(LOC("SETTINGS_APPLY"), ImVec2(GUIConstants::BUTTON_WIDTH,
                                     GUIConstants::BUTTON_HEIGHT)))
  {
    applyAndSaveSettings();
  }

  ImGui::End();
}

void SettingsScene::applyAndSaveSettings()
{
  SettingsManager* settingsManager = SettingsManager::instance();
  auto& settings = settingsManager->getSettings();

  if (selectedLanguage >= 0 &&
      selectedLanguage < static_cast<int>(availableLanguageEnums.size()))
  {
    settings.language =
        availableLanguageEnums[static_cast<std::size_t>(selectedLanguage)];
  }

  if (selectedFPS >= 0 &&
      selectedFPS < static_cast<int>(GUIConstants::FPS_OPTIONS.size()))
  {
    settings.fps_limit =
        GUIConstants::FPS_OPTIONS[static_cast<std::size_t>(selectedFPS)];
  }

  if (selectedResolution >= 0 &&
      selectedResolution < static_cast<int>(GUIConstants::RESOLUTIONS.size()))
  {
    settings.resolution_width =
        GUIConstants::RESOLUTIONS[static_cast<std::size_t>(selectedResolution)]
            .width;
    settings.resolution_height =
        GUIConstants::RESOLUTIONS[static_cast<std::size_t>(selectedResolution)]
            .height;
  }
  settings.fullscreen = fullscreen;

  settingsManager->apply(guiView->getWindow());
  settingsManager->save();

  changeScene(std::make_unique<MainMenuScene>(guiView));
}
