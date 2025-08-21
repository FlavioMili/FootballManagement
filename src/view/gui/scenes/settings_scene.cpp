#include "settings_scene.h"
#include "main_menu_scene.h"
#include "view/gui/gui_view.h"
#include <SDL3_ttf/SDL_ttf.h>
#include <iostream>

SceneID SettingsScene::getID() const { return SceneID::SETTINGS; }

SettingsScene::SettingsScene(GUIView* guiView) : GUIScene(guiView), font(nullptr) {
}

SettingsScene::~SettingsScene() {
}

void SettingsScene::onEnter() {
  if (!TTF_WasInit()) {
    if (!TTF_Init()) {
      std::cerr << "Failed to initialize TTF: " << SDL_GetError() << std::endl;
      return;
    }
  }

  font = TTF_OpenFont("assets/fonts/font.ttf", 32);
  if (!font) {
    std::cerr << "Failed to load font: " << SDL_GetError() << std::endl;
  }
}

void SettingsScene::handleEvent(const SDL_Event& event) {
  if (event.type == SDL_EVENT_KEY_DOWN) {
    switch (event.key.key) {
      case SDLK_ESCAPE:
      case SDLK_BACKSPACE: {
        // Return to main menu
        auto mainMenu = std::make_unique<MainMenuScene>(guiView);
        changeScene(std::move(mainMenu));
        break;
      }
    }
  }

  if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
    // For now, any click returns to main menu
    auto mainMenu = std::make_unique<MainMenuScene>(guiView);
    changeScene(std::move(mainMenu));
  }
}

void SettingsScene::update(float deltaTime) {
  // Placeholder for settings updates
  if (deltaTime < -1) return; // avoiding warnings
}

void SettingsScene::render() {
  // Clear screen with different color to show we're in settings
  SDL_SetRenderDrawColor(getRenderer(), 20, 40, 60, 255);
  SDL_RenderClear(getRenderer());

  // Draw settings box
  SDL_FRect settingsBox = {200, 150, 400, 300};
  SDL_SetRenderDrawColor(getRenderer(), 60, 80, 100, 255);
  SDL_RenderFillRect(getRenderer(), &settingsBox);

  // Draw border
  SDL_SetRenderDrawColor(getRenderer(), 100, 120, 140, 255);
  SDL_RenderRect(getRenderer(), &settingsBox);

  // Draw scene title
  renderText("SETTINGS SCENE", 400, 200, {255, 255, 100, 255});

  // Draw instructions
  renderText("Press ESC or click to return", 400, 350, {200, 200, 200, 255});
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
        x - textSurface->w / 2.0f,  // Center horizontally
        y - textSurface->h / 2.0f,  // Center vertically
        (float)textSurface->w,
        (float)textSurface->h
      };

      SDL_RenderTexture(getRenderer(), textTexture, nullptr, &textRect);
      SDL_DestroyTexture(textTexture);
    }
    SDL_DestroySurface(textSurface);
  }
}

