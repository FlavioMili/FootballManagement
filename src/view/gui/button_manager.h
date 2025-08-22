#pragma once
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
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
  SDL_FRect rect;
  const char* label;
  std::function<void()> onClick;
  bool isHovered = false;
  bool isVisible = true;
  ButtonStyle style;
  int id = -1; // Optional ID for identification
};

class ButtonManager {
 public:
  ButtonManager(SDL_Renderer* renderer, TTF_Font* font);
  ~ButtonManager();

  // Button creation and management
  int addButton(const SDL_FRect& rect, const char* label,
                std::function<void()> callback);
  int addButton(float x, float y, float w, float h,
              const char* label, std::function<void()> callback);


  void removeButton(int buttonId);
  void clearButtons();

  // Button modification
  void setButtonStyle(int buttonId, const ButtonStyle& style);
  void setButtonVisible(int buttonId, bool visible);
  void setButtonEnabled(int buttonId, bool enabled);
  void setButtonText(int buttonId, const char* text);

  // Event handling
  void handleMouseMove(float x, float y);
  bool handleMouseClick(float x, float y);

  // Rendering
  void render();

  // Utility
  void setFont(TTF_Font* font) { this->font = font; }
  void setDefaultStyle(const ButtonStyle& style) { defaultStyle = style; }

private:
  void renderButton(const Button& button);
  bool isPointInButton(float x, float y, const Button& button) const;

  SDL_Renderer* renderer;
  TTF_Font* font;
  std::vector<Button> buttons;
  ButtonStyle defaultStyle;
  int nextButtonId = 0;
};

class OrderedButtons {
public:
  OrderedButtons(int padding = 30, int startingX = 100, int startingY = 100, 
                 float width = 30, float height = 100, 
                 ButtonStyle style = ButtonStyle())
  : padding(padding), startingX(startingX), 
    startingY(startingY), width(width), height(height), style(style) {}

  // Base virtual function
  virtual void addButton(const char* text, std::function<void()> callback) {
    // Base class does nothing
    (void) text; (void) callback;
  }

  const std::vector<Button>& getButtons() const { return buttons; }

protected:
  int padding;
  int startingX, startingY;
  float width, height;
  ButtonStyle style;
  std::vector<Button> buttons;
};

// Vertical buttons stack
class VerticalButtons : public OrderedButtons {
public:
  VerticalButtons(int padding = 30, int startingX = 100,
                  int startingY = 100, float width = 60, float height = 30, 
                  ButtonStyle style = ButtonStyle())
  : OrderedButtons(padding, startingX, startingY, width, height, style) {}

  void addButton(const char* text, std::function<void()> callback) override {
    float y = startingY;
    if (!buttons.empty())
      y = buttons.back().rect.y + buttons.back().rect.h + padding;

    Button btn;
    btn.label = text;
    btn.rect = { static_cast<float>(startingX), y, width, height };
    btn.onClick = callback;
    btn.style = style;
    buttons.push_back(btn);
  }
};

// Horizontal buttons stack
class HorizontalButtons : public OrderedButtons {
public:
  HorizontalButtons(int padding = 30, int startingX = 100,
                  int startingY = 100, float width = 60, float height = 30, 
                  ButtonStyle style = ButtonStyle())
  : OrderedButtons(padding, startingX, startingY, width, height, style) {}

  void addButton(const char* text, std::function<void()> callback) override {
    float x = startingX;
    if (!buttons.empty())
      x = buttons.back().rect.x + buttons.back().rect.w + padding;

    Button btn;
    btn.label = text;
    btn.rect = { x, static_cast<float>(startingY), width, height };
    btn.onClick = callback;
    btn.style = style;
    buttons.push_back(btn);
  }
};
