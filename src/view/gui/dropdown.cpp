// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "dropdown.h"

Dropdown::Dropdown(SDL_Renderer* renderer, TTF_Font* font,
                   const SDL_FRect& rect, const std::vector<std::string>& options)
: renderer(renderer), font(font), mainRect(rect), options(options) {

  optionRects.resize(options.size());
  for (size_t i = 0; i < options.size(); ++i) {
    optionRects[i] = {mainRect.x, mainRect.y + mainRect.h * (float)(i + 1), mainRect.w, mainRect.h};
  }

  createTextures();
}

Dropdown::~Dropdown() {
  freeTextures();
}

void Dropdown::close() {
  isOpen = false;
}

bool Dropdown::isDropdownOpen() const {
  return isOpen;
}

bool Dropdown::handleEvent(const SDL_Event& event) {
  if (event.type == SDL_EVENT_MOUSE_MOTION) {
    if (isOpen) {
      hoveredIndex = -1;
      SDL_FPoint mouse_point = {event.motion.x, event.motion.y};
      for (size_t i = 0; i < optionRects.size(); ++i) {
        if (SDL_PointInRectFloat(&mouse_point, &optionRects[i])) {
          hoveredIndex = i;
          break;
        }
      }
    }
  } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
    SDL_FPoint mouse_point = {event.button.x, event.button.y};

    // Case 1: Click on the main button to toggle the dropdown.
    if (SDL_PointInRectFloat(&mouse_point, &mainRect)) {
      isOpen = !isOpen;
      return true; // Event handled.
    }

    // Case 2: Dropdown is open, check for clicks on options or outside.
    if (isOpen) {
      // Check if an option was clicked.
      for (size_t i = 0; i < optionRects.size(); ++i) {
        if (SDL_PointInRectFloat(&mouse_point, &optionRects[i])) {
          setSelectedIndex(i);
          isOpen = false;
          return true; // Event handled.
        }
      }

      // If we reach here, the click was outside the options.
      // Since the dropdown was open, we should close it and consume the click.
      isOpen = false;
      return true;
    }
  }
  return false;
}

void Dropdown::render() {
  SDL_SetRenderDrawColor(renderer, 
                         style.backgroundColor.r,
                         style.backgroundColor.g,
                         style.backgroundColor.b,
                         style.backgroundColor.a);
  SDL_RenderFillRect(renderer, &mainRect);
  SDL_SetRenderDrawColor(renderer,
                         style.borderColor.r,
                         style.borderColor.g,
                         style.borderColor.b, 
                         style.borderColor.a);
  SDL_RenderRect(renderer, &mainRect);

  if (selectedOptionTexture) {
    float textW, textH;
    SDL_GetTextureSize(selectedOptionTexture, &textW, &textH);
    SDL_FRect textRect = {mainRect.x + 10, mainRect.y + (mainRect.h - textH) / 2.0f, textW, textH};
    SDL_RenderTexture(renderer, selectedOptionTexture, nullptr, &textRect);
  }

  renderArrow(mainRect);

  if (isOpen) {
    for (size_t i = 0; i < optionRects.size(); ++i) {
      if (i == (size_t)hoveredIndex) {
        SDL_SetRenderDrawColor(renderer,
                               style.hoverColor.r,
                               style.hoverColor.g, 
                               style.hoverColor.b,
                               style.hoverColor.a);
      } else {
        SDL_SetRenderDrawColor(renderer,
                               style.backgroundColor.r,
                               style.backgroundColor.g,
                               style.backgroundColor.b,
                               style.backgroundColor.a);
      }
      SDL_RenderFillRect(renderer, &optionRects[i]);
      SDL_SetRenderDrawColor(renderer, 
                             style.borderColor.r,
                             style.borderColor.g,
                             style.borderColor.b,
                             style.borderColor.a);
      SDL_RenderRect(renderer, &optionRects[i]);

      if (optionTextures[i]) {
        float textW, textH;
        SDL_GetTextureSize(optionTextures[i], &textW, &textH);
        SDL_FRect textRect = {optionRects[i].x + 10, optionRects[i].y + (optionRects[i].h - textH) / 2.0f, textW, textH};
        SDL_RenderTexture(renderer, optionTextures[i], nullptr, &textRect);
      }
    }
  }
}

