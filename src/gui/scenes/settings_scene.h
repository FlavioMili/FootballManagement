// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once
#include "gui/gui_scene.h"
#include "gui/gui_view.h"
#include <string>
#include <vector>
#include <utility>
#include <array>

class SettingsScene : public GUIScene
{
 public:
  explicit SettingsScene(GUIView* guiView_ptr);
  ~SettingsScene() = default;

  void update(float deltaTime) override;
  void render() override;
  void onEnter() override;
  SceneID getID() const override;

 private:
  void applyAndSaveSettings();

  std::vector<std::string> languageOptions;
  std::vector<Language> availableLanguageEnums;
  std::vector<std::string> resolutionOptions;
  std::vector<std::string> fpsOptionsStrings;

  int selectedLanguage = 0;
  int selectedFPS = 0;
  int selectedResolution = 0;
  bool fullscreen = false;
};
