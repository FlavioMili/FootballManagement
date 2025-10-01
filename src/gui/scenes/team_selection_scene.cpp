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

  font = TTF_OpenFont(FONT_PATH, 24);
  if (!font) {
    std::cerr << "Failed to load button font: " << SDL_GetError() << '\n';
  }

  title_font = TTF_OpenFont(FONT_PATH, 36);
  if (!title_font) {
    std::cerr << "Failed to load title font: " << SDL_GetError() << '\n';
    title_font = font;
  }

  button_manager.setFont(font);

  Logger::debug("Entering TeamSelectionScene\n");
  loadAvailableLeagues();
  setupLeagueAndTeamButtons();
}

void TeamSelectionScene::onExit() {
  Logger::debug("Exiting TeamSelectionScene\n");
  button_manager.clearButtons();
  cleanup();
}

void TeamSelectionScene::handleEvent(const SDL_Event &event) {
  bool event_handled = false;
  SDL_FPoint mouse_point;

  switch (event.type) {
  case SDL_EVENT_MOUSE_MOTION:
    if (is_dragging_leagues_) {
      league_scroll_offset_ -= event.motion.xrel;
      setupLeagueAndTeamButtons();
      event_handled = true;
    } else if (is_dragging_teams_) {
      team_scroll_offset_ -= event.motion.yrel;
      setupLeagueAndTeamButtons();
      event_handled = true;
    }
    break;

  case SDL_EVENT_MOUSE_BUTTON_DOWN:
    mouse_point = {(float)event.button.x, (float)event.button.y};
    if (SDL_PointInRectFloat(&mouse_point, &league_list_rect_)) {
      is_dragging_leagues_ = true;
    } else if (SDL_PointInRectFloat(&mouse_point, &team_list_rect_)) {
      is_dragging_teams_ = true;
    }
    break;

  case SDL_EVENT_MOUSE_BUTTON_UP:
    if (is_dragging_leagues_) {
      is_dragging_leagues_ = false;
      event_handled = true;
    }
    if (is_dragging_teams_) {
      is_dragging_teams_ = false;
      event_handled = true;
    }
    break;

  case SDL_EVENT_MOUSE_WHEEL:
    mouse_point = {(float)event.wheel.x, (float)event.wheel.y};
    if (SDL_PointInRectFloat(&mouse_point, &league_list_rect_)) {
      league_scroll_offset_ += event.wheel.x * 20;
      setupLeagueAndTeamButtons();
      event_handled = true;
    } else if (SDL_PointInRectFloat(&mouse_point, &team_list_rect_)) {
      team_scroll_offset_ += event.wheel.y * 20;
      setupLeagueAndTeamButtons();
      event_handled = true;
    }
    break;
  }

  if (!event_handled) {
    if (event.type == SDL_EVENT_MOUSE_MOTION) {
      button_manager.handleMouseMove(event.motion.x, event.motion.y);
    } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
      button_manager.handleMouseClick(event.button.x, event.button.y);
    }
  }
}

void TeamSelectionScene::update(float deltaTime) { (void)deltaTime; }

void TeamSelectionScene::render() {
  int width, height;
  SDL_GetWindowSizeInPixels(getWindow(), &width, &height);

  SDL_SetRenderDrawColor(getRenderer(), 50, 50, 50, 255);
  SDL_RenderClear(getRenderer());

  if (title_font) {
    SDL_Color textColor = {255, 255, 255, 255};
    SDL_Surface *textSurface =
        TTF_RenderText_Solid(title_font, "Select Your Team", 0, textColor);
    if (textSurface) {
      SDL_Texture *textTexture =
          SDL_CreateTextureFromSurface(getRenderer(), textSurface);
      if (textTexture) {
        SDL_FRect textRect = {(width - static_cast<float>(textSurface->w)) /
                                  2.0f,
                              50.0f, static_cast<float>(textSurface->w),
                              static_cast<float>(textSurface->h)};
        SDL_RenderTexture(getRenderer(), textTexture, NULL, &textRect);
        SDL_DestroyTexture(textTexture);
      }
      SDL_DestroySurface(textSurface);
    }
  }

  // Set clipping rect for league buttons
  SDL_Rect league_clip_rect = {static_cast<int>(league_list_rect_.x),
                               static_cast<int>(league_list_rect_.y),
                               static_cast<int>(league_list_rect_.w),
                               static_cast<int>(league_list_rect_.h)};
  SDL_SetRenderClipRect(getRenderer(), &league_clip_rect);
  button_manager.render();

  // Set clipping rect for team buttons
  if (selected_league_id) {
    SDL_Rect team_clip_rect = {static_cast<int>(team_list_rect_.x),
                               static_cast<int>(team_list_rect_.y),
                               static_cast<int>(team_list_rect_.w),
                               static_cast<int>(team_list_rect_.h)};
    SDL_SetRenderClipRect(getRenderer(), &team_clip_rect);
    button_manager.render();
  }

  // Reset clipping
  SDL_SetRenderClipRect(getRenderer(), NULL);

  renderLeagueScrollbar();
  renderTeamScrollbar();
}

