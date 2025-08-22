#include "view/gui/button_manager.h"
#include "view/gui/scenes/main_menu_scene.h"
#include "view/gui/gui_scene.h"
#include "view/gui/gui_view.h"
#include "view/gui/scenes/settings_scene.h"
#include "view/gui/scenes/main_game_scene.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_video.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <iostream>

SceneID MainMenuScene::getID() const { return SceneID::MAIN_MENU; }

enum ButtonIndex {
  NEW_GAME_BUTTON = 0,
  LOAD_GAME_BUTTON = 1,
  SETTINGS_BUTTON = 2,
  QUIT_BUTTON = 3,
};

MainMenuScene::~MainMenuScene() = default;

MainMenuScene::MainMenuScene(GUIView* guiView)
    : GUIScene(guiView),
      font(nullptr),
  vButtons(30, 300, 200, 200, 60) // <-- initialize the member here
{
  vButtons.addButton("New game", [this]() { handleButtonClick(NEW_GAME_BUTTON); });
  vButtons.addButton("Load game", [this]() { handleButtonClick(LOAD_GAME_BUTTON); });
  vButtons.addButton("Settings", [this]() { handleButtonClick(SETTINGS_BUTTON); });
  vButtons.addButton("Quit game", [this]() { handleButtonClick(QUIT_BUTTON); });

  buttons = vButtons.getButtons();
}

void MainMenuScene::onEnter() {
  // Initialize TTF if not already done
  if (!TTF_WasInit()) {
    if (!TTF_Init()) {
      std::cerr << "Failed to initialize TTF: " << SDL_GetError() << std::endl;
      return;
    }
  }

  font = TTF_OpenFont("assets/fonts/font.ttf", 28);
  if (!font) {
    std::cerr << "Failed to load font: " << SDL_GetError() << std::endl;
    // TODO add a fallback? 
  }
}

void MainMenuScene::handleEvent(const SDL_Event& event) {
  (void) event;
  if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
    float mouseX = event.button.x;
    float mouseY = event.button.y;

    // Check which button was clicked
    for (size_t i = 0; i < buttons.size(); ++i) {
      if (isPointInButton(mouseX, mouseY, buttons[i])) {
        handleButtonClick(i);
        break;
      }
    }
  }

  // Handle keyboard shortcuts
  if (event.type == SDL_EVENT_KEY_DOWN) {
    switch (event.key.key) {
      case SDLK_ESCAPE:
        quit();
        break;
      case SDLK_RETURN: // This is an option for a shortcut
        handleButtonClick(LOAD_GAME_BUTTON);
        break;
    }
  }
}

void MainMenuScene::update(float deltaTime) {
  // Update any animations or hover effects here
  // For now, this can be empty
  (void)deltaTime; // Suppress unused parameter warning
}

void MainMenuScene::render() {
  // Clear screen with dark background
  SDL_SetRenderDrawColor(getRenderer(), 30, 30, 35, 255);
  SDL_RenderClear(getRenderer());

  // Draw title
  renderTitle();

  // Draw buttons
  for (const auto& btn : buttons) {
    renderButton(btn);
  }
}

void MainMenuScene::onExit() {
  if (font) {
    TTF_CloseFont(font);
    font = nullptr;
  }
}

// Helper methods
bool MainMenuScene::isPointInButton(float x, float y, const Button& button) {
  return (x >= button.rect.x && x <= button.rect.x + button.rect.w &&
  y >= button.rect.y && y <= button.rect.y + button.rect.h);
}

void MainMenuScene::handleButtonClick(int buttonIndex) {
  switch (buttonIndex) {
    case NEW_GAME_BUTTON: {
      auto gameScene = std::make_unique<MainGameScene>(guiView);
      changeScene(std::move(gameScene));
      break;
    }
    case LOAD_GAME_BUTTON: {
      break;
    }
    case SETTINGS_BUTTON: {
      auto settingsScene = std::make_unique<SettingsScene>(guiView);
      changeScene(std::move(settingsScene));
      break;
    }
    case QUIT_BUTTON:
      quit();
      break;
  }
}

void MainMenuScene::renderButton(const Button& btn) {
  // Button background with subtle gradient effect
  SDL_SetRenderDrawColor(getRenderer(), 70, 70, 150, 255);
  SDL_RenderFillRect(getRenderer(), &btn.rect);

  // Button border
  SDL_SetRenderDrawColor(getRenderer(), 100, 100, 200, 255);
  SDL_RenderRect(getRenderer(), &btn.rect);

  // Render button text
  if (font) {
    SDL_Color textColor = {255, 255, 255, 255};
    SDL_Surface* textSurface = TTF_RenderText_Solid(font, btn.label, strlen(btn.label), textColor);

    if (textSurface) {
      SDL_Texture* textTexture = SDL_CreateTextureFromSurface(getRenderer(), textSurface);

      if (textTexture) {
        // Center text in button
        SDL_FRect textRect = {
          btn.rect.x + (btn.rect.w - textSurface->w) / 2.0f,
          btn.rect.y + (btn.rect.h - textSurface->h) / 2.0f,
          (float)textSurface->w,
          (float)textSurface->h
        };

        SDL_RenderTexture(getRenderer(), textTexture, nullptr, &textRect);
        SDL_DestroyTexture(textTexture);
      }
      SDL_DestroySurface(textSurface);
    }
  }
}

void MainMenuScene::renderTitle() {
  if (!font) return;

  const char* title = "Football Management";
  SDL_Color titleColor = {255, 255, 100, 255}; // Yellow title

  SDL_Surface* titleSurface = TTF_RenderText_Solid(font, title, strlen(title), titleColor);
  if (titleSurface) {
    SDL_Texture* titleTexture = SDL_CreateTextureFromSurface(getRenderer(), titleSurface);
    if (titleTexture) {
      SDL_FRect titleRect = {
        400 - titleSurface->w / 2.0f,  // Center horizontally (assuming 800px window)
        100,  // Top of screen
        (float)titleSurface->w,
        (float)titleSurface->h
      };

      SDL_RenderTexture(getRenderer(), titleTexture, nullptr, &titleRect);
      SDL_DestroyTexture(titleTexture);
    }
    SDL_DestroySurface(titleSurface);
  }
}
