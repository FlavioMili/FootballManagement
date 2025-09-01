// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "view/gui/button_manager.h"
#include "view/gui/scenes/main_menu_scene.h"
#include "view/gui/gui_scene.h"
#include "view/gui/gui_view.h"
#include "view/gui/scenes/main_game_scene.h"
#include "view/gui/scenes/settings_scene.h"
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

MainMenuScene::MainMenuScene(GUIView* guiView) 
    : GUIScene(guiView), font(nullptr), titleTexture(nullptr), titleRect({}) {}

void MainMenuScene::onEnter() {
  if (!TTF_WasInit() && !TTF_Init()) {
    std::cerr << "Failed to initialize TTF: " << SDL_GetError() << '\n';
    return;
  }

  font = TTF_OpenFont("assets/fonts/font.ttf", 32);
  if (!font) {
    std::cerr << "Failed to load font: " << SDL_GetError() << '\n';
    return;
  }

  buttonManager = std::make_unique<ButtonManager>(getRenderer(), font);
  
  createStaticContent();
  updateLayout();
}

void MainMenuScene::handleEvent(const SDL_Event& event) {
  if (buttonManager) {
    if (event.type == SDL_EVENT_MOUSE_MOTION) {
      buttonManager->handleMouseMove(event.motion.x, event.motion.y);
    }
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
      if (buttonManager->handleMouseClick(event.button.x, event.button.y)) {
        return;
      }
    }
    if (event.type == SDL_EVENT_WINDOW_RESIZED) {
      updateLayout();
    }
  }

  if (event.type == SDL_EVENT_KEY_DOWN) {
    switch (event.key.key) {
      case SDLK_ESCAPE:
        quit();
        break;
      case SDLK_RETURN:
        handleButtonClick(LOAD_GAME_BUTTON);
        break;
    }
  }
}

void MainMenuScene::createStaticContent() {
  if (!buttonManager || !font) return;

  const char* title = "Football Management";
  SDL_Color titleColor = {255, 255, 100, 255};
  SDL_Surface* titleSurface = TTF_RenderText_Solid(font, title, 0, titleColor);
  
  if (titleSurface) {
    titleTexture = SDL_CreateTextureFromSurface(getRenderer(), titleSurface);
    if (!titleTexture) {
      std::cerr << "Failed to create title texture: " << SDL_GetError() << '\n';
    }
    SDL_DestroySurface(titleSurface);
  }

  buttonManager->addButton({0, 0, 200, 60}, "New game", [this]() {
    handleButtonClick(NEW_GAME_BUTTON);
  });

  buttonManager->addButton({0, 0, 200, 60}, "Load game", [this]() {
    handleButtonClick(LOAD_GAME_BUTTON);
  });

  buttonManager->addButton({0, 0, 200, 60}, "Settings", [this]() {
    handleButtonClick(SETTINGS_BUTTON);
  });

  buttonManager->addButton({0, 0, 200, 60}, "Quit game", [this]() {
    handleButtonClick(QUIT_BUTTON);
  });
}

void MainMenuScene::updateLayout() {
  if (!buttonManager) return;

  int w, h;
  SDL_GetWindowSizeInPixels(guiView->getWindow(), &w, &h);

  // Update title position
  if (titleTexture) {
    float titleW, titleH;
    if (SDL_GetTextureSize(titleTexture, &titleW, &titleH)) {
      titleRect = {
        static_cast<float>(w)/2 - titleW / 2.0f,
        100.0f,
        static_cast<float>(titleW),
        static_cast<float>(titleH)
      };
    }
  }

  // Update button positions
  float btnWidth = 200.0f;
  float btnHeight = 60.0f;
  float startX = static_cast<float>(w)/2 - btnWidth/2;
  float startY = 200.0f;
  float padding = 30.0f;

  buttonManager->updateButtonPosition(0, {startX, startY, btnWidth, btnHeight});
  buttonManager->updateButtonPosition(1, {startX, startY + (btnHeight + padding), btnWidth, btnHeight});
  buttonManager->updateButtonPosition(2, {startX, startY + 2 * (btnHeight + padding), btnWidth, btnHeight});
  buttonManager->updateButtonPosition(3, {startX, startY + 3 * (btnHeight + padding), btnWidth, btnHeight});
}

void MainMenuScene::update(float deltaTime) {
  (void)deltaTime;
}

void MainMenuScene::render() {
  SDL_SetRenderDrawColor(getRenderer(), 30, 30, 35, 255);
  SDL_RenderClear(getRenderer());

  renderTitle();

  if (buttonManager) { buttonManager->render(); }
}

void MainMenuScene::onExit() {
  if (titleTexture) {
    SDL_DestroyTexture(titleTexture);
    titleTexture = nullptr;
  }
  if (font) {
    TTF_CloseFont(font);
    font = nullptr;
  }
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

void MainMenuScene::renderTitle() {
  if (titleTexture) {
    SDL_RenderTexture(getRenderer(), titleTexture, nullptr, &titleRect);
  }
}

void MainMenuScene::make_scene() {
  if (!buttonManager) return;
  buttonManager->clearButtons();

  if (titleTexture) {
    SDL_DestroyTexture(titleTexture);
    titleTexture = nullptr;
  }

  int w, h;
  SDL_GetWindowSizeInPixels(guiView->getWindow(), &w, &h);

  const char* title = "Football Management";
  SDL_Color titleColor = {255, 255, 100, 255};
  SDL_Surface* titleSurface = TTF_RenderText_Solid(font, title, 0, titleColor);
  if (titleSurface) {
    titleTexture = SDL_CreateTextureFromSurface(getRenderer(), titleSurface);
    titleRect = {
      static_cast<float>(w)/2 - titleSurface->w / 2.0f,
      100.0f,
      static_cast<float>(titleSurface->w),
      static_cast<float>(titleSurface->h)
    };
    SDL_DestroySurface(titleSurface);
  }

  // Button layout parameters
  float btnWidth = 200.0f;
  float btnHeight = 60.0f;
  float startX = static_cast<float>(w)/2 - btnWidth/2;
  float startY = 200.0f;
  float padding = 30.0f;
  float currentY = startY;

  buttonManager->addButton({startX, currentY, btnWidth, btnHeight}, "New game", [this]() {
    handleButtonClick(NEW_GAME_BUTTON);
  });
  currentY += btnHeight + padding;

  buttonManager->addButton({startX, currentY, btnWidth, btnHeight}, "Load game", [this]() {
    handleButtonClick(LOAD_GAME_BUTTON);
  });
  currentY += btnHeight + padding;

  buttonManager->addButton({startX, currentY, btnWidth, btnHeight}, "Settings", [this]() {
    handleButtonClick(SETTINGS_BUTTON);
  });
  currentY += btnHeight + padding;

  buttonManager->addButton({startX, currentY, btnWidth, btnHeight}, "Quit game", [this]() {
    handleButtonClick(QUIT_BUTTON);
  });

}
