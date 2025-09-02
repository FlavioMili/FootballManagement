// ----------------------------------------------------------------------------- 
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "strategy_scene.h"
#include "gui_scene.h"
#include "view/gui/gui_view.h"
#include "global.h"
#include <iostream>
#include <SDL3_ttf/SDL_ttf.h>
#include <algorithm>


SceneID StrategyScene::getID() const { return SceneID::STRATEGY; }


StrategyScene::StrategyScene(GUIView* parent)
    : GUIScene(parent), parent_view(parent),
      button_manager(parent->getRenderer(), TTF_OpenFont(FONT_PATH, 24)) {}

StrategyScene::~StrategyScene() {
    // The font is managed by ButtonManager, no need to close here.
}

void StrategyScene::onEnter() {
    std::cout << "Entering StrategyScene\n";
    loadStrategy();
    setupStrategyControls();
}

void StrategyScene::onExit() {
    std::cout << "Exiting StrategyScene\n";
    saveStrategy();
    button_manager.clearButtons();
}

void StrategyScene::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_EVENT_MOUSE_MOTION) {
        button_manager.handleMouseMove(event.motion.x, event.motion.y);
    } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        button_manager.handleMouseClick(event.button.x, event.button.y);
    }
}

void StrategyScene::update(float deltaTime) {
  (void)deltaTime;
    // No continuous updates needed for this scene yet
}

void StrategyScene::render() {
    SDL_SetRenderDrawColor(getRenderer(), 50, 50, 50, 255); // Dark grey background
    SDL_RenderClear(getRenderer());

    // Render title
    SDL_Color textColor = {255, 255, 255, 255}; // White
    TTF_Font* font = TTF_OpenFont(FONT_PATH, 36);
    if (!font) {
        std::cerr << "Failed to load font: " << SDL_GetError() << "\n";
        return;
    }

    SDL_Surface* textSurface = TTF_RenderText_Solid(font, "Team Strategy", 0, textColor);
    if (!textSurface) {
        std::cerr << "Failed to render text: " << SDL_GetError() << "\n";
        TTF_CloseFont(font);
        return;
    }
    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(getRenderer(), textSurface);
    if (!textTexture) {
        std::cerr << "Failed to create texture from text: " << SDL_GetError() << "\n";
        SDL_DestroySurface(textSurface);
        TTF_CloseFont(font);
        return;
    }

    SDL_FRect textRect = { (1200.0f - textSurface->w) / 2.0f, 50.0f, (float)textSurface->w, (float)textSurface->h };
    SDL_RenderTexture(getRenderer(), textTexture, NULL, &textRect);

    SDL_DestroyTexture(textTexture);
    SDL_DestroySurface(textSurface);
    TTF_CloseFont(font);

    // Render slider values
    renderSliderValue("Pressing", current_sliders.pressing, 150);
    renderSliderValue("Risk Taking", current_sliders.riskTaking, 250);
    renderSliderValue("Offensive Bias", current_sliders.offensiveBias, 350);
    renderSliderValue("Width Usage", current_sliders.widthUsage, 450);
    renderSliderValue("Compactness", current_sliders.compactness, 550);

    button_manager.render();
}

