#pragma once
#include "view/gui/button_manager.h"
#include "view/gui/gui_scene.h"
#include "view/gui/gui_view.h"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <array>
#include <utility>

class SettingsScene : public GUIScene {
 public:
  SettingsScene(GUIView* guiView);
  ~SettingsScene();
  void handleEvent(const SDL_Event& event) override;
  void update(float deltaTime) override;
  void render() override;
  void onEnter() override;
  void onExit() override;
  SceneID getID() const override;

 private:
  TTF_Font* font;
  std::unique_ptr<ButtonManager> buttonManager;

  // Options
  std::array<int, 4> fpsOptions = {60, 90, 120, 144};
  std::array<std::pair<int, int>, 4> resolutions = {{
    {1280, 720}, {1920, 1080},
    {2560, 1440}, {3840, 2160}, }};

  // Current selections
  int selectedLanguage = 0;
  int selectedFPS = 0;
  int selectedResolution = 0;
  bool fullscreen = false;

  // Button IDs for visual updates
  std::vector<int> languageButtonIds;
  std::vector<int> fpsButtonIds;
  std::vector<int> resolutionButtonIds;
  int fullscreenButtonId = -1;


  // Helper methods
  void initializeCurrentSelections();
  void updateLanguageSelection(int newSelection);
  void updateFPSSelection(int newSelection);
  void updateResolutionSelection(int newSelection);
  void updateFullscreenToggle();
  void applyAndSaveSettings();

  ButtonStyle getDefaultButtonStyle();
  ButtonStyle getSelectedButtonStyle();

  void renderText(const char* text, float x, float y, SDL_Color color = {255, 255, 255, 255});
};