void TeamSelectionScene::renderLeagueScrollbar() {
  int width, height;
  SDL_GetWindowSizeInPixels(getWindow(), &width, &height);
  float viewWidth = static_cast<float>(width);
  float leagueButtonWidth = 250.0f;
  float leaguePadding = 20.0f;
  float totalLeagueWidth =
      available_leagues.size() * (leagueButtonWidth + leaguePadding) -
      leaguePadding;

  if (totalLeagueWidth > viewWidth) {
    float scrollbar_area_y = league_list_rect_.y + league_list_rect_.h + 5;
    float scrollbar_area_height = 5;
    SDL_FRect scrollbar_bg = {league_list_rect_.x, scrollbar_area_y,
                              league_list_rect_.w, scrollbar_area_height};

    SDL_SetRenderDrawColor(getRenderer(), 100, 100, 100,
                           255); // Scrollbar background
    SDL_RenderFillRect(getRenderer(), &scrollbar_bg);

    float max_scroll_offset = totalLeagueWidth - viewWidth + 2 * leaguePadding;
    float handle_width = (viewWidth / totalLeagueWidth) * league_list_rect_.w;
    float handle_x =
        league_list_rect_.x + (league_scroll_offset_ / max_scroll_offset) *
                                  (league_list_rect_.w - handle_width);

    SDL_FRect scrollbar_handle = {handle_x, scrollbar_area_y, handle_width,
                                  scrollbar_area_height};
    SDL_SetRenderDrawColor(getRenderer(), 150, 150, 150,
                           255); // Scrollbar handle
    SDL_RenderFillRect(getRenderer(), &scrollbar_handle);
  }
}

void TeamSelectionScene::renderTeamScrollbar() {
  if (!selected_league_id)
    return;

  float viewHeight = 600.0f;
  float teamButtonHeight = 50.0f;
  float teamPadding = 15.0f;
  int teams_per_row = 4;
  int num_rows = (available_teams.size() + teams_per_row - 1) / teams_per_row;
  float totalTeamHeight =
      num_rows * (teamButtonHeight + teamPadding) - teamPadding;
  float teamListVisibleHeight =
      viewHeight - (league_list_rect_.y + league_list_rect_.h + 50);

  if (totalTeamHeight > teamListVisibleHeight) {
    float scrollbar_area_x = team_list_rect_.x + team_list_rect_.w + 5;
    float scrollbar_area_width = 8;

    SDL_FRect scrollbar_bg = {scrollbar_area_x, team_list_rect_.y, scrollbar_area_width,
                              team_list_rect_.h};
    float max_scroll_offset = totalTeamHeight - team_list_rect_.h;
    float handle_height =
        (team_list_rect_.h / totalTeamHeight) * team_list_rect_.h;
    float handle_y =
        team_list_rect_.y + (team_scroll_offset_ / max_scroll_offset) *
                                (team_list_rect_.h - handle_height);
    SDL_SetRenderDrawColor(getRenderer(), 100, 100, 100,
                           255); // Scrollbar background
    SDL_RenderFillRect(getRenderer(), &scrollbar_bg);

    SDL_FRect scrollbar_handle = {scrollbar_area_x, handle_y,
                                  scrollbar_area_width, handle_height};
    SDL_SetRenderDrawColor(getRenderer(), 250, 150, 190,
                           255); // Scrollbar handle
    SDL_RenderFillRect(getRenderer(), &scrollbar_handle);
  }
}

void TeamSelectionScene::loadAvailableLeagues() {
  available_leagues = parent_view->getController().getLeagues();
}

