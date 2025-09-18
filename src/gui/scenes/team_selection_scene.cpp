// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "team_selection_scene.h"
#include "global.h"
#include "view/gui/gui_view.h"
#include "view/gui/scenes/main_menu_scene.h"
#include <iostream>
#include <SDL3_ttf/SDL_ttf.h>

TeamSelectionScene::TeamSelectionScene(GUIView* parent)
  : GUIScene(parent), parent_view(parent),
  font(nullptr), title_font(nullptr),
  button_manager(getRenderer(), TTF_OpenFont(FONT_PATH, 18))
{}

TeamSelectionScene::~TeamSelectionScene() {
  cleanup();
}

void TeamSelectionScene::cleanup() {
  if (font) {
    TTF_CloseFont(font);
    font = nullptr;
  }
  if (title_font) {
    TTF_CloseFont(title_font);
    title_font = nullptr;
  }
}

void TeamSelectionScene::onEnter() {
  if (!TTF_WasInit() && !TTF_Init()) {
    std::cerr << "Failed to initialize TTF: " << SDL_GetError() << '\n';
    return;
  }

  // Load fonts with error checking
  font = TTF_OpenFont("assets/fonts/font.ttf", 24);
  if (!font) {
    std::cerr << "Failed to load button font: " << SDL_GetError() << '\n';
    // Try fallback font path if FONT_PATH is defined
#ifdef FONT_PATH
    font = TTF_OpenFont(FONT_PATH, 24);
    if (!font) {
      std::cerr << "Failed to load fallback font: " << SDL_GetError() << '\n';
      return;
    }
#else
    return;
#endif
  }

  title_font = TTF_OpenFont("assets/fonts/font.ttf", 36);
  if (!title_font) {
    std::cerr << "Failed to load title font: " << SDL_GetError() << '\n';
#ifdef FONT_PATH
    title_font = TTF_OpenFont(FONT_PATH, 36);
    if (!title_font) {
      std::cerr << "Failed to load fallback title font: " << SDL_GetError() << '\n';
      // Use the same font as buttons if title font fails
      title_font = font;
    }
#else
    title_font = font;
#endif
  }

  // Set font for button manager
  button_manager.setFont(font);

  std::cout << "Entering TeamSelectionScene\n";
  loadAvailableTeams();
  setupTeamButtons();
}

void TeamSelectionScene::onExit() {
  std::cout << "Exiting TeamSelectionScene\n";
  button_manager.clearButtons();
  cleanup();
}

void TeamSelectionScene::handleEvent(const SDL_Event& event) {
  if (event.type == SDL_EVENT_MOUSE_MOTION) {
    button_manager.handleMouseMove(event.motion.x, event.motion.y);
  } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
    button_manager.handleMouseClick(event.button.x, event.button.y);
  }
}

void TeamSelectionScene::update(float deltaTime) {
  // No continuous updates needed for this scene yet
  (void)deltaTime;
}

void TeamSelectionScene::render() {
  SDL_SetRenderDrawColor(getRenderer(), 50, 50, 50, 255); // Dark grey background
  SDL_RenderClear(getRenderer());

  // Render title using the pre-loaded title font
  if (title_font) {
    SDL_Color textColor = {255, 255, 255, 255}; // White

    SDL_Surface* textSurface = TTF_RenderText_Solid(title_font, "Select Your Team", 0, textColor);
    if (textSurface) {
      SDL_Texture* textTexture = SDL_CreateTextureFromSurface(getRenderer(), textSurface);
      if (textTexture) {
        SDL_FRect textRect = { 
          (1200.0f - textSurface->w) / 2.0f, 
          50.0f, 
          (float)textSurface->w, 
          (float)textSurface->h 
        };
        SDL_RenderTexture(getRenderer(), textTexture, NULL, &textRect);
        SDL_DestroyTexture(textTexture);
      } else {
        std::cerr << "Failed to create texture from text: " << SDL_GetError() << "\n";
      }
      SDL_DestroySurface(textSurface);
    } else {
      std::cerr << "Failed to render text: " << SDL_GetError() << "\n";
    }
  }

  button_manager.render();
}

void TeamSelectionScene::loadAvailableTeams() {
  available_teams = parent_view->getController().getAvailableTeams();
}

void TeamSelectionScene::setupTeamButtons() {
  float startY = 150.0f;
  float buttonHeight = 50.0f;
  float padding = 10.0f;
  float buttonWidth = 400.0f;

  for (size_t i = 0; i < available_teams.size(); ++i) {
    const Team& team = available_teams[i];
    // Get league name from GameController
    std::string leagueName = parent_view->getController().getLeagueById(team.getLeagueId()).getName();
    std::string buttonText = team.getName() + " (" + leagueName + ")";
    float buttonX = (1200.0f - buttonWidth) / 2.0f; // Fixed: was 120.0f, should be screen width
    float buttonY = startY + (i * (buttonHeight + padding));

    button_manager.addButton(
      buttonX, buttonY, buttonWidth, buttonHeight,
      buttonText,
      [this, team_id = team.getId()]() {
        this->onTeamSelected(team_id);
      }
    );
  }
}

void TeamSelectionScene::onTeamSelected(int team_id) {
  std::cout << "Team selected: " << team_id << "\n";
  parent_view->getController().selectManagedTeam(team_id);
  parent_view->changeScene(std::make_unique<MainMenuScene>(parent_view));
}

SceneID TeamSelectionScene::getID() const {
  return SceneID::TEAM_SELECTION;
}
