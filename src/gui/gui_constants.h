// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <array>

namespace GUIConstants
{

// Button sizes
constexpr float BUTTON_WIDTH = 120.0f;
constexpr float BUTTON_HEIGHT = 40.0f;

// Menu Button sizes
constexpr float MENU_BUTTON_WIDTH = 200.0f;
constexpr float MENU_BUTTON_HEIGHT = 50.0f;

// Window layout
constexpr float CENTER_PIVOT = 0.5f;

struct Resolution
{
  int width;
  int height;
};

constexpr std::array<int, 4> FPS_OPTIONS = {60, 90, 120, 144};
constexpr std::array<Resolution, 4> RESOLUTIONS = {
    Resolution{1280, 720}, Resolution{1920, 1080}, Resolution{2560, 1440},
    Resolution{3840, 2160}};

}  // namespace GUIConstants
