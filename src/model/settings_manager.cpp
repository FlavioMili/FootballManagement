// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "settings_manager.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

#include "global/language_manager.h"
#include "global/paths.h"
#include "global/runtime_paths.h"

using json = nlohmann::json;

SettingsManager::SettingsManager() {}

SettingsManager* SettingsManager::instance()
{
  static SettingsManager instance;
  return &instance;
}

void SettingsManager::load()
{
  std::ifstream in(RuntimePaths::settingsPath());
  if (!in)
  {
    std::cerr << "Settings file not found, using defaults.\n";
    return;
  }

  try
  {
    json j;
    in >> j;

    settings_.language = j.value("language", settings_.language);
    const auto resolution =
        j.value("resolution", std::vector<int>{settings_.resolution_width,
                                               settings_.resolution_height});
    if (resolution.size() == 2 && resolution[0] >= 640 && resolution[1] >= 480)
    {
      settings_.resolution_width = resolution[0];
      settings_.resolution_height = resolution[1];
    }
    settings_.fullscreen = j.value("fullscreen", settings_.fullscreen);
    settings_.fps_limit =
        std::clamp(j.value("fps_limit", settings_.fps_limit), 15, 360);
    settings_.theme_preset =
        std::clamp(j.value("theme_preset", settings_.theme_preset), 0, 4);
    settings_.club_accent = j.value("club_accent", settings_.club_accent);
    settings_.accent_rgb =
        j.value("accent_rgb", settings_.accent_rgb) & 0xFFFFFFU;
    const float scale = j.value("ui_scale", settings_.ui_scale);
    settings_.ui_scale = scale <= 0.0f ? 0.0f : std::clamp(scale, 0.75f, 2.0f);
    settings_.compact_density =
        j.value("compact_density", settings_.compact_density);
    settings_.reduced_motion =
        j.value("reduced_motion", settings_.reduced_motion);
  }
  catch (const json::exception& exception)
  {
    std::cerr << "Invalid settings file, using defaults: " << exception.what()
              << '\n';
  }

  LanguageManager::instance().loadLanguage(settings_.language);
}

void SettingsManager::save() const
{
  json j;
  j["language"] = settings_.language;
  j["resolution"] = {settings_.resolution_width, settings_.resolution_height};
  j["fullscreen"] = settings_.fullscreen;
  j["fps_limit"] = settings_.fps_limit;
  j["theme_preset"] = settings_.theme_preset;
  j["club_accent"] = settings_.club_accent;
  j["accent_rgb"] = settings_.accent_rgb;
  j["ui_scale"] = settings_.ui_scale;
  j["compact_density"] = settings_.compact_density;
  j["reduced_motion"] = settings_.reduced_motion;

  std::ofstream out(RuntimePaths::settingsPath());
  out << j.dump(2);
}

void SettingsManager::apply(SDL_Window* window)
{
  if (!window) return;

  SDL_SetWindowFullscreen(window, settings_.fullscreen ? true : false);
  if (!settings_.fullscreen)
  {
    SDL_SetWindowSize(window, settings_.resolution_width,
                      settings_.resolution_height);
  }
  // fps limit: handled by main loop

  LanguageManager::instance().loadLanguage(settings_.language);
}
