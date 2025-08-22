#include "button_manager.h"
#include <algorithm>

ButtonManager::ButtonManager(SDL_Renderer* renderer, TTF_Font* font) 
: renderer(renderer), font(font) {
}

ButtonManager::~ButtonManager() {
  // Buttons are automatically cleaned up
}

int ButtonManager::addButton(const SDL_FRect& rect, const char* label, std::function<void()> callback) {
  Button button;
  button.rect = rect;
  button.label = label;
  button.onClick = callback;
  button.style = defaultStyle;
  button.id = nextButtonId++;

  buttons.push_back(button);
  return button.id;
}

int ButtonManager::addButton(float x, float y, float w, float h, const char* label, std::function<void()> callback) {
  return addButton({x, y, w, h}, label, callback);
}

void ButtonManager::removeButton(int buttonId) {
  buttons.erase(
    std::remove_if(buttons.begin(), buttons.end(),
                   [buttonId](const Button& btn) { return btn.id == buttonId; }),
    buttons.end()
  );
}

void ButtonManager::clearButtons() {
  buttons.clear();
}

void ButtonManager::setButtonStyle(int buttonId, const ButtonStyle& style) {
  auto it = std::find_if(buttons.begin(), buttons.end(),
                         [buttonId](Button& btn) { return btn.id == buttonId; });
  if (it != buttons.end()) {
    it->style = style;
  }
}

void ButtonManager::setButtonVisible(int buttonId, bool visible) {
  auto it = std::find_if(buttons.begin(), buttons.end(),
                         [buttonId](Button& btn) { return btn.id == buttonId; });
  if (it != buttons.end()) {
    it->isVisible = visible;
  }
}

void ButtonManager::setButtonText(int buttonId, const char* text) {
  auto it = std::find_if(buttons.begin(), buttons.end(),
                         [buttonId](Button& btn) { return btn.id == buttonId; });
  if (it != buttons.end()) {
    it->label = text;
  }
}

void ButtonManager::handleMouseMove(float x, float y) {
  for (auto& button : buttons) {
    if (!button.isVisible) continue;
    button.isHovered = isPointInButton(x, y, button);
  }
}

bool ButtonManager::handleMouseClick(float x, float y) {
  for (const auto& button : buttons) {
    if (!button.isVisible) continue;

    if (isPointInButton(x, y, button)) {
      if (button.onClick) {
        button.onClick();
      }
      return true; // Button was clicked
    }
  }
  return false; // No button was clicked
}

void ButtonManager::render() {
  for (const auto& button : buttons) {
    if (button.isVisible) {
      renderButton(button);
    }
  }
}

void ButtonManager::renderButton(const Button& button) {
  const ButtonStyle& style = button.style;

  // Choose colors based on hover state
  SDL_Color bgColor = button.isHovered ? style.hoverBackgroundColor : style.backgroundColor;
  SDL_Color borderColor = button.isHovered ? style.hoverBorderColor : style.borderColor;

  // Draw background
  SDL_SetRenderDrawColor(renderer, bgColor.r, bgColor.g, bgColor.b, bgColor.a);
  SDL_RenderFillRect(renderer, &button.rect);

  // Draw border
  if (style.hasBorder) {
    SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, borderColor.a);
    SDL_RenderRect(renderer, &button.rect);
  }

  // Draw text
  if (font && button.label && *button.label) {
    SDL_Surface* textSurface = TTF_RenderText_Solid(font, button.label, strlen(button.label), style.textColor);

    if (textSurface) {
      SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);

      if (textTexture) {
        // Center text in button
        SDL_FRect textRect = {
          button.rect.x + (button.rect.w - textSurface->w) / 2.0f,
          button.rect.y + (button.rect.h - textSurface->h) / 2.0f,
          (float)textSurface->w,
          (float)textSurface->h
        };

        SDL_RenderTexture(renderer, textTexture, nullptr, &textRect);
        SDL_DestroyTexture(textTexture);
      }
      SDL_DestroySurface(textSurface);
    }
  }
}

bool ButtonManager::isPointInButton(float x, float y, const Button& button) const {
  return (x >= button.rect.x && x <= button.rect.x + button.rect.w &&
  y >= button.rect.y && y <= button.rect.y + button.rect.h);
}



