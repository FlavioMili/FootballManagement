// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once
#include <SDL3/SDL.h>

#include <cstdint>

#include "global/languages.h"

/**
 * @struct Settings
 * @brief Stores the game's configuration settings.
 */
struct Settings
{
  Language language = Language::EN; /**< The selected language */
  int resolution_width = 1280;      /**< Screen resolution width */
  int resolution_height = 720;      /**< Screen resolution height */
  bool fullscreen = false;          /**< Fullscreen mode flag */
  int fps_limit = 60;               /**< Frames per second limit */

  // Appearance (applied live by the GUI theme)
  int theme_preset = 0;            /**< Index of the colour preset */
  bool club_accent = true;         /**< Accent follows the managed club */
  uint32_t accent_rgb = 0x21A663;  /**< Custom accent colour, 0xRRGGBB */
  float ui_scale = 0.0f;           /**< 0 = automatic (display scale) */
  bool compact_density = false;    /**< Denser tables and controls */
  bool reduced_motion = false;     /**< Avoid non-essential animation */
};

/**
 * @class SettingsManager
 * @brief Singleton class that manages the application settings.
 */
class SettingsManager
{
 public:
  /**
   * @brief Gets the singleton instance of the SettingsManager.
   * @return Pointer to the SettingsManager instance.
   */
  static SettingsManager* instance();

  /**
   * @brief Loads the settings from the configuration file.
   */
  void load();

  /**
   * @brief Saves the current settings to the configuration file.
   */
  void save() const;

  /**
   * @brief Applies the current settings to the given SDL window.
   * @param window Pointer to the SDL_Window to apply settings to.
   */
  void apply(SDL_Window* window);

  /**
   * @brief Gets a reference to the current settings.
   * @return Reference to the Settings object.
   */
  Settings& get() { return settings_; }

  /**
   * @brief Gets a const reference to the current settings.
   * @return Const reference to the Settings object.
   */
  const Settings& get() const { return settings_; }

  // TODO This is pretty horrible
  // check /src/gui/scenes/settings_scene.cpp
  /**
   * @brief Gets a reference to the current settings.
   * @return Reference to the Settings object.
   */
  Settings& getSettings() { return settings_; }

 private:
  SettingsManager();
  Settings settings_;
};
