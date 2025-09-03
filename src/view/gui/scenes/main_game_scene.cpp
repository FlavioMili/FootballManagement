// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "main_game_scene.h"
#include "global.h"
#include "main_menu_scene.h"
#include "view/gui/scenes/roster_scene.h"
#include "view/gui/scenes/strategy_scene.h"
#include <SDL3_ttf/SDL_ttf.h> 
#include <iostream> 

SceneID MainGameScene::getID() const { return SceneID::GAME_MENU; }

MainGameScene::MainGameScene(GUIView* guiView) 
: GUIScene(guiView), font(nullptr) {}

MainGameScene::~MainGameScene() {
  cleanup();
}

void MainGameScene::onEnter() { 
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
  initializeUI(); 
}

void MainGameScene::onResize(int width, int height) {
  (void)width;
  (void)height;
  updateLayout();
}

void MainGameScene::handleEvent(const SDL_Event& event) {
  if (event.type == SDL_EVENT_WINDOW_RESIZED) {
    int w, h;
    SDL_GetWindowSizeInPixels(getWindow(), &w, &h);
    onResize(w, h);
  }

  if (event.type == SDL_EVENT_MOUSE_MOTION) {
    buttonManager->handleMouseMove(event.motion.x, event.motion.y);
  } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
    buttonManager->handleMouseClick(event.button.x, event.button.y);
  }

  if (event.type == SDL_EVENT_KEY_DOWN) {
    switch (event.key.key) {
      case SDLK_ESCAPE:
        changeScene(std::make_unique<MainMenuScene>(guiView));
        break;
      case SDLK_Q:
        quit();
        break;
    }
  }
} 

void MainGameScene::update(float deltaTime) { 
  (void)deltaTime; 
}

void MainGameScene::render() { 
  SDL_SetRenderDrawColor(getRenderer(), 10, 50, 10, 255); // Dark green background 
  SDL_RenderClear(getRenderer()); 

  renderSidebar(); 
  buttonManager->render(); 
}

void MainGameScene::onExit() { 
  cleanup(); 
}

void MainGameScene::initializeUI() {
  setupButtons();
  updateLayout(); // Set initial layout
}

void MainGameScene::renderSidebar() {
  SDL_SetRenderDrawColor(getRenderer(), 40, 40, 40, 255); // Dark grey for sidebar
  SDL_RenderFillRect(getRenderer(), &sidebarRect);
}

void MainGameScene::setupButtons() {
  ButtonStyle sidebarButtonStyle;
  sidebarButtonStyle.backgroundColor = {50, 50, 50, 255};
  sidebarButtonStyle.hoverBackgroundColor = {70, 70, 70, 255};
  sidebarButtonStyle.textColor = {255, 255, 255, 255};
  sidebarButtonStyle.borderColor = {100, 100, 100, 255};
  sidebarButtonStyle.borderWidth = 1;
  sidebarButtonStyle.hasBorder = true;

  sidebarButtonIds.push_back(buttonManager->addButton(0, 0, 0, 0, "View Roster", sidebarButtonStyle, [this]() { guiView->overlayScene(std::make_unique<RosterScene>(guiView)); }));
  sidebarButtonIds.push_back(buttonManager->addButton(0, 0, 0, 0, "Set Strategy", sidebarButtonStyle, [this]() { guiView->overlayScene(std::make_unique<StrategyScene>(guiView)); }));
  sidebarButtonIds.push_back(buttonManager->addButton(0, 0, 0, 0, "Finances", sidebarButtonStyle, [](){ /* Placeholder */ }));
  sidebarButtonIds.push_back(buttonManager->addButton(0, 0, 0, 0, "Transfer Market", sidebarButtonStyle, [](){ /* Placeholder */ }));

  ButtonStyle nextButtonStyle;
  nextButtonStyle.backgroundColor = {80, 120, 80, 255};
  nextButtonStyle.hoverBackgroundColor = {100, 140, 100, 255};
  nextButtonStyle.textColor = {255, 255, 255, 255};
  nextButtonStyle.borderColor = {120, 180, 120, 255};
  nextButtonStyle.borderWidth = 1;
  nextButtonStyle.hasBorder = true;

  nextButtonId = buttonManager->addButton(0, 0, 0, 0, "Next", nextButtonStyle, [this](){
    guiView->getController().advanceWeek();
  });
}

void MainGameScene::updateLayout() {
  int windowWidth = 0;
  int windowHeight = 0;
  if (guiView && guiView->getWindow()) {
    SDL_GetWindowSize(guiView->getWindow(), &windowWidth, &windowHeight);
  }

  sidebarRect = { 0, 0, windowWidth * 0.2f, (float)windowHeight };

  float buttonY = 10.0f;
  float buttonHeight = 50.0f;
  for (size_t i = 0; i < sidebarButtonIds.size(); ++i) {
    buttonManager->updateButtonPositionById(sidebarButtonIds[i], { sidebarRect.x + 10, buttonY, sidebarRect.w - 20, buttonHeight });
    buttonY += buttonHeight + 5; // Add some padding
  }

  if (nextButtonId != -1) {
    float buttonWidth = 100.0f;
    float buttonHeight = 40.0f;
    buttonManager->updateButtonPositionById(nextButtonId, { windowWidth - buttonWidth - 10.0f, 10.0f, buttonWidth, buttonHeight });
  }
}

void MainGameScene::cleanup() { 
  if (font) { 
    TTF_CloseFont(font); 
    font = nullptr; 
  }
}
