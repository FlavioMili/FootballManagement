#pragma once
#include "view/gui/button_manager.h"
#include "view/gui/gui_scene.h"
#include "view/gui/gui_view.h"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

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
  void renderText(const char* text, float x, float y, SDL_Color color = {255, 255, 255, 255});

  TTF_Font* font;

  std::unique_ptr<ButtonManager> buttonManager;

  // indices for current selections
  int selectedLanguage = 0;
  int selectedFPS = 0;
  int selectedResolution = 0;
  bool fullscreen = false;
 
  // TODO possibly improve these?
  std::vector<int> fpsOptions = {60, 90, 120};
  std::vector<std::pair<int,int>> resolutions = {{1280,720}, {1920,1080}, {2560,1440}};
};
