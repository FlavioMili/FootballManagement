#pragma once
#include "view/gui/gui_scene.h"
#include "view/gui/gui_view.h"
#include <vector>
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

struct Button {
  SDL_FRect rect;
  const char* label;
};

class MainMenuScene : public GUIScene {
 public:
  MainMenuScene(GUIView* guiView);
  ~MainMenuScene();

  void handleEvent(const SDL_Event& event) override;
  void update(float deltaTime) override;
  void render() override;
  void onEnter() override;
  void onExit() override;

  SceneID getID() const override;

 private:
  // Helper methods
  bool isPointInButton(float x, float y, const Button& button);
  void handleButtonClick(int buttonIndex);
  void renderButton(const Button& btn);
  void renderTitle();

  // Resources
  TTF_Font* font;
  std::vector<Button> buttons;
};
