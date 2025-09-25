// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "roster_scene.h"
#include "gui/gui_view.h"
#include "global/paths.h"
#include <iostream>
#include <SDL3_ttf/SDL_ttf.h>

RosterScene::RosterScene(GUIView* parent)
  : GUIScene(parent), parent_view(parent),
  button_manager(parent->getRenderer(), TTF_OpenFont(FONT_PATH, 24)) {}

RosterScene::~RosterScene() {
  // The font is managed by ButtonManager, no need to close here.
}

void RosterScene::onEnter() {
  std::cout << "Entering RosterScene\n";
  loadRoster();
  setupUI();
}

void RosterScene::onExit() {
  std::cout << "Exiting RosterScene\n";
  button_manager.clearButtons();
}

void RosterScene::onResize(int width, int height) {
  (void)width;
  (void)height;
  setupUI();
}

void RosterScene::setupUI() {
  button_manager.clearButtons();

  int width, height;
  SDL_GetWindowSizeInPixels(getWindow(), &width, &height);

  backButtonId = button_manager.addButton(10, static_cast<float>(height) - 50, 100, 40, "Back", [this]() {
    parent_view->popScene();
  });

  setupPlayerDisplay();
}

void RosterScene::handleEvent(const SDL_Event& event) {
  if (event.type == SDL_EVENT_MOUSE_MOTION) {
    button_manager.handleMouseMove(event.motion.x, event.motion.y);
  } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
    button_manager.handleMouseClick(event.button.x, event.button.y);
  }
}

void RosterScene::update(float deltaTime) {
  (void)(deltaTime);
}

void RosterScene::render() {
  SDL_SetRenderDrawColor(getRenderer(), 50, 50, 50, 255); // Dark grey background
  SDL_RenderClear(getRenderer());

  int width, height;
  SDL_GetWindowSizeInPixels(getWindow(), &width, &height);

  // Render title
  SDL_Color textColor = {255, 255, 255, 255}; // White
  TTF_Font* font = TTF_OpenFont(FONT_PATH, 36);
  if (!font) {
    std::cerr << "Failed to load font: " << SDL_GetError() << "\n";
    return;
  }

  SDL_Surface* textSurface = TTF_RenderText_Solid(font, "Team Roster", 0, textColor);
  if (!textSurface) {
    std::cerr << "Failed to render text: " << SDL_GetError() << "\n";
    TTF_CloseFont(font);
    return;
  }
  SDL_Texture* textTexture = SDL_CreateTextureFromSurface(getRenderer(), textSurface);
  if (!textTexture) {
    std::cerr << "Failed to create texture from text: " << SDL_GetError() << "\n";
    SDL_DestroySurface(textSurface);
    TTF_CloseFont(font);
    return;
  }

  SDL_FRect textRect = { static_cast<float>(width - textSurface->w) / 2.0f, 50.0f, 
    static_cast<float>(textSurface->w), static_cast<float>(textSurface->h) };
  SDL_RenderTexture(getRenderer(), textTexture, NULL, &textRect);

  SDL_DestroyTexture(textTexture);
  SDL_DestroySurface(textSurface);
  TTF_CloseFont(font);

  // Render player list
  TTF_Font* playerFont = TTF_OpenFont(FONT_PATH, 24);
  if (!playerFont) {
    std::cerr << "Failed to load player font: " << SDL_GetError() << "\n";
    return;
  }

  int startY = 150;
  float lineHeight = 30.0f;
  for (size_t i = 0; i < roster_players.size(); ++i) {
    const Player& player = roster_players[i].get();
    std::string playerText = player.getName() + " (" + player.getRole() + ") - OVR: " +
      std::to_string(player.getOverall(parent_view->getController().getStatsConfig()));

    SDL_Surface* playerSurface = TTF_RenderText_Solid(playerFont, playerText.c_str(), 0, textColor);
    if (!playerSurface) {
      std::cerr << "Failed to render player text: " << SDL_GetError() << "\n";
      continue;
    }
    SDL_Texture* playerTexture = SDL_CreateTextureFromSurface(getRenderer(), playerSurface);
    if (!playerTexture) {
      std::cerr << "Failed to create player texture: " << SDL_GetError() << "\n";
      SDL_DestroySurface(playerSurface);
      continue;
    }

    SDL_FRect playerRect = { 50.0f, static_cast<float>(startY) + static_cast<float>(i) * lineHeight, 
                                    static_cast<float>(playerSurface->w), static_cast<float>(playerSurface->h) };
    SDL_RenderTexture(getRenderer(), playerTexture, NULL, &playerRect);

    SDL_DestroyTexture(playerTexture);
    SDL_DestroySurface(playerSurface);
  }
  TTF_CloseFont(playerFont);

  button_manager.render();
}

void RosterScene::loadRoster() {
  auto managedTeamOpt = parent_view->getController().getManagedTeam();
  if (managedTeamOpt.has_value()) {
    roster_players = parent_view->getController().getPlayersForTeam(
        managedTeamOpt->get().getId());
  }
}

void RosterScene::setupPlayerDisplay() {
  // TODO do later
}

SceneID RosterScene::getID() const {
  return SceneID::ROSTER;
}
