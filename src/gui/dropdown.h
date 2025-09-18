// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <string>
#include <vector>
#include <functional>

struct DropdownStyle {
  SDL_Color backgroundColor = {60, 60, 60, 255};
  SDL_Color borderColor = {100, 100, 100, 255};
  SDL_Color textColor = {255, 255, 255, 255};
  SDL_Color hoverColor = {80, 80, 80, 255};
  SDL_Color arrowColor = {200, 200, 200, 255};
};

class Dropdown {
 public:
  Dropdown(SDL_Renderer* renderer, TTF_Font* font,
           const SDL_FRect& rect, const std::vector<std::string>& options);
  ~Dropdown();

  // Disable copy and move to prevent accidental memory issues
  Dropdown(const Dropdown&) = delete;
  Dropdown& operator=(const Dropdown&) = delete;
  Dropdown(Dropdown&&) = delete;
  Dropdown& operator=(Dropdown&&) = delete;

  bool handleEvent(const SDL_Event& event);
  void render();
  void update(float deltaTime);
  void close();
  bool isDropdownOpen() const;

  int getSelectedIndex() const;
  std::string getSelectedOption() const;
  void setSelectedIndex(int index);

  void setOnSelectionChanged(std::function<void(int, const std::string&)> callback);

 private:
  void createTextures();
  void freeTextures();
  void renderArrow(const SDL_FRect& buttonRect) const;

  SDL_Renderer* renderer;
  TTF_Font* font;
  SDL_FRect mainRect;
  std::vector<std::string> options;
  std::vector<SDL_FRect> optionRects;
  DropdownStyle style;

  bool isOpen = false;
  int selectedIndex = 0;
  int hoveredIndex = -1;

  SDL_Texture* selectedOptionTexture = nullptr;
  std::vector<SDL_Texture*> optionTextures;

  std::function<void(int, const std::string&)> onSelectionChangedCallback;
};
