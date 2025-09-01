// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "settings_scene.h"
#include "main_menu_scene.h"
#include "settings_manager.h"
#include "view/gui/gui_view.h"
#include <SDL3_ttf/SDL_ttf.h>
#include <iostream>

SceneID SettingsScene::getID() const { return SceneID::SETTINGS; }

SettingsScene::SettingsScene(GUIView* guiView) :
  GUIScene(guiView), font(nullptr), fullscreenButtonId(-1)
{}

SettingsScene::~SettingsScene() { 
  if (font) {
    TTF_CloseFont(font);
    font = nullptr;
  }
}

void SettingsScene::onEnter() {
  if (!TTF_WasInit() && !TTF_Init()) {
    std::cerr << "TTF init failed\n";
  }

  font = TTF_OpenFont("assets/fonts/font.ttf", 28);
  if (!font) {
    std::cerr << "Font load failed\n";
  }

  // Show current state
  initializeCurrentSelections();

  buttonManager = std::make_unique<ButtonManager>(getRenderer(), font);

  // Define button layout parameters
  float btnY = 180.0f;
  float btnPaddingX = 30.0f;
  float btnPaddingY = 60.0f;

  // Language buttons
  float langX = 250.0f;
  float langWidth = 120.0f;
  float langHeight = 40.0f;
  int langIndex = 0;
  for (const auto& [lang, str] : languageToString) {
    ButtonStyle style = (langIndex == selectedLanguage) ? getSelectedButtonStyle() 
                      : getDefaultButtonStyle();

    int buttonId = buttonManager->addButton({langX, btnY, langWidth, langHeight},
                                            str, style, [this, langIndex]() {
                                            updateLanguageSelection(langIndex); });

    languageButtonIds.push_back(buttonId);
    langX += langWidth + btnPaddingX;
    langIndex++;
  }

  // FPS buttons
  btnY += btnPaddingY;
  float fpsX = 250.0f;
  float fpsWidth = 100.0f;
  float fpsHeight = 40.0f;
  for (size_t i = 0; i < fpsOptions.size(); i++) {
    const std::string fpsText = std::to_string(fpsOptions[i]);
    ButtonStyle style = (i == (size_t)selectedFPS) ? getSelectedButtonStyle() : getDefaultButtonStyle();

    int buttonId = buttonManager->addButton({fpsX, btnY, fpsWidth, fpsHeight}, fpsText, style, [this, i]() {
      updateFPSSelection(i);
    });

    fpsButtonIds.push_back(buttonId);
    fpsX += fpsWidth + btnPaddingX;
  }

  // Resolution buttons
  btnY += btnPaddingY;
  float resX = 250.0f;
  float resWidth = 160.0f;
  float resHeight = 40.0f;
  for (size_t i = 0; i < resolutions.size(); i++) {
    const std::string resText = std::to_string(resolutions[i].first) + "x" + std::to_string(resolutions[i].second);
    ButtonStyle style = (i == (size_t)selectedResolution) ? getSelectedButtonStyle() : getDefaultButtonStyle();

    int buttonId = buttonManager->addButton({resX, btnY, resWidth, resHeight}, 
                                            resText, style, [this, i]() {
      updateResolutionSelection(i);
    });

    resolutionButtonIds.push_back(buttonId);
    resX += resWidth + btnPaddingX;
  }

  // Fullscreen toggle
  btnY += btnPaddingY;
  std::string fullscreenText = fullscreen ? "Fullscreen: ON" : "Fullscreen: OFF";
  ButtonStyle fullscreenStyle = fullscreen ? getSelectedButtonStyle() : getDefaultButtonStyle();

  fullscreenButtonId = buttonManager->addButton({250, btnY, 200, 40},
                                      fullscreenText, fullscreenStyle, [this]() {
    updateFullscreenToggle();
  });

  // Back/Apply button
  btnY += btnPaddingY;
  buttonManager->addButton({250, btnY, 200, 40}, "Apply & Back", [this]() {
    applyAndSaveSettings();
  });
}

void SettingsScene::initializeCurrentSelections() {
  SettingsManager* settingsManager = SettingsManager::instance();
  const auto& settings = settingsManager->getSettings();

  // Find current language index
  auto itLang = std::find_if(languageToString.begin(), languageToString.end(),
      [&](const auto& kv) { return static_cast<int>(kv.first) == settings.language; });

  selectedLanguage = (itLang != languageToString.end())
                       ? std::distance(languageToString.begin(), itLang)
                       : 0;

  // Find current FPS index
  auto itFps = std::find(fpsOptions.begin(), fpsOptions.end(), settings.fps_limit);
  selectedFPS = (itFps != fpsOptions.end()) ? std::distance(fpsOptions.begin(), itFps) : 0;

  // Find current resolution index
  auto itRes = std::find_if(resolutions.begin(), resolutions.end(),
                          [&](const auto& r) {
                          return r.first == settings.resolution_width &&
                          r.second == settings.resolution_height;
                          });
  selectedResolution = (itRes != resolutions.end()) ? std::distance(resolutions.begin(), itRes) : 0;

  fullscreen = settings.fullscreen;
}

