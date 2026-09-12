// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <SDL3/SDL.h>
#include <imgui.h>

/**
 * @brief Sets the SDL render scale for a scope and restores the previous one.
 *
 * The SDL_Renderer ImGui backend only projects clip rectangles by the
 * framebuffer scale; vertices stay in window coordinates. Drawing ImGui with
 * the render scale set to DisplayFramebufferScale makes HiDPI outputs
 * (Wayland scale 2, macOS Retina) fill the window instead of a corner.
 */
class ScopedRenderScale
{
 public:
  ScopedRenderScale(SDL_Renderer* renderer, ImVec2 scale) : target(renderer)
  {
    SDL_GetRenderScale(target, &previous.x, &previous.y);
    SDL_SetRenderScale(target, scale.x, scale.y);
  }
  ~ScopedRenderScale() { SDL_SetRenderScale(target, previous.x, previous.y); }
  ScopedRenderScale(const ScopedRenderScale&) = delete;
  ScopedRenderScale& operator=(const ScopedRenderScale&) = delete;

 private:
  SDL_Renderer* target;
  ImVec2 previous{1.0f, 1.0f};
};
