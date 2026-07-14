// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once
#include <array>
#include <string>
#include <utility>
#include <vector>

#include "gui/gui_scene.h"
#include "gui/gui_view.h"

/**
 * @brief Scene for managing application settings.
 */
class SettingsScene : public GUIScene
{
 public:
  /**
   * @brief Constructs a new SettingsScene.
   * @param guiView_ptr Pointer to the GUIView.
   */
  explicit SettingsScene(GUIView* guiView_ptr);

  /**
   * @brief Destroys the SettingsScene.
   */
  ~SettingsScene() = default;

  /**
   * @brief Updates scene logic.
   * @param deltaTime Time elapsed since last update.
   */
  void update(float deltaTime) override;

  /**
   * @brief Renders the scene.
   */
  void render() override;

  /**
   * @brief Called when entering the scene.
   */
  void onEnter() override;

  /**
   * @brief Gets the ID of this scene.
   * @return The SceneID (SETTINGS).
   */
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