void SettingsScene::updateLanguageSelection(int newSelection) {
  if (newSelection == selectedLanguage) return;

  // Update visual selection
  for (size_t i = 0; i < languageButtonIds.size(); i++) {
    ButtonStyle style = (i == (size_t)newSelection) ? getSelectedButtonStyle() : getDefaultButtonStyle();
    buttonManager->setButtonStyle(languageButtonIds[i], style);
  }
  selectedLanguage = newSelection;
}

void SettingsScene::updateFPSSelection(int newSelection) {
  if (newSelection == selectedFPS) return;

  for (size_t i = 0; i < fpsButtonIds.size(); i++) {
    ButtonStyle style = (i == (size_t)newSelection) ? getSelectedButtonStyle() : getDefaultButtonStyle();
    buttonManager->setButtonStyle(fpsButtonIds[i], style);
  }
  selectedFPS = newSelection;
}

void SettingsScene::updateResolutionSelection(int newSelection) {
  if (newSelection == selectedResolution) return;

  for (size_t i = 0; i < resolutionButtonIds.size(); i++) {
    ButtonStyle style = (i == (size_t)newSelection) ? getSelectedButtonStyle() : getDefaultButtonStyle();
    buttonManager->setButtonStyle(resolutionButtonIds[i], style);
  }
  selectedResolution = newSelection;
}

void SettingsScene::updateFullscreenToggle() {
  fullscreen = !fullscreen;
  std::string newText = fullscreen ? "Fullscreen: ON" : "Fullscreen: OFF";
  ButtonStyle newStyle = fullscreen ? getSelectedButtonStyle() : getDefaultButtonStyle();

  if (fullscreenButtonId != -1) {
    buttonManager->setButtonText(fullscreenButtonId, newText);
    buttonManager->setButtonStyle(fullscreenButtonId, newStyle);
  }
}

void SettingsScene::applyAndSaveSettings() {
  SettingsManager* settingsManager = SettingsManager::instance();
  auto& settings = settingsManager->getSettings();

  // Update language
  auto langIt = languageToString.begin();
  std::advance(langIt, selectedLanguage);
  settings.language = langIt->first;

  // Update FPS
  if (selectedFPS < (int)fpsOptions.size()) {
    settings.fps_limit = fpsOptions[selectedFPS];
  }

  // Update resolution
  if (selectedResolution < (int)resolutions.size()) {
    settings.resolution_width = resolutions[selectedResolution].first;
    settings.resolution_height = resolutions[selectedResolution].second;
  }

  settings.fullscreen = fullscreen;

  // Apply settings to window immediately
  settingsManager->apply(guiView->getWindow());

  // Save to JSON file
  settingsManager->save();

  // Return to main menu
  auto mainMenu = std::make_unique<MainMenuScene>(guiView);
  changeScene(std::move(mainMenu));
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
  style.backgroundColor = {80, 120, 80, 255}; // Green tint for selected
  style.textColor = {255, 255, 255, 255};
  style.borderColor = {120, 180, 120, 255};
  style.hasBorder = true;
  return style;
}

void SettingsScene::handleEvent(const SDL_Event& event) {
  if (event.type == SDL_EVENT_MOUSE_MOTION) {
    buttonManager->handleMouseMove(event.motion.x, event.motion.y);
  }
  if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
    if (buttonManager->handleMouseClick(event.button.x, event.button.y))
      return;
  }
  if (event.type == SDL_EVENT_KEY_DOWN && (event.key.key == SDLK_ESCAPE || event.key.key == SDLK_BACKSPACE)) {
    auto mainMenu = std::make_unique<MainMenuScene>(guiView);
    changeScene(std::move(mainMenu));
  }
}

void SettingsScene::update(float deltaTime) {
  (void)deltaTime; // Suppress unused parameter warning
}

void SettingsScene::render() {
  SDL_RenderClear(getRenderer());

  // Draw settings box
  SDL_FRect settingsBox = {200, 150, 400, 350};
  SDL_SetRenderDrawColor(getRenderer(), 60, 80, 100, 255);
  SDL_RenderFillRect(getRenderer(), &settingsBox);
  SDL_SetRenderDrawColor(getRenderer(), 100, 120, 140, 255);
  SDL_RenderRect(getRenderer(), &settingsBox);

  renderText("SETTINGS", 400, 170, {255, 255, 100, 255});

  if (buttonManager) buttonManager->render();
}

void SettingsScene::onExit() {
  if (font) {
    TTF_CloseFont(font);
    font = nullptr;
  }
}

void SettingsScene::renderText(const char* text, float x, float y, SDL_Color color) {
  if (!font) return;

  SDL_Surface* textSurface = TTF_RenderText_Solid(font, text, strlen(text), color);
  if (textSurface) {
    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(getRenderer(), textSurface);
    if (textTexture) {
      SDL_FRect textRect = {
        x - textSurface->w / 2.0f,
        y - textSurface->h / 2.0f,
        (float)textSurface->w,
        (float)textSurface->h
      };

      SDL_RenderTexture(getRenderer(), textTexture, nullptr, &textRect);
    }
    SDL_DestroySurface(textSurface);
  }
}