void Dropdown::update(float deltaTime) {
  (void)deltaTime;
}

int Dropdown::getSelectedIndex() const {
  return selectedIndex;
}

std::string Dropdown::getSelectedOption() const {
  if (selectedIndex >= 0 && selectedIndex < (int)options.size()) {
    return options[selectedIndex];
  }
  return "";
}

void Dropdown::setSelectedIndex(int index) {
  if (index >= 0 && index < (int)options.size() && index != selectedIndex) {
    selectedIndex = index;
    if (onSelectionChangedCallback) {
      onSelectionChangedCallback(selectedIndex, options[selectedIndex]);
    }

    // Re-create the selected option texture
    if (selectedOptionTexture) {
      SDL_DestroyTexture(selectedOptionTexture);
      selectedOptionTexture = nullptr;
    }
    SDL_Surface* surf = TTF_RenderText_Blended(font, options[selectedIndex].c_str(), 0, style.textColor);
    if (surf) {
      selectedOptionTexture = SDL_CreateTextureFromSurface(renderer, surf);
      SDL_DestroySurface(surf);
    }
  }
}

void Dropdown::setOnSelectionChanged(std::function<void(int, const std::string&)> callback) {
  onSelectionChangedCallback = std::move(callback);
}

void Dropdown::createTextures() {
  freeTextures(); // Clear any existing textures
  optionTextures.resize(options.size(), nullptr);

  for (size_t i = 0; i < options.size(); ++i) {
    SDL_Surface* surf = TTF_RenderText_Blended(font, options[i].c_str(), 0, style.textColor);
    if (surf) {
      optionTextures[i] = SDL_CreateTextureFromSurface(renderer, surf);
      SDL_DestroySurface(surf);
    }
  }

  if (selectedIndex >= 0 && selectedIndex < (int)options.size()) {
    SDL_Surface* surf = TTF_RenderText_Blended(font, options[selectedIndex].c_str(), 0, style.textColor);
    if (surf) {
      selectedOptionTexture = SDL_CreateTextureFromSurface(renderer, surf);
      SDL_DestroySurface(surf);
    }
  }
}

void Dropdown::freeTextures() {
  if (selectedOptionTexture) {
    SDL_DestroyTexture(selectedOptionTexture);
    selectedOptionTexture = nullptr;
  }
  for (auto& texture : optionTextures) {
    if (texture) {
      SDL_DestroyTexture(texture);
      texture = nullptr;
    }
  }
  optionTextures.clear();
}

void Dropdown::renderArrow(const SDL_FRect& buttonRect) const {
  float arrowSize = buttonRect.h / 4.0f;
  float arrowX = buttonRect.x + buttonRect.w - arrowSize * 2.0f;
  float arrowY = buttonRect.y + (buttonRect.h - arrowSize) / 2.0f;

  std::vector<SDL_Vertex> vertices(3);
  SDL_FColor color = { style.arrowColor.r / 255.0f,
    style.arrowColor.g / 255.0f,
    style.arrowColor.b / 255.0f,
    style.arrowColor.a / 255.0f };

  if (isOpen) { // Up arrow
    vertices[0].position = {arrowX, arrowY + arrowSize};
    vertices[0].color = color;
    vertices[0].tex_coord = {0, 0};

    vertices[1].position = {arrowX + arrowSize * 1.5f, arrowY + arrowSize};
    vertices[1].color = color;
    vertices[1].tex_coord = {0, 0};

    vertices[2].position = {arrowX + arrowSize * 0.75f, arrowY};
    vertices[2].color = color;
    vertices[2].tex_coord = {0, 0};
  } else { // Down arrow
    vertices[0].position = {arrowX, arrowY};
    vertices[0].color = color;
    vertices[0].tex_coord = {0, 0};

    vertices[1].position = {arrowX + arrowSize * 1.5f, arrowY};
    vertices[1].color = color;
    vertices[1].tex_coord = {0, 0};

    vertices[2].position = {arrowX + arrowSize * 0.75f, arrowY + arrowSize};
    vertices[2].color = color;
    vertices[2].tex_coord = {0, 0};
  }
  SDL_RenderGeometry(renderer, nullptr, vertices.data(), vertices.size(), nullptr, 0);
}
