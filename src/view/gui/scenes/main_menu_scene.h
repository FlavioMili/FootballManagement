#pragma once
#include "view/gui/gui_scene.h"
#include "view/gui/gui_view.h"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "view/gui/button_manager.h"


class MainMenuScene : public GUIScene {
 public:
  MainMenuScene(GUIView* guiView);
  ~MainMenuScene() = default;

  void handleEvent(const SDL_Event& event) override;
  void update(float deltaTime) override;
  void render() override;
  void onEnter() override;
  void onExit() override;

  SceneID getID() const override;

 private:
  // Helper methods
  void handleButtonClick(int buttonIndex);
  void renderTitle();
  void createStaticContent();
  void updateLayout();
  void make_scene();

  std::unique_ptr<ButtonManager> buttonManager;

  TTF_Font* font;
  SDL_Texture* titleTexture;
  SDL_FRect titleRect;
};