void TeamSelectionScene::setupLeagueAndTeamButtons() {
  button_manager.clearButtons();
  league_button_ids.clear();

  int width, height;
  SDL_GetWindowSizeInPixels(getWindow(), &width, &height);
  float viewWidth = static_cast<float>(width);

  float leagueButtonStartY = 150.0f;
  float leagueButtonHeight = 80.0f;
  float leagueButtonWidth = 250.0f;
  float leaguePadding = 20.0f;

  // Calculate total width needed for all league buttons
  float totalLeagueWidth =
      available_leagues.size() * (leagueButtonWidth + leaguePadding);

  // The visible area for leagues with padding
  float leagueVisibleWidth = viewWidth - 40.0f; // 20px padding on each side
  float leagueAreaStartX = 20.0f;

  league_list_rect_ = {leagueAreaStartX, leagueButtonStartY, leagueVisibleWidth,
                       leagueButtonHeight};

  float max_scroll_offset = totalLeagueWidth > leagueVisibleWidth
                                ? totalLeagueWidth - leagueVisibleWidth
                                : 0;
  league_scroll_offset_ =
      std::max(0.0f, std::min(league_scroll_offset_, max_scroll_offset));

  // Start rendering buttons from the left edge of the visible area
  float currentX = leagueAreaStartX - league_scroll_offset_;

  for (size_t i = 0; i < available_leagues.size(); ++i) {
    const League &league = available_leagues[i].get();

    int buttonId = button_manager.addButton(
        currentX, leagueButtonStartY, leagueButtonWidth, leagueButtonHeight,
        league.getName(),
        [this, league_id = league.getId()]() { this->onLeagueSelected(league_id); });
    league_button_ids.push_back(buttonId);

    currentX += leagueButtonWidth + leaguePadding;
  }

  if (selected_league_id) {
    auto it = std::find_if(available_leagues.begin(), available_leagues.end(),
                           [this](const auto &league_ref) {
                             return league_ref.get().getId() ==
                                    selected_league_id.value();
                           });
    if (it != available_leagues.end()) {
      size_t index = std::distance(available_leagues.begin(), it);
      if (index < league_button_ids.size()) {
        button_manager.setButtonSelected(league_button_ids[index], true);
      }
    }

    loadAvailableTeams();
    int teams_per_row = 4;
    // float teamButtonWidth = 250.0f;
    float teamButtonHeight = 50.0f;
    float teamPadding = 15.0f;
    float teamAreaWidth = viewWidth - 40.0f; // 20px padding on each side
    float teamStartX = 20.0f;

    float teamStartY = leagueButtonStartY + leagueButtonHeight + 50.0f;
    float teamListHeight = height - teamStartY - 50.0f;
    team_list_rect_ = {teamStartX, teamStartY, teamAreaWidth, teamListHeight};

    int num_rows = (available_teams.size() + teams_per_row - 1) / teams_per_row;
    float totalTeamHeight =
        num_rows * (teamButtonHeight + teamPadding) - teamPadding;
    float max_team_scroll_offset =
        totalTeamHeight > teamListHeight ? totalTeamHeight - teamListHeight : 0;
    team_scroll_offset_ =
        std::max(0.0f, std::min(team_scroll_offset_, max_team_scroll_offset));

    float dynamicTeamButtonWidth = (teamAreaWidth - (teams_per_row - 1) * teamPadding) / teams_per_row;

    for (size_t i = 0; i < available_teams.size(); ++i) {
      const Team &team = available_teams[i].get();
      int row = i / teams_per_row;
      int col = i % teams_per_row;
      float buttonX = teamStartX + col * (dynamicTeamButtonWidth + teamPadding);
      float buttonY = teamStartY + row * (teamButtonHeight + teamPadding) -
                      team_scroll_offset_;

      button_manager.addButton(buttonX, buttonY, dynamicTeamButtonWidth,
                               teamButtonHeight, team.getName(),
                               [this, team_id = team.getId()]() {
                                 this->onTeamSelected(team_id);
                               });
    }
  }
}

void TeamSelectionScene::onLeagueSelected(uint8_t league_id) {
  selected_league_id = league_id;
  team_scroll_offset_ = 0.0f;
  setupLeagueAndTeamButtons();
}

void TeamSelectionScene::loadAvailableTeams() {
  if (!selected_league_id)
    return;

  auto all_teams = parent_view->getController().getTeams();
  available_teams.clear();

  for (const auto &team_ref : all_teams) {
    const Team &team = team_ref.get();
    if (team.getLeagueId() == selected_league_id.value() &&
        team.getName() != FREE_AGENTS_TEAM_NAME) {
      available_teams.push_back(team_ref);
    }
  }
}

void TeamSelectionScene::onTeamSelected(uint16_t team_id) {
  Logger::debug("Team selected: " +
                GameData::instance().getTeam(team_id)->get().getName() + "\n");
  parent_view->getController().selectManagedTeam(team_id);
  parent_view->popScene();
}

SceneID TeamSelectionScene::getID() const { return SceneID::TEAM_SELECTION; }
