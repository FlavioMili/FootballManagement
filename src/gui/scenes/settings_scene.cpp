// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "settings_scene.h"
#include "global/paths.h"
#include "main_menu_scene.h"
#include "settings_manager.h"
#include "gui/gui_view.h"
#include "gui/dropdown.h"
#include <algorithm>
#include <iostream>

SceneID SettingsScene::getID() const { return SceneID::SETTINGS; }

SettingsScene::SettingsScene(GUIView* guiView_ptr)
: GUIScene(guiView_ptr), font(nullptr) {}

SettingsScene::~SettingsScene() {
  // unique_ptrs and onExit
  // will handle cleanup
}

void SettingsScene::onEnter() {
  if (!TTF_WasInit() && TTF_Init() != 0) {
    std::cerr << "TTF_Init() failed: " << SDL_GetError() << "\n";
    return;
  }

  font = TTF_OpenFont(FONT_PATH, 24);
  if (!font) {
    std::cerr << "Failed to load font: " << SDL_GetError() << "\n";
    return;
  }

  buttonManager = std::make_unique<ButtonManager>(getRenderer(), font);

for (const auto &[lang, str] : languageToString) {
    languageOptions.push_back(str);
  }
  for(const auto& res : resolutions) { 
    resolutionOptions.push_back(std::to_string(res.first) + "x" + std::to_string(res.second)); 
  }
  for(const auto& fps : fpsOptions) {
    fpsOptionsStrings.push_back(std::to_string(fps));
  }

  initializeCurrentSelections();
  createLabelTextures();

  float dropdownX = 350.0f;
  float dropdownY = 200.0f;
  float dropdownW = 300.0f;
  float dropdownH = 40.0f;
  float paddingY = 60.0f;

  languageDropdown = std::make_shared<Dropdown>(getRenderer(), font, 
                                                SDL_FRect{dropdownX, dropdownY, dropdownW, dropdownH}, languageOptions);
  languageDropdown->setOnSelectionChanged([this](int index, const std::string& value)
                                          { (void)value; selectedLanguage = index; });
  languageDropdown->setSelectedIndex(selectedLanguage);

  dropdownY += paddingY;
  resolutionDropdown = std::make_shared<Dropdown>(getRenderer(), font, 
                                                  SDL_FRect{dropdownX, dropdownY, dropdownW, dropdownH}, resolutionOptions);
  resolutionDropdown->setOnSelectionChanged([this](int index, const std::string& value)
                                            { (void)value; selectedResolution = index; });
  resolutionDropdown->setSelectedIndex(selectedResolution);

  dropdownY += paddingY;
  fpsDropdown = std::make_shared<Dropdown>(getRenderer(), font, 
                                           SDL_FRect{dropdownX, dropdownY, dropdownW, dropdownH}, fpsOptionsStrings);
  fpsDropdown->setOnSelectionChanged([this](int index, const std::string& value)
                                     { (void)value; selectedFPS = index; });
  fpsDropdown->setSelectedIndex(selectedFPS);

  dropdownY += paddingY + 10.0f;
  std::string fullscreenText = fullscreen ? "Fullscreen: ON" : "Fullscreen: OFF";
  fullscreenButtonId = buttonManager->addButton(dropdownX, dropdownY, dropdownW, dropdownH, fullscreenText, [this](){
    fullscreen = !fullscreen;
    buttonManager->setButtonText(fullscreenButtonId, fullscreen ? "Fullscreen: ON" : "Fullscreen: OFF");
    buttonManager->setButtonStyle(fullscreenButtonId, fullscreen ? getSelectedButtonStyle() : getDefaultButtonStyle());
  });
  buttonManager->setButtonStyle(fullscreenButtonId, fullscreen ? getSelectedButtonStyle() : getDefaultButtonStyle());

  dropdownY += paddingY;
  float cancelX = 250.0f;
  float buttonW = 200.0f;
  buttonManager->addButton(cancelX, dropdownY, buttonW, dropdownH, "Cancel", [this](){
    changeScene(std::make_unique<MainMenuScene>(guiView));
  });

  applyButtonId = buttonManager->addButton(cancelX + buttonW + 20, dropdownY, buttonW, dropdownH, "Apply & Back", [this](){
    applyAndSaveSettings();
  });
}

