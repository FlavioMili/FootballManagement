#include "main_game_scene.h"
#include "main_menu_scene.h"
#include <SDL3_ttf/SDL_ttf.h>
#include <iostream>

SceneID MainGameScene::getID() const { return SceneID::GAME_MENU; }

MainGameScene::MainGameScene(GUIView* guiView) 
  : GUIScene(guiView), 
  font(nullptr), 
  smallFont(nullptr),
  titleTexture(nullptr),
  gameInfoTexture(nullptr),
  instructionsTexture(nullptr),
  statusTexture(nullptr),
  controlsTexture(nullptr) {
}

MainGameScene::~MainGameScene() {
  cleanup();
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
    return;
  }
  if (!smallFont) {
    std::cerr << "Failed to load small font: " << SDL_GetError() << std::endl;
    return;
  }
  createStaticTextures();
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
  (void)deltaTime; // Avoid unused parameter warning
}

void MainGameScene::render() {
  SDL_SetRenderDrawColor(getRenderer(), 10, 50, 10, 255);  // Dark green
  SDL_RenderClear(getRenderer());

  SDL_FRect gameArea = {50, 50, 700, 450};
  SDL_SetRenderDrawColor(getRenderer(), 30, 100, 30, 255);  // Lighter green (like a field)
  SDL_RenderFillRect(getRenderer(), &gameArea);

  // Draw border around game area
  SDL_SetRenderDrawColor(getRenderer(), 255, 255, 255, 255);
  SDL_RenderRect(getRenderer(), &gameArea);

  // Render pre-created textures
  renderCachedTexture(titleTexture, 400, 100);
  renderCachedTexture(gameInfoTexture, 400, 275);
  renderCachedTexture(instructionsTexture, 400, 550);

  // Draw UI elements
  renderGameUI();
}

void MainGameScene::onExit() {
  cleanup();
}

void MainGameScene::createStaticTextures() {
  SDL_Renderer* renderer = getRenderer();
  if (!renderer) {
    std::cerr << "Renderer not available for texture creation" << std::endl;
    return;
  }

  // Create title texture
  titleTexture = createTextTexture("MAIN GAME SCENE", font, {255, 255, 100, 255});
  if (!titleTexture) std::cerr << "Failed to create title texture" << std::endl;

  // Create game info texture
  gameInfoTexture = createTextTexture("Football Management Game", font, {255, 255, 255, 255});
  if (!gameInfoTexture) std::cerr << "Failed to create game info texture" << std::endl;

  // Create instructions texture
  instructionsTexture = createTextTexture("Press ESC to return to menu | Q to quit", font, {200, 200, 200, 255});
  if (!instructionsTexture) std::cerr << "Failed to create instructions texture" << std::endl;

  // Create UI textures
  statusTexture = createTextTexture("Game Status: Running", smallFont, {200, 200, 200, 255});
  if (!statusTexture) std::cerr << "Failed to create status texture" << std::endl;

  controlsTexture = createTextTexture("Controls", smallFont, {200, 200, 200, 255});
  if (!controlsTexture) std::cerr << "Failed to create controls texture" << std::endl;
}

SDL_Texture* MainGameScene::createTextTexture(const char* text, TTF_Font* textFont, SDL_Color color) {
  if (!textFont || !text || !getRenderer()) {
    std::cerr << "Invalid parameters for createTextTexture" << std::endl;
    return nullptr;
  }

  SDL_Surface* surface = TTF_RenderText_Solid(textFont, text, 0, color);
  if (!surface) {
    std::cerr << "Failed to create text surface: " << SDL_GetError() << std::endl;
    return nullptr;
  }

  SDL_Texture* texture = SDL_CreateTextureFromSurface(getRenderer(), surface);
  if (!texture) {
    std::cerr << "Failed to create texture from surface: " << SDL_GetError() << std::endl;
  }

  SDL_DestroySurface(surface);
  return texture;
}

void MainGameScene::renderCachedTexture(SDL_Texture* texture, float centerX, float centerY) {
  if (!texture) return;

  float w, h;
  if (SDL_GetTextureSize(texture, &w, &h) == 0) {
    SDL_FRect textRect = {
      centerX - w / 2.0f,  // Center horizontally
      centerY - h / 2.0f,  // Center vertically
      (float)w,
      (float)h
    };
    SDL_RenderTexture(getRenderer(), texture, nullptr, &textRect);
  }
}

void MainGameScene::renderText(const char* text, float x, float y, SDL_Color color) {
  if (!font || !text) return;

  SDL_Surface* textSurface = TTF_RenderText_Solid(font, text, strlen(text), color);
  if (!textSurface) return;

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

  SDL_DestroySurface(textSurface); // Always clean up surface
}

void MainGameScene::renderGameUI() {
  SDL_Renderer* renderer = getRenderer();

  // Top status bar
  SDL_FRect statusBar = {0, 0, 800, 40};
  SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
  SDL_RenderFillRect(renderer, &statusBar);

  // Side panel
  SDL_FRect sidePanel = {650, 40, 150, 460};
  SDL_SetRenderDrawColor(renderer, 60, 60, 60, 255);
  SDL_RenderFillRect(renderer, &sidePanel);
  SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
  SDL_RenderRect(renderer, &sidePanel);

  // Render cached UI textures
  if (statusTexture) {
    float w, h;
    if (SDL_GetTextureSize(statusTexture, &w, &h) == 0) {
      SDL_FRect statusRect = {10, 10, (float)w, (float)h};
      SDL_RenderTexture(renderer, statusTexture, nullptr, &statusRect);
    }
  }

  if (controlsTexture) {
    float w, h;
    if (SDL_GetTextureSize(controlsTexture, &w, &h) == 0) {
      SDL_FRect controlsRect = {
        725 - w / 2.0f,  // Center in side panel
        70,
        (float)w,
        (float)h
      };
      SDL_RenderTexture(renderer, controlsTexture, nullptr, &controlsRect);
    }
  }
}

void MainGameScene::cleanup() {
  // Clean up textures
  if (titleTexture) {
    SDL_DestroyTexture(titleTexture);
    titleTexture = nullptr;
  }
  if (gameInfoTexture) {
    SDL_DestroyTexture(gameInfoTexture);
    gameInfoTexture = nullptr;
  }
  if (instructionsTexture) {
    SDL_DestroyTexture(instructionsTexture);
    instructionsTexture = nullptr;
  }
  if (statusTexture) {
    SDL_DestroyTexture(statusTexture);
    statusTexture = nullptr;
  }
  if (controlsTexture) {
    SDL_DestroyTexture(controlsTexture);
    controlsTexture = nullptr;
  }

  // Clean up fonts
  if (font) {
    TTF_CloseFont(font);
    font = nullptr;
  }
  if (smallFont) {
    TTF_CloseFont(smallFont);
    smallFont = nullptr;
  }
}
