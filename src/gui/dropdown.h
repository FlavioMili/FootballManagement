// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <functional>
#include <string>
#include <vector>

/**
 * @brief Style configuration for a dropdown.
 */
struct DropdownStyle
{
  SDL_Color backgroundColor = {60, 60, 60, 255};
  SDL_Color borderColor = {100, 100, 100, 255};
  SDL_Color textColor = {255, 255, 255, 255};
  SDL_Color hoverColor = {80, 80, 80, 255};
  SDL_Color arrowColor = {200, 200, 200, 255};
};

/**
 * @brief Represents a UI dropdown menu.
 */
class Dropdown
{
 public:
  /**
   * @brief Constructs a new Dropdown.
   * @param dropdown_renderer The SDL renderer used for drawing.
   * @param dropdown_font The font used for text.
   * @param rect The rectangle defining the dropdown's position and size.
   * @param dropdown_options The list of string options in the dropdown.
   */
  Dropdown(SDL_Renderer* dropdown_renderer, TTF_Font* dropdown_font,
           const SDL_FRect& rect,
           const std::vector<std::string>& dropdown_options);

  /**
   * @brief Destroys the Dropdown.
   */
  ~Dropdown();

  // Disable copy and move to prevent accidental memory issues
  Dropdown(const Dropdown&) = delete;
  Dropdown& operator=(const Dropdown&) = delete;
  Dropdown(Dropdown&&) = delete;
  Dropdown& operator=(Dropdown&&) = delete;

  /**
   * @brief Handles SDL events for the dropdown.
   * @param event The SDL event to process.
   * @return True if the event was handled by the dropdown.
   */
  bool handleEvent(const SDL_Event& event);

  /**
   * @brief Renders the dropdown.
   */
  void render();

  /**
   * @brief Updates the dropdown logic.
   * @param deltaTime The time elapsed since the last update.
   */
  void update(float deltaTime);

  /**
   * @brief Closes the dropdown if it is open.
   */
  void close();

  /**
   * @brief Checks if the dropdown is currently open.
   * @return True if open, false otherwise.
   */
  bool isDropdownOpen() const;

  /**
   * @brief Gets the index of the currently selected option.
   * @return The selected index.
   */
  int getSelectedIndex() const;

  /**
   * @brief Gets the text of the currently selected option.
   * @return The selected string.
   */
  std::string getSelectedOption() const;

  /**
   * @brief Sets the selected option by index.
   * @param index The index to select.
   */
  void setSelectedIndex(int index);

  /**
   * @brief Sets the callback invoked when the selection changes.
   * @param callback The function to call, passing the new index and text.
   */
  void setOnSelectionChanged(
      std::function<void(int, const std::string&)> callback);

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
