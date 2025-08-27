#include "button_manager.h"
#include <algorithm>

ButtonManager::ButtonManager(SDL_Renderer* renderer, TTF_Font* font) 
: renderer(renderer), font(font) {
}

ButtonManager::~ButtonManager() {
  for (auto& btn : buttons) {
    if (btn.textTexture) {
      SDL_DestroyTexture(btn.textTexture);
      btn.textTexture = nullptr;
    }
  }
}

int ButtonManager::addButton(const SDL_FRect& rect, const std::string label, std::function<void()> callback) {
  Button button;
  button.rect = rect;
  button.label = label;
  button.onClick = callback;
  button.style = defaultStyle;
  button.id = nextButtonId++;

  createButtonTexture(button);

  buttons.push_back(button);
  return button.id;
}

int ButtonManager::addButton(float x, float y, float w, float h, const std::string label, std::function<void()> callback) {
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

void ButtonManager::setButtonText(int buttonId, const std::string text) {
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

void ButtonManager::renderButton(const Button& btn) {
  if (!btn.isVisible) return;

  SDL_SetRenderDrawColor(renderer,
                         btn.style.backgroundColor.r,
                         btn.style.backgroundColor.g,
                         btn.style.backgroundColor.b,
                         btn.style.backgroundColor.a);
  SDL_RenderFillRect(renderer, &btn.rect);

  if (btn.style.hasBorder) {
    SDL_SetRenderDrawColor(renderer,
                           btn.style.borderColor.r,
                           btn.style.borderColor.g,
                           btn.style.borderColor.b,
                           btn.style.borderColor.a);
    SDL_RenderRect(renderer, &btn.rect);
  }

  if (btn.textTexture)
    SDL_RenderTexture(renderer, btn.textTexture, nullptr, &btn.textRect);
}

bool ButtonManager::isPointInButton(float x, float y, const Button& button) const {
  return (x >= button.rect.x && x <= button.rect.x + button.rect.w &&
  y >= button.rect.y && y <= button.rect.y + button.rect.h);
}

void ButtonManager::createButtonTexture(Button& btn) {
  if (!font || !renderer) return;
  if (btn.textTexture) SDL_DestroyTexture(btn.textTexture);

  SDL_Color color = btn.style.textColor;
  SDL_Surface* surf = TTF_RenderText_Blended(font, btn.label.c_str(), 0, color);
  if (!surf) return;

  btn.textTexture = SDL_CreateTextureFromSurface(renderer, surf);
  btn.textRect = {
    btn.rect.x + (btn.rect.w - surf->w) / 2.0f,
    btn.rect.y + (btn.rect.h - surf->h) / 2.0f,
    static_cast<float>(surf->w),
    static_cast<float>(surf->h)
  };
  SDL_DestroySurface(surf);
}

// TODO remove?
// void ButtonManager::addOrderedButtons(const OrderedButtons& ordered) {
//   for (auto& btn : ordered.getButtons()) {
//     addButton(btn.rect, btn.label, btn.onClick);
//   }
// }
