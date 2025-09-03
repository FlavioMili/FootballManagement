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

struct ButtonStyle {
  SDL_Color backgroundColor = {70, 70, 150, 255};
  SDL_Color borderColor = {100, 100, 200, 255};
  SDL_Color textColor = {255, 255, 255, 255};
  SDL_Color hoverBackgroundColor = {90, 90, 170, 255};
  SDL_Color hoverBorderColor = {120, 120, 220, 255};
  bool hasBorder = true;
  int borderWidth = 1;
};

struct Button {
  SDL_FRect rect {};
  SDL_Texture* textTexture = nullptr;
  SDL_FRect textRect{};
  std::string label;
  std::function<void()> onClick;
  bool isHovered = false;
  bool isVisible = true;
  ButtonStyle style;
  int id = -1;
};

class ButtonManager {
 public:
  ButtonManager(SDL_Renderer* renderer, TTF_Font* font);
  ~ButtonManager();

  int getButtonCount() { return buttons.size(); }
  // Button creation and management
  int addButton(const SDL_FRect& rect, const std::string& label,
                std::function<void()> callback);
  int addButton(const SDL_FRect& rect, const std::string& label,
                ButtonStyle style, std::function<void()> callback);

  int addButton(float x, float y, float w, float h,
                const std::string& label, std::function<void()> callback);
  int addButton(float x, float y, float w, float h, const std::string& label,
                ButtonStyle style, std::function<void()> callback);

  void removeButton(int buttonId);
  void clearButtons();

  // Button modification
  void setButtonStyle(int buttonId, const ButtonStyle& style);
  void setButtonVisible(int buttonId, bool visible);
  void setButtonEnabled(int buttonId, bool enabled);
  void setButtonText(int buttonId, const std::string& text);

  // Event handling
  void handleMouseMove(float x, float y);
  bool handleMouseClick(float x, float y);

  // Rendering
  void render();

  // Utility
  void setFont(TTF_Font* font) { this->font = font; }
  TTF_Font* getFont() { return this->font; }
  void setDefaultStyle(const ButtonStyle& style) { defaultStyle = style; }

  const std::vector<Button> getButtons() { return buttons; }
  void recreateTextures();
  void updateButtonPosition(int buttonIndex, SDL_FRect newRect);
  void updateButtonPositionById(int buttonId, SDL_FRect newRect);

 private:
  void renderButton(const Button& button);
  bool isPointInButton(float x, float y, const Button& button) const;
  void createButtonTexture(Button& btn);

  SDL_Renderer* renderer;
  TTF_Font* font;
  std::vector<Button> buttons;
  ButtonStyle defaultStyle;
  int nextButtonId = 0;
};
