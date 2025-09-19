// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "gui/gui_scene.h"
#include "gui/gui_view.h"

GUIScene::GUIScene(GUIView* guiView_ptr) : guiView(guiView_ptr) {
}

void GUIScene::onEnter() {
  // Default implementation - can be overridden by derived classes
}

void GUIScene::onExit() {
  // Default implementation - can be overridden by derived classes
}

void GUIScene::onResize(int width, int height) {
  // Default implementation - can be overridden by derived classes
  (void)width;
  (void)height;
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