void StrategyScene::setupStrategyControls() {
    float buttonWidth = 50.0f;
    float buttonHeight = 40.0f;
    float xOffset = 800.0f;
    float yOffset = 150.0f;
    float padding = 10.0f;

    // Pressing
    button_manager.addButton(xOffset, yOffset, buttonWidth, buttonHeight, "-", [this]() { updateStrategyValue("pressing", -0.1f); });
    button_manager.addButton(xOffset + buttonWidth + padding, yOffset, buttonWidth, buttonHeight, "+", [this]() { updateStrategyValue("pressing", 0.1f); });

    // Risk Taking
    yOffset += 100.0f;
    button_manager.addButton(xOffset, yOffset, buttonWidth, buttonHeight, "-", [this]() { updateStrategyValue("riskTaking", -0.1f); });
    button_manager.addButton(xOffset + buttonWidth + padding, yOffset, buttonWidth, buttonHeight, "+", [this]() { updateStrategyValue("riskTaking", 0.1f); });

    // Offensive Bias
    yOffset += 100.0f;
    button_manager.addButton(xOffset, yOffset, buttonWidth, buttonHeight, "-", [this]() { updateStrategyValue("offensiveBias", -0.1f); });
    button_manager.addButton(xOffset + buttonWidth + padding, yOffset, buttonWidth, buttonHeight, "+", [this]() { updateStrategyValue("offensiveBias", 0.1f); });

    // Width Usage
    yOffset += 100.0f;
    button_manager.addButton(xOffset, yOffset, buttonWidth, buttonHeight, "-", [this]() { updateStrategyValue("widthUsage", -0.1f); });
    button_manager.addButton(xOffset + buttonWidth + padding, yOffset, buttonWidth, buttonHeight, "+", [this]() { updateStrategyValue("widthUsage", 0.1f); });

    // Compactness
    yOffset += 100.0f;
    button_manager.addButton(xOffset, yOffset, buttonWidth, buttonHeight, "-", [this]() { updateStrategyValue("compactness", -0.1f); });
    button_manager.addButton(xOffset + buttonWidth + padding, yOffset, buttonWidth, buttonHeight, "+", [this]() { updateStrategyValue("compactness", 0.1f); });
}

void StrategyScene::updateStrategyValue(const std::string& slider_name, float delta) {
    if (slider_name == "pressing") {
        current_sliders.pressing = std::max(0.0f, std::min(1.0f, current_sliders.pressing + delta));
    } else if (slider_name == "riskTaking") {
        current_sliders.riskTaking = std::max(0.0f, std::min(1.0f, current_sliders.riskTaking + delta));
    } else if (slider_name == "offensiveBias") {
        current_sliders.offensiveBias = std::max(0.0f, std::min(1.0f, current_sliders.offensiveBias + delta));
    } else if (slider_name == "widthUsage") {
        current_sliders.widthUsage = std::max(0.0f, std::min(1.0f, current_sliders.widthUsage + delta));
    } else if (slider_name == "compactness") {
        current_sliders.compactness = std::max(0.0f, std::min(1.0f, current_sliders.compactness + delta));
    }
}

void StrategyScene::saveStrategy() {
    parent_view->getController().getManagedTeam().getStrategy().setAllSliders(current_sliders);
}

void StrategyScene::loadStrategy() {
    current_sliders = parent_view->getController().getManagedTeam().getStrategy().getSliders();
}

void StrategyScene::renderSliderValue(const std::string& label, float value, int y_pos) {
    SDL_Color textColor = {255, 255, 255, 255}; // White
    TTF_Font* font = TTF_OpenFont(FONT_PATH, 24);
    if (!font) {
        std::cerr << "Failed to load font: " << SDL_GetError() << "\n";
        return;
    }

    char buffer[50];
    snprintf(buffer, sizeof(buffer), "%s: %.1f", label.c_str(), value);

    SDL_Surface* textSurface = TTF_RenderText_Solid(font, buffer, 0, textColor);
    if (!textSurface) {
        std::cerr << "Failed to render text: " << SDL_GetError() << "\n";
        TTF_CloseFont(font);
        return;
    }
    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(getRenderer(), textSurface);
    if (!textTexture) {
        std::cerr << "Failed to create texture from text: " << SDL_GetError() << "\n";
        SDL_DestroySurface(textSurface);
        TTF_CloseFont(font);
        return;
    }

    SDL_FRect textRect = { 50.0f, (float)y_pos, (float)textSurface->w, (float)textSurface->h };
    SDL_RenderTexture(getRenderer(), textTexture, NULL, &textRect);

    SDL_DestroyTexture(textTexture);
    SDL_DestroySurface(textSurface);
    TTF_CloseFont(font);
}
