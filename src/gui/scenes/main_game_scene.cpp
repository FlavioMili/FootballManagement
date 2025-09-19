// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "main_game_scene.h"
#include "global.h"
#include "gui/scenes/roster_scene.h"
#include "gui/scenes/strategy_scene.h"
#include "main_menu_scene.h"
#include <SDL3_ttf/SDL_ttf.h>
#include <algorithm>
#include <iostream>
#include <vector>

SceneID MainGameScene::getID() const { return SceneID::GAME_MENU; }

MainGameScene::MainGameScene(GUIView *guiView_ptr)
    : GUIScene(guiView_ptr), font(nullptr) {}

MainGameScene::~MainGameScene() { cleanup(); }

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

void MainGameScene::handleEvent(const SDL_Event &event) {
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

void MainGameScene::update(float deltaTime) { (void)deltaTime; }

void MainGameScene::render() {
  // Dark green background
  SDL_SetRenderDrawColor(getRenderer(), 10, 50, 10, 255);
  SDL_RenderClear(getRenderer());

  renderSidebar();
  renderLeaderboard();
  renderTopPlayers();
  buttonManager->render();
}

void MainGameScene::onExit() { cleanup(); }

void MainGameScene::initializeUI() {
  setupButtons();
  updateLayout(); // Set initial layout
}

void MainGameScene::renderSidebar() {
  SDL_SetRenderDrawColor(getRenderer(), 40, 40, 40,
                         255); // Dark grey for sidebar
  SDL_RenderFillRect(getRenderer(), &sidebarRect);
}

void MainGameScene::renderLeaderboard() {
  int windowWidth, windowHeight;
  SDL_GetWindowSizeInPixels(getWindow(), &windowWidth, &windowHeight);

  float leaderboardX = sidebarRect.w + 20;
  float leaderboardY = 20;

  SDL_Color textColor = {255, 255, 255, 255};
  TTF_Font *titleFont = TTF_OpenFont(FONT_PATH, 28);
  SDL_Surface *surface =
      TTF_RenderText_Solid(titleFont, "League Standings", 0, textColor);
  SDL_Texture *texture = SDL_CreateTextureFromSurface(getRenderer(), surface);
  SDL_FRect rect = {leaderboardX, leaderboardY, static_cast<float>(surface->w),
                    static_cast<float>(surface->h)};
  SDL_RenderTexture(getRenderer(), texture, nullptr, &rect);
  SDL_DestroySurface(surface);
  SDL_DestroyTexture(texture);
  TTF_CloseFont(titleFont);

  leaderboardY += rect.h + 10;

  League &league = guiView->getController().getLeagueById(
      guiView->getController().getManagedTeam().getLeagueId());
  const auto &leaderboard = league.getLeaderboard();
  std::vector<std::pair<int, int>> sorted_teams(leaderboard.begin(),
                                                leaderboard.end());
  std::sort(sorted_teams.begin(), sorted_teams.end(),
            [](const auto &a, const auto &b) { return a.second > b.second; });

  TTF_Font *itemFont = TTF_OpenFont(FONT_PATH, 20);
  int rank = 1;
  for (const auto &pair : sorted_teams) {
    Team &team = guiView->getController().getTeamById(pair.first);
    std::string text = std::to_string(rank) + ". " + team.getName() + " - " +
                       std::to_string(pair.second) + " pts";
    surface = TTF_RenderText_Solid(itemFont, text.c_str(), 0, textColor);
    texture = SDL_CreateTextureFromSurface(getRenderer(), surface);
    rect = {leaderboardX, leaderboardY, static_cast<float>(surface->w),
            static_cast<float>(surface->h)};
    SDL_RenderTexture(getRenderer(), texture, nullptr, &rect);
    SDL_DestroySurface(surface);
    SDL_DestroyTexture(texture);
    leaderboardY += rect.h + 5;
    rank++;
  }
  TTF_CloseFont(itemFont);
}

void MainGameScene::renderTopPlayers() {
  int windowWidth, windowHeight;
  SDL_GetWindowSizeInPixels(getWindow(), &windowWidth, &windowHeight);

  float topPlayersX = sidebarRect.w + 350;
  float topPlayersY = 20;

  SDL_Color textColor = {255, 255, 255, 255};
  TTF_Font *titleFont = TTF_OpenFont(FONT_PATH, 28);
  SDL_Surface *surface =
      TTF_RenderText_Solid(titleFont, "Top Players", 0, textColor);
  SDL_Texture *texture = SDL_CreateTextureFromSurface(getRenderer(), surface);
  SDL_FRect rect = {topPlayersX, topPlayersY, static_cast<float>(surface->w),
                    static_cast<float>(surface->h)};
  SDL_RenderTexture(getRenderer(), texture, nullptr, &rect);
  SDL_DestroySurface(surface);
  SDL_DestroyTexture(texture);
  TTF_CloseFont(titleFont);

  topPlayersY += rect.h + 10;

  auto players = guiView->getController().getPlayersForTeam(
      guiView->getController().getManagedTeam().getId());
  const auto &stats_config = guiView->getController().getStatsConfig();
  std::sort(players.begin(), players.end(),
            [&stats_config](const Player &a, const Player &b) {
              return a.getOverall(stats_config) > b.getOverall(stats_config);
            });

  TTF_Font *itemFont = TTF_OpenFont(FONT_PATH, 20);
  for (size_t i = 0; i < 3 && i < players.size(); ++i) {
    const auto &player = players[i];
    std::string text =
        player.getName() + " (" + player.getRole() +
        ") - OVR: " + std::to_string(player.getOverall(stats_config));
    surface = TTF_RenderText_Solid(itemFont, text.c_str(), 0, textColor);
    texture = SDL_CreateTextureFromSurface(getRenderer(), surface);
    rect = {topPlayersX, topPlayersY, static_cast<float>(surface->w),
            static_cast<float>(surface->h)};
    SDL_RenderTexture(getRenderer(), texture, nullptr, &rect);
    SDL_DestroySurface(surface);
    SDL_DestroyTexture(texture);
    topPlayersY += rect.h + 5;
  }
  TTF_CloseFont(itemFont);
}

void MainGameScene::setupButtons() {
  ButtonStyle sidebarButtonStyle;
  sidebarButtonStyle.backgroundColor = {50, 50, 50, 255};
  sidebarButtonStyle.hoverBackgroundColor = {70, 70, 70, 255};
  sidebarButtonStyle.textColor = {255, 255, 255, 255};
  sidebarButtonStyle.borderColor = {100, 100, 100, 255};
  sidebarButtonStyle.borderWidth = 1;
  sidebarButtonStyle.hasBorder = true;

  sidebarButtonIds.push_back(buttonManager->addButton(
      0, 0, 0, 0, "View Roster", sidebarButtonStyle, [this]() {
        guiView->overlayScene(std::make_unique<RosterScene>(guiView));
      }));
  sidebarButtonIds.push_back(buttonManager->addButton(
      0, 0, 0, 0, "Set Strategy", sidebarButtonStyle, [this]() {
        guiView->overlayScene(std::make_unique<StrategyScene>(guiView));
      }));
  sidebarButtonIds.push_back(buttonManager->addButton(
      0, 0, 0, 0, "Finances", sidebarButtonStyle, []() { /* Placeholder */ }));
  sidebarButtonIds.push_back(
      buttonManager->addButton(0, 0, 0, 0, "Transfer Market",
                               sidebarButtonStyle, []() { /* Placeholder */ }));

  ButtonStyle nextButtonStyle;
  nextButtonStyle.backgroundColor = {80, 120, 80, 255};
  nextButtonStyle.hoverBackgroundColor = {100, 140, 100, 255};
  nextButtonStyle.textColor = {255, 255, 255, 255};
  nextButtonStyle.borderColor = {120, 180, 120, 255};
  nextButtonStyle.borderWidth = 1;
  nextButtonStyle.hasBorder = true;

  nextButtonId =
      buttonManager->addButton(0, 0, 0, 0, "Next", nextButtonStyle, [this]() {
        guiView->getController().advanceWeek();
      });
}

void MainGameScene::updateLayout() {
  int windowWidth = 0;
  int windowHeight = 0;
  if (guiView && guiView->getWindow()) {
    SDL_GetWindowSize(guiView->getWindow(), &windowWidth, &windowHeight);
  }

  sidebarRect = {0, 0, static_cast<float>(windowWidth) * 0.2f,
                 static_cast<float>(windowHeight)};

  float buttonY = 10.0f;
  float buttonH = 50.0f;
  for (size_t i = 0; i < sidebarButtonIds.size(); ++i) {
    buttonManager->updateButtonPositionById(
        sidebarButtonIds[i],
        {sidebarRect.x + 10, buttonY, sidebarRect.w - 20, buttonH});
    buttonY += buttonH + 5; // Add some padding
  }

  if (nextButtonId != -1) {
    float buttonW = 100.0f;
    float buttonH2 = 40.0f;
    buttonManager->updateButtonPositionById(
        nextButtonId, {static_cast<float>(windowWidth) - buttonW - 10.0f, 10.0f,
                       buttonW, buttonH2});
  }
}

void MainGameScene::cleanup() {
  if (font) {
    TTF_CloseFont(font);
    font = nullptr;
  }
}
