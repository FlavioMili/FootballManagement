// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once
#include "global/languages.h"
#include <SDL3/SDL.h>

struct Settings {
  Language language = Language::EN;
  int resolution_width = 1280;
  int resolution_height = 720;
  bool fullscreen = false;
  int fps_limit = 60;
};

class SettingsManager {
 public:
  static SettingsManager* instance();

  void load();
  void save() const;
  void apply(SDL_Window* window);

  Settings& get() { return settings_; }
  const Settings& get() const { return settings_; }

  // TODO This is pretty horrible 
  // check /src/gui/scenes/settings_scene.cpp
  Settings& getSettings() { return settings_; }

 private:
  SettingsManager();
  Settings settings_;
};
