#include "settings_manager.h"
#include "json.hpp"
#include <fstream>
#include <iostream>

using json = nlohmann::json;

SettingsManager::SettingsManager() {
  configFile_ = "assets/config/settings.json";
}

SettingsManager& SettingsManager::instance() {
  static SettingsManager instance;
  return instance;
}

void SettingsManager::load() {
  std::ifstream in(configFile_);
  if (!in) {
    std::cerr << "Settings file not found, using defaults.\n";
    return;
  }

  json j;
  in >> j;

  settings_.language = j.value("language", settings_.language);
  auto res = j.value("resolution", std::vector<int>{settings_.resolution_width, settings_.resolution_height});
  if (res.size() == 2) {
    settings_.resolution_width = res[0];
    settings_.resolution_height = res[1];
  }
  settings_.fullscreen = j.value("fullscreen", settings_.fullscreen);
  settings_.fps_limit = j.value("fps_limit", settings_.fps_limit);
}

void SettingsManager::save() const {
  json j;
  j["language"]   = settings_.language;
  j["resolution"] = {settings_.resolution_width, settings_.resolution_height};
  j["fullscreen"] = settings_.fullscreen;
  j["fps_limit"]  = settings_.fps_limit;

  std::ofstream out(configFile_);
  out << j.dump(4);
}

void SettingsManager::apply(SDL_Window* window) {
  if (!window) return;

  SDL_SetWindowFullscreen(window, settings_.fullscreen ? true : false);
  if (!settings_.fullscreen) {
    SDL_SetWindowSize(window, settings_.resolution_width, settings_.resolution_height);
  }
  // fps limit: handled by main loop
}

