#pragma once
#include "view/gui/gui_view.h"
#include <SDL3/SDL.h>
#include <memory>

// TODO add scene types
enum class SceneID { MAIN_MENU, SETTINGS, GAME_MENU, };

class GUIScene {
 public:
  GUIScene(GUIView* guiView);
  virtual ~GUIScene();

  // Core scene interface - must be implemented by derived classes
  virtual void handleEvent(const SDL_Event& event) = 0;
  virtual void update(float deltaTime) = 0;
  virtual void render() = 0;

  // Optional lifecycle hooks
  virtual void onEnter();
  virtual void onExit();

  /*
  * This function might be used to check what we are rendering.
  * we would simply do something like this: 
  * MainMenuScene::getID() { return SceneID::MAIN_MENU; }
  */
  virtual SceneID getID() const = 0;

 protected:
  // Helper methods for derived classes
  SDL_Renderer* getRenderer() const;
  SDL_Window* getWindow() const;
  GUIView* getGuiView() const { return guiView; }
  void changeScene(std::unique_ptr<GUIScene> newScene);
  void quit();

  GUIView* guiView;

 private:
};
