// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once
#include "gui/button_manager.h"
#include "gui/gui_scene.h"
#include "gui/gui_view.h"
#include "gui/dropdown.h"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <array>
#include <utility>
#include <memory>
#include <vector>

class SettingsScene : public GUIScene {
 public:
  explicit SettingsScene(GUIView* guiView_ptr);
  ~SettingsScene();
  void handleEvent(const SDL_Event& event) override;
  void update(float deltaTime) override;
  void render() override;
  void onEnter() override;
  void onExit() override;
  SceneID getID() const override;

 private:
  TTF_Font* font;
  std::unique_ptr<ButtonManager> buttonManager;

  // New dropdowns
  std::shared_ptr<Dropdown> languageDropdown;
  std::shared_ptr<Dropdown> resolutionDropdown;
  std::shared_ptr<Dropdown> fpsDropdown;
  std::shared_ptr<Dropdown> activeDropdown = nullptr;

  // Options
  std::vector<std::string> languageOptions;
  std::vector<std::string> resolutionOptions;
  std::vector<std::string> fpsOptionsStrings;
  std::array<int, 4> fpsOptions = {60, 90, 120, 144};
  std::array<std::pair<int, int>, 4> resolutions = {{ {1280, 720}, {1920, 1080}, {2560, 1440}, {3840, 2160} }};

  // Current selections
  int selectedLanguage = 0;
  int selectedFPS = 0;
  int selectedResolution = 0;
  bool fullscreen = false;

  int fullscreenButtonId = -1;
  int applyButtonId = -1;

  // Helper methods
  void initializeCurrentSelections();
  void applyAndSaveSettings();
  void createLabelTextures();
  void freeLabelTextures();

  ButtonStyle getDefaultButtonStyle();
  ButtonStyle getSelectedButtonStyle();

  // Cached Textures
  SDL_Texture* titleTexture = nullptr;
  SDL_Texture* langLabelTexture = nullptr;
  SDL_Texture* resLabelTexture = nullptr;
  SDL_Texture* fpsLabelTexture = nullptr;
  SDL_FRect titleRect{}, langLabelRect{}, resLabelRect{}, fpsLabelRect{};
};
