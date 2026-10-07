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
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "global/languages.h"
#include "model/match_tuning.h"

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
  bool vsync = false;               /**< Wait for the display refresh */

  // Appearance (applied live by the GUI theme)
  int theme_preset = 0;           /**< Index of the colour preset */
  bool club_accent = true;        /**< Accent follows the managed club */
  uint32_t accent_rgb = 0x21A663; /**< Custom accent colour, 0xRRGGBB */
  float ui_scale = 0.0f;          /**< 0 = automatic (display scale) */
  bool compact_density = false;   /**< Denser tables and controls */
  bool reduced_motion = false;    /**< Avoid non-essential animation */
  int color_vision = 0;           /**< Theme::ColorVision: status colour hues */
  float text_scale = 1.0f;        /**< Text size on top of the UI scale */

  // Guidance
  bool screen_tips = true;       /**< One-line tip on a screen's first visit */
  uint64_t screen_tips_seen = 0; /**< Bit per NavSection already explained */

  // Saving (applied to the controller's AutosavePolicy)
  int autosave_frequency = 2; /**< AutosaveFrequency: 0 Off .. 5 season end */
  int autosave_backups = 3;   /**< Previous saves kept per slot, 0-9 */

  // Audio (linear gains 0-1, applied live by the match audio)
  float master_volume = 0.8f;
  float crowd_volume = 0.8f;
  float effects_volume = 0.8f;
  bool audio_muted = false;

  // Live match
  bool pause_for_match_changes = true; /**< Substitutions/tactics pause play */
  bool pause_at_breaks = true; /**< Managed match stops at half-time etc. */

  // Play mode (controlling the team on the pitch)
  int play_half_minutes = MatchTuning::Timing::
      DEFAULT_PLAY_HALF_MINUTES; /**< Real minutes per regulation half, 3-15 */
  bool play_mode = true;         /**< Offer Play / Take control in matches */
  int play_auto_switch = 2;    /**< PlayAutoSwitch: 0 off, 1 assisted, 2 auto */
  int play_pass_assist = 1;    /**< 0 none, 1 normal, 2 strong */
  float play_dead_zone = 0.2f; /**< Gamepad stick dead zone, 0.05-0.5 */

  // Controls: action id -> chord names (primary, alternate); only bindings
  // that differ from the defaults (see src/gui/input_actions.h).
  std::map<std::string, std::vector<std::string>> key_bindings;

  // Table views: table key -> label keys of the columns the user hid.
  std::map<std::string, std::vector<std::string>, std::less<>> hidden_columns;
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
