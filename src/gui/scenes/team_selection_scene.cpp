// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "team_selection_scene.h"
#include "gamedata.h"
#include "global/global.h"
#include "global/logger.h"
#include "global/paths.h"
#include "gui/gui_view.h"
#include "gui/scenes/main_menu_scene.h"
#include <SDL3_ttf/SDL_ttf.h>
#include <algorithm>
#include <iostream>

TeamSelectionScene::TeamSelectionScene(GUIView *parent)
    : GUIScene(parent), parent_view(parent), font(nullptr), title_font(nullptr),
      button_manager(getRenderer(), TTF_OpenFont(FONT_PATH, 18)) {}

TeamSelectionScene::~TeamSelectionScene() { cleanup(); }

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
  font = TTF_OpenFont(FONT_PATH, 24);
  if (!font) {
    std::cerr << "Failed to load button font: " << SDL_GetError() << '\n';
  }

  title_font = TTF_OpenFont(FONT_PATH, 36);
  if (!title_font) {
    std::cerr << "Failed to load title font: " << SDL_GetError() << '\n';
    title_font = font;
  }

  // Set font for button manager
  button_manager.setFont(font);

  Logger::debug("Entering TeamSelectionScene\n");
  loadAvailableTeams();
  setupTeamButtons();
}

void TeamSelectionScene::onExit() {
  Logger::debug("Exiting TeamSelectionScene\n");
  button_manager.clearButtons();
  cleanup();
}

void TeamSelectionScene::handleEvent(const SDL_Event &event) {
  if (event.type == SDL_EVENT_MOUSE_MOTION) {
    button_manager.handleMouseMove(event.motion.x, event.motion.y);
  } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
    button_manager.handleMouseClick(event.button.x, event.button.y);
  }
}

void TeamSelectionScene::update(float deltaTime) { (void)deltaTime; }

void TeamSelectionScene::render() {
  SDL_SetRenderDrawColor(getRenderer(), 50, 50, 50,
                         255); // Dark grey background
  SDL_RenderClear(getRenderer());

  // Render title using the pre-loaded title font
  if (title_font) {
    SDL_Color textColor = {255, 255, 255, 255}; // White

    SDL_Surface *textSurface =
        TTF_RenderText_Solid(title_font, "Select Your Team", 0, textColor);
    if (textSurface) {
      SDL_Texture *textTexture =
          SDL_CreateTextureFromSurface(getRenderer(), textSurface);
      if (textTexture) {
        SDL_FRect textRect = {(1200.0f - static_cast<float>(textSurface->w)) /
                                  2.0f,
                              50.0f, static_cast<float>(textSurface->w),
                              static_cast<float>(textSurface->h)};
        SDL_RenderTexture(getRenderer(), textTexture, NULL, &textRect);
        SDL_DestroyTexture(textTexture);
      } else {
        std::cerr << "Failed to create texture from text: " << SDL_GetError()
                  << "\n";
      }
      SDL_DestroySurface(textSurface);
    } else {
      std::cerr << "Failed to render text: " << SDL_GetError() << "\n";
    }
  }

  button_manager.render();
}

/**
* Note that this function skips the free agents team
*/
void TeamSelectionScene::loadAvailableTeams() {
  available_teams = parent_view->getController().getTeams();
  available_teams.erase(
      std::remove_if(available_teams.begin(), available_teams.end(),
                     [](const std::reference_wrapper<const Team> &team_ref) {
                       return team_ref.get().getName() == FREE_AGENTS_TEAM_NAME;
                     }),
      available_teams.end());
}

void TeamSelectionScene::setupTeamButtons() {
  float startY = 150.0f;
  float buttonHeight = 50.0f;
  float padding = 10.0f;
  float buttonWidth = 400.0f;

  for (size_t i = 0; i < available_teams.size(); ++i) {
    const Team &team = available_teams[i].get();

    // Get league name from GameController
    std::string leagueName = "Unknown League";
    auto leagueOpt =
        parent_view->getController().getLeagueById(team.getLeagueId());
    if (leagueOpt.has_value()) {
      leagueName = leagueOpt->get().getName();
    }
    std::string buttonText = team.getName() + " (" + leagueName + ")";
    float buttonX = (1200.0f - buttonWidth) /
                    2.0f; // Fixed: was 120.0f, should be screen width
    float buttonY = startY + (static_cast<float>(i) * (buttonHeight + padding));

    button_manager.addButton(
        buttonX, buttonY, buttonWidth, buttonHeight, buttonText,
        [this, team_id = team.getId()]() { this->onTeamSelected(team_id); });
  }
}

void TeamSelectionScene::onTeamSelected(uint16_t team_id) {
  Logger::debug("Team selected: " +
                GameData::instance().getTeam(team_id)->get().getName() + "\n");
  parent_view->getController().selectManagedTeam(team_id);
  parent_view->popScene();
}

SceneID TeamSelectionScene::getID() const { return SceneID::TEAM_SELECTION; }
