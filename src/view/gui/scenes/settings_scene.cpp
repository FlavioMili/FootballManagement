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
  if (!TTF_WasInit() && !TTF_Init()) {
    std::cerr << "TTF init failed\n";
  }

  font = TTF_OpenFont("assets/fonts/font.ttf", 28);
  if (!font) {
    std::cerr << "Font load failed\n";
  }

  buttonManager = std::make_unique<ButtonManager>(getRenderer(), font);

  // Define button layout parameters
  float btnY = 180.0f; // Starting Y position for button rows
  float btnPaddingX = 30.0f; // Horizontal spacing between buttons
  float btnPaddingY = 60.0f; // Vertical spacing between button rows

  // Language buttons
  float langX = 250.0f; // Starting X position for the first button in this row
  float langWidth = 120.0f;
  float langHeight = 40.0f;
  int langIndex = 0;
  for (const auto& [lang, str] : languageToString) {
    buttonManager->addButton({langX, btnY, langWidth, langHeight}, str, [this, langIndex]() {
      selectedLanguage = langIndex;
      // TODO: highlight selected button
    });
    langX += langWidth + btnPaddingX;
    langIndex++;
  }

  // FPS buttons
  btnY += btnPaddingY; // Move down to the next row
  float fpsX = 250.0f;
  float fpsWidth = 100.0f;
  float fpsHeight = 40.0f;
  for (size_t i = 0; i < fpsOptions.size(); i++) {
    const std::string fpsText = std::to_string(fpsOptions[i]);
    buttonManager->addButton({fpsX, btnY, fpsWidth, fpsHeight}, fpsText, [this, i]() {
      selectedFPS = i;
      // TODO: Update visual selection
    });
    fpsX += fpsWidth + btnPaddingX;
  }

  // Resolution buttons
  btnY += btnPaddingY; // Move down to the next row
  float resX = 250.0f;
  float resWidth = 160.0f;
  float resHeight = 40.0f;
  for (size_t i = 0; i < resolutions.size(); i++) {
    const std::string resText = std::to_string(resolutions[i].first) + "x" + std::to_string(resolutions[i].second);
    buttonManager->addButton({resX, btnY, resWidth, resHeight}, resText, [this, i]() {
      selectedResolution = i;
    });
    resX += resWidth + btnPaddingX;
  }

  // Fullscreen toggle
  btnY += btnPaddingY;
  buttonManager->addButton({250, btnY, 200, 40}, "Toggle Fullscreen", [this]() {
    fullscreen = !fullscreen;
  });

  // Back/Apply button
  btnY += btnPaddingY;
  buttonManager->addButton({250, btnY, 200, 40}, "Apply & Back", [this]() {
    // Apply settings to SDL window
    SDL_Window* window = guiView->getWindow();
    if (window && (size_t)selectedResolution < resolutions.size()) {
      auto [w, h] = resolutions[selectedResolution];
      SDL_SetWindowSize(window, w, h);
      SDL_SetWindowFullscreen(window, fullscreen);
      SDL_SetWindowTitle(window, "Football Management");
      SDL_SetWindowResizable(window, true);
      // TODO: apply FPS & language through SettingsManager
    }

    // Return to main menu
    auto mainMenu = std::make_unique<MainMenuScene>(guiView);
    changeScene(std::move(mainMenu));
  });
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
  // TODO Placeholder for settings updates 
  (void)deltaTime; // Suppress unused parameter warning
}

void SettingsScene::render() {
  SDL_SetRenderDrawColor(getRenderer(), 20, 40, 60, 255);
  SDL_RenderClear(getRenderer());

  // Draw box
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
