#include "main_game_scene.h"
#include "main_menu_scene.h"
#include <SDL3_ttf/SDL_ttf.h>
#include <iostream>

SceneID MainGameScene::getID() const { return SceneID::GAME_MENU; }

MainGameScene::MainGameScene(GUIView* guiView) 
: GUIScene(guiView), font(nullptr), smallFont(nullptr) {
}

MainGameScene::~MainGameScene() {
}

void MainGameScene::onEnter() {
  if (!TTF_WasInit()) {
    if (!TTF_Init()) {
      std::cerr << "Failed to initialize TTF: " << SDL_GetError() << std::endl;
      return;
    }
  }

  font = TTF_OpenFont("assets/fonts/font.ttf", 28);
  smallFont = TTF_OpenFont("assets/fonts/font.ttf", 18);

  if (!font) {
    std::cerr << "Failed to load font: " << SDL_GetError() << std::endl;
  }
  if (!smallFont) {
    std::cerr << "Failed to load small font: " << SDL_GetError() << std::endl;
  }
}

void MainGameScene::handleEvent(const SDL_Event& event) {
  if (event.type == SDL_EVENT_KEY_DOWN) {
    switch (event.key.key) {
      case SDLK_ESCAPE: {
        // Return to main menu
        auto mainMenu = std::make_unique<MainMenuScene>(guiView);
        changeScene(std::move(mainMenu));
        break;
      }
      case SDLK_Q:
        quit();
        break;
    }
  }
}

void MainGameScene::update(float deltaTime) {
  // Placeholder for game logic updates
  // Update game state, animations, etc.
  if (deltaTime < -1) return; // avoiding warnings
}

void MainGameScene::render() {
  // Clear screen with game background color
  SDL_SetRenderDrawColor(getRenderer(), 10, 50, 10, 255);  // Dark green
  SDL_RenderClear(getRenderer());

  // Draw main game area
  SDL_FRect gameArea = {50, 50, 700, 450};
  SDL_SetRenderDrawColor(getRenderer(), 30, 100, 30, 255);  // Lighter green (like a field)
  SDL_RenderFillRect(getRenderer(), &gameArea);

  // Draw border around game area
  SDL_SetRenderDrawColor(getRenderer(), 255, 255, 255, 255);
  SDL_RenderRect(getRenderer(), &gameArea);

  // Draw scene title
  renderText("MAIN GAME SCENE", 400, 100, {255, 255, 100, 255});

  // Draw placeholder game info
  renderText("Football Management Game", 400, 275, {255, 255, 255, 255});

  // Draw UI elements
  renderGameUI();

  // Draw instructions
  renderText("Press ESC to return to menu | Q to quit", 400, 550, {200, 200, 200, 255});
}

void MainGameScene::onExit() {
  if (font) {
    TTF_CloseFont(font);
    font = nullptr;
  }
  if (smallFont) {
    TTF_CloseFont(smallFont);
    smallFont = nullptr;
  }
}

void MainGameScene::renderText(const char* text, float x, float y, SDL_Color color) {
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

void MainGameScene::renderGameUI() {
  // Draw placeholder HUD elements

  // Top status bar
  SDL_FRect statusBar = {0, 0, 800, 40};
  SDL_SetRenderDrawColor(getRenderer(), 40, 40, 40, 255);
  SDL_RenderFillRect(getRenderer(), &statusBar);

  // Side panel
  SDL_FRect sidePanel = {650, 40, 150, 460};
  SDL_SetRenderDrawColor(getRenderer(), 60, 60, 60, 255);
  SDL_RenderFillRect(getRenderer(), &sidePanel);
  SDL_SetRenderDrawColor(getRenderer(), 100, 100, 100, 255);
  SDL_RenderRect(getRenderer(), &sidePanel);

  // Use smaller font for UI text if available
  TTF_Font* uiFont = smallFont ? smallFont : font;
  if (uiFont) {
    SDL_Color uiColor = {200, 200, 200, 255};

    // Status bar text
    SDL_Surface* statusSurface = TTF_RenderText_Solid(uiFont, "Game Status: Running", strlen("Game Status: Running"), uiColor);
    if (statusSurface) {
      SDL_Texture* statusTexture = SDL_CreateTextureFromSurface(getRenderer(), statusSurface);
      if (statusTexture) {
        SDL_FRect statusRect = {10, 10, (float)statusSurface->w, (float)statusSurface->h};
        SDL_RenderTexture(getRenderer(), statusTexture, nullptr, &statusRect);
        SDL_DestroyTexture(statusTexture);
      }
      SDL_DestroySurface(statusSurface);
    }

    // Side panel text
    SDL_Surface* panelSurface = TTF_RenderText_Solid(uiFont, "Controls", strlen("Controls"), uiColor);
    if (panelSurface) {
      SDL_Texture* panelTexture = SDL_CreateTextureFromSurface(getRenderer(), panelSurface);
      if (panelTexture) {
        SDL_FRect panelRect = {
          725 - panelSurface->w / 2.0f,
          70,
          (float)panelSurface->w,
          (float)panelSurface->h
        };
        SDL_RenderTexture(getRenderer(), panelTexture, nullptr, &panelRect);
        SDL_DestroyTexture(panelTexture);
      }
      SDL_DestroySurface(panelSurface);
    }
  }
}