void SettingsScene::initializeCurrentSelections() {
  SettingsManager* settingsManager = SettingsManager::instance();
  const auto& settings = settingsManager->getSettings();

  auto itLang = std::find_if(languageToString.begin(), languageToString.end(),
                             [&](const auto& kv) { return kv.first == settings.language; });
  selectedLanguage = (itLang != languageToString.end()) ? static_cast<int>(std::distance(languageToString.begin(), itLang)) : 0;

  auto itFps = std::find(fpsOptions.begin(), fpsOptions.end(), settings.fps_limit);
  selectedFPS = (itFps != fpsOptions.end()) ? static_cast<int>(std::distance(fpsOptions.begin(), itFps)) : 0;

  auto itRes = std::find_if(resolutions.begin(), resolutions.end(),
                            [&](const auto& r) {
                            return r.first == settings.resolution_width && r.second == settings.resolution_height; 
                            });
  selectedResolution = (itRes != resolutions.end()) ? static_cast<int>(std::distance(resolutions.begin(), itRes)) : 0;

  fullscreen = settings.fullscreen;
}

void SettingsScene::applyAndSaveSettings() {
  SettingsManager* settingsManager = SettingsManager::instance();
  auto& settings = settingsManager->getSettings();

  auto langIt = languageToString.begin();
  std::advance(langIt, selectedLanguage);
  settings.language = langIt->first;

  if (selectedFPS >= 0 && selectedFPS < static_cast<int>(fpsOptions.size())) {
    settings.fps_limit = fpsOptions[static_cast<std::size_t>(selectedFPS)];
  }

  if (selectedResolution >= 0 && selectedResolution < static_cast<int>(resolutions.size())) {
    settings.resolution_width  = resolutions[static_cast<std::size_t>(selectedResolution)].first;
    settings.resolution_height = resolutions[static_cast<std::size_t>(selectedResolution)].second;
  }
  settings.fullscreen = fullscreen;

  settingsManager->apply(guiView->getWindow());
  settingsManager->save();

  changeScene(std::make_unique<MainMenuScene>(guiView));
}

void SettingsScene::handleEvent(const SDL_Event& event) {
  // Motion events are passed to both buttons and open dropdowns
  if (event.type == SDL_EVENT_MOUSE_MOTION) {
    if (buttonManager) {
      buttonManager->handleMouseMove(event.motion.x, event.motion.y);
    }
    if (activeDropdown && activeDropdown->isDropdownOpen()) {
      activeDropdown->handleEvent(event);
    }
    return;
  }

  // Click events require careful handling
  if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
    // If a dropdown is open, it gets exclusive priority
    if (activeDropdown && activeDropdown->isDropdownOpen()) {
      activeDropdown->handleEvent(event);
      // After handling, the dropdown might have closed.
      // If it's now closed, we check if it was because the user clicked outside.
      // The dropdown's handleEvent should return true if it handled the click (selection or closing).
      // This prevents the click from passing through to buttons underneath.
      if (!activeDropdown->isDropdownOpen()) {
        activeDropdown = nullptr;
      }
      return; // The click is consumed by the dropdown logic
    }

    // If no dropdown is open, check if a click opens one
    if (languageDropdown->handleEvent(event)) {
      activeDropdown = languageDropdown;
      resolutionDropdown->close();
      fpsDropdown->close();
      return;
    }
    if (resolutionDropdown->handleEvent(event)) {
      activeDropdown = resolutionDropdown;
      languageDropdown->close();
      fpsDropdown->close();
      return;
    }
    if (fpsDropdown->handleEvent(event)) {
      activeDropdown = fpsDropdown;
      languageDropdown->close();
      resolutionDropdown->close();
      return;
    }

    // If no dropdown was opened or interacted with, check buttons
    if (buttonManager) {
      buttonManager->handleMouseClick(event.button.x, event.button.y);
    }
  }

  if (event.type == SDL_EVENT_KEY_DOWN && (event.key.key == SDLK_ESCAPE || event.key.key == SDLK_BACKSPACE)) {
    changeScene(std::make_unique<MainMenuScene>(guiView));
  }
}

void SettingsScene::update(float deltaTime) {
  (void)deltaTime;
}

