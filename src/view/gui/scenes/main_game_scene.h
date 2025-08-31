#pragma once
#include "view/gui/gui_scene.h"
#include "view/gui/gui_view.h"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

class MainGameScene : public GUIScene {
 public:
  MainGameScene(GUIView* guiView);
  ~MainGameScene();
  void handleEvent(const SDL_Event& event) override;
  void update(float deltaTime) override;
  void render() override;
  void onEnter() override;
  void onExit() override;
  SceneID getID() const override;

 private:
  void renderText(const char* text, float x, float y, SDL_Color color = {255, 255, 255, 255});
  void renderGameUI();
  void createStaticTextures();
  void cleanup();
  SDL_Texture* createTextTexture(const char* text, TTF_Font* textFont, SDL_Color color);
  void renderCachedTexture(SDL_Texture* texture, float centerX, float centerY);

  // Fonts
  TTF_Font* font;
  TTF_Font* smallFont;

  // Cached textures for static text
  SDL_Texture* titleTexture;
  SDL_Texture* gameInfoTexture;
  SDL_Texture* instructionsTexture;
  SDL_Texture* statusTexture;
  SDL_Texture* controlsTexture;
};
