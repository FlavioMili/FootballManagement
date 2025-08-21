#include "gui_scene.h"
#include "gui_view.h"

GUIScene::GUIScene(GUIView* guiView) : guiView(guiView) {
}

GUIScene::~GUIScene() {
}

void GUIScene::onEnter() {
  // Default implementation - can be overridden by derived classes
}

void GUIScene::onExit() {
  // Default implementation - can be overridden by derived classes
}

SDL_Renderer* GUIScene::getRenderer() const {
  return guiView ? guiView->getRenderer() : nullptr;
}

SDL_Window* GUIScene::getWindow() const {
  return guiView ? guiView->getWindow() : nullptr;
}

void GUIScene::changeScene(std::unique_ptr<GUIScene> newScene) {
  if (guiView) {
    guiView->changeScene(std::move(newScene));
  }
}

void GUIScene::quit() {
  if (guiView) {
    guiView->quit();
  }
}