void SettingsScene::render() {
  SDL_SetRenderDrawColor(getRenderer(), 30, 30, 30, 255);
  SDL_RenderClear(getRenderer());

  if (titleTexture) { SDL_RenderTexture(getRenderer(), titleTexture, nullptr, &titleRect); }
  if (buttonManager) { buttonManager->render(); }

  if (langLabelTexture) { SDL_RenderTexture(getRenderer(), langLabelTexture, nullptr, &langLabelRect); }
  if (resLabelTexture) { SDL_RenderTexture(getRenderer(), resLabelTexture, nullptr, &resLabelRect); }
  if (fpsLabelTexture) { SDL_RenderTexture(getRenderer(), fpsLabelTexture, nullptr, &fpsLabelRect); }

  // Render non-active dropdowns first
  if (languageDropdown != activeDropdown) languageDropdown->render();
  if (resolutionDropdown != activeDropdown) resolutionDropdown->render();
  if (fpsDropdown != activeDropdown) fpsDropdown->render();

  // Render active dropdown last so it's on top
  if (activeDropdown) activeDropdown->render();
}

void SettingsScene::onExit() {
  freeLabelTextures();
  if (font) {
    TTF_CloseFont(font);
    font = nullptr;
  }
}

void SettingsScene::createLabelTextures() {
  SDL_Color color = {255, 255, 255, 255};
  float w, h;
  float yPos = 200.0f;

  // Title
  SDL_Surface* surf = TTF_RenderText_Blended(font, "Settings", 0, color);
  titleTexture = SDL_CreateTextureFromSurface(getRenderer(), surf);
  SDL_GetTextureSize(titleTexture, &w, &h);
  int win_w, win_h;
  SDL_GetWindowSizeInPixels(guiView->getWindow(), &win_w, &win_h);
  titleRect = { (static_cast<float>(win_w) / 2 - w / 2), 100.0f, w, h };
  SDL_DestroySurface(surf);

  // Labels
  surf = TTF_RenderText_Blended(font, "Language", 0, color);
  langLabelTexture = SDL_CreateTextureFromSurface(getRenderer(), surf);
  SDL_GetTextureSize(langLabelTexture, &w, &h);
  langLabelRect = { 200.0f, yPos + 10, w, h };
  SDL_DestroySurface(surf);

  yPos += 60.0f;
  surf = TTF_RenderText_Blended(font, "Resolution", 0, color);
  resLabelTexture = SDL_CreateTextureFromSurface(getRenderer(), surf);
  SDL_GetTextureSize(resLabelTexture, &w, &h);
  resLabelRect = { 200.0f, yPos + 10, w, h };
  SDL_DestroySurface(surf);

  yPos += 60.0f;
  surf = TTF_RenderText_Blended(font, "Refresh Rate", 0, color);
  fpsLabelTexture = SDL_CreateTextureFromSurface(getRenderer(), surf);
  SDL_GetTextureSize(fpsLabelTexture, &w, &h);
  fpsLabelRect = { 200.0f, yPos + 10, w, h };
  SDL_DestroySurface(surf);
}

void SettingsScene::freeLabelTextures() {
  if (titleTexture) { SDL_DestroyTexture(titleTexture); titleTexture = nullptr; }
  if (langLabelTexture) { SDL_DestroyTexture(langLabelTexture); langLabelTexture = nullptr; }
  if (resLabelTexture) { SDL_DestroyTexture(resLabelTexture); resLabelTexture = nullptr; }
  if (fpsLabelTexture) { SDL_DestroyTexture(fpsLabelTexture); fpsLabelTexture = nullptr; }
}

ButtonStyle SettingsScene::getDefaultButtonStyle() {
  ButtonStyle style;
  style.backgroundColor = {60, 60, 60, 255};
  style.textColor = {255, 255, 255, 255};
  style.borderColor = {100, 100, 100, 255};
  style.hasBorder = true;
  return style;
}

ButtonStyle SettingsScene::getSelectedButtonStyle() {
  ButtonStyle style;
  style.backgroundColor = {80, 120, 80, 255};
  style.textColor = {255, 255, 255, 255};
  style.borderColor = {120, 180, 120, 255};
  style.hasBorder = true;
  return style;
}
