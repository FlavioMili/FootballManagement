// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <array>

/**
 * @brief Namespace containing all GUI-related constants.
 */
namespace GUIConstants
{

// Button sizes
/** @brief Default button width. */
constexpr float BUTTON_WIDTH = 120.0f;
/** @brief Default button height. */
constexpr float BUTTON_HEIGHT = 40.0f;

// Menu Button sizes
/** @brief Default menu button width. */
constexpr float MENU_BUTTON_WIDTH = 200.0f;
/** @brief Default menu button height. */
constexpr float MENU_BUTTON_HEIGHT = 50.0f;

// Window layout
/** @brief Center pivot for layout alignment. */
constexpr float CENTER_PIVOT = 0.5f;

/**
 * @brief Represents a screen resolution.
 */
struct Resolution
{
  int width;  /**< The width of the resolution. */
  int height; /**< The height of the resolution. */
};

/** @brief Supported FPS options. */
constexpr std::array<int, 4> FPS_OPTIONS = {60, 90, 120, 144};

/** @brief Supported resolution options. */
constexpr std::array<Resolution, 4> RESOLUTIONS = {
    Resolution{1280, 720}, Resolution{1920, 1080}, Resolution{2560, 1440},
    Resolution{3840, 2160}};

}  // namespace GUIConstants
