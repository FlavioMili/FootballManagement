// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <imgui.h>

#include <cstdint>

/**
 * Named tuning values for the 2D tactical match view. Distances are metres
 * on the pitch unless a name says pixels (logical pixels, multiplied by the
 * UI scale), times are real seconds.
 */
struct MatchRender2DTuning final
{
  /** Mown surface: stripes along the length, cross bands for a checker. */
  struct Grass final
  {
    static constexpr int STRIPES = 18;
    static constexpr int BANDS = 8;
    /** Each cell is split so the lighting gradient stays smooth. */
    static constexpr int CELL_SPLIT = 2;
    static constexpr float STRIPE_CONTRAST = 0.055f;
    static constexpr float BAND_CONTRAST = 0.018f;
    static constexpr float NOISE_METRES = 9.0f;
    static constexpr float NOISE_STRENGTH = 0.08f;
    static constexpr float FINE_NOISE_METRES = 2.6f;
    static constexpr float FINE_NOISE_STRENGTH = 0.045f;
    /** Soft overhead light: bright centre, darker touchlines and corners. */
    static constexpr float LIGHT_CENTRE = 1.06f;
    static constexpr float LIGHT_EDGE = 0.86f;
    static constexpr ImU32 PITCH_COLOR = IM_COL32(50, 128, 52, 255);
    static constexpr ImU32 RUN_OFF_COLOR = IM_COL32(40, 106, 44, 255);
    /** Worn goalmouths and spots: darker, browner turf. */
    static constexpr ImU32 WEAR_COLOR = IM_COL32(84, 78, 44, 255);
    static constexpr std::uint8_t WEAR_ALPHA = 70;
    static constexpr int WEAR_SEGMENTS = 12;
  };

  struct Lines final
  {
    static constexpr float WIDTH_METRES = 0.12f;
    static constexpr float MIN_PIXELS = 1.0f;
    static constexpr ImU32 COLOR = IM_COL32(246, 248, 244, 236);
  };

  struct Goal final
  {
    static constexpr float POST_METRES = 0.12f;
    static constexpr float MIN_POST_PIXELS = 2.0f;
    static constexpr int NET_COLUMNS = 4;
    static constexpr int NET_ROWS = 12;
    static constexpr ImU32 POST_COLOR = IM_COL32(252, 252, 252, 255);
    static constexpr ImU32 NET_FILL_COLOR = IM_COL32(12, 18, 22, 120);
    static constexpr ImU32 NET_LINE_COLOR = IM_COL32(230, 236, 240, 88);
    static constexpr ImU32 SHADOW_COLOR = IM_COL32(0, 0, 0, 60);
  };

  /** Stands, boards and run-off around the pitch (shares of the scene's
   * apron, MatchSceneTuning::Stadium::APRON_WIDTH). */
  struct Stadium final
  {
    static constexpr float RUN_OFF_SHARE = 0.34f;
    static constexpr float BOARD_SHARE = 0.12f;
    static constexpr float CROWD_STEP_PIXELS = 6.0f;
    static constexpr float CROWD_SIZE_SHARE = 0.62f;
    static constexpr float EMPTY_SEAT_SHARE = 0.1f;
    /** Supporters in club colours: behind each goal, then elsewhere. */
    static constexpr float END_FAN_SHARE = 0.55f;
    static constexpr float SIDE_FAN_SHARE = 0.2f;
    static constexpr float BOARD_LENGTH_METRES = 8.0f;
    static constexpr ImU32 STAND_FRONT_COLOR = IM_COL32(44, 52, 68, 255);
    static constexpr ImU32 STAND_BACK_COLOR = IM_COL32(18, 22, 32, 255);
    static constexpr ImU32 BOARD_COLOR = IM_COL32(22, 26, 34, 255);
    static constexpr std::uint32_t SEED = 20260929U;
  };

  /** Top-down kit tokens. */
  struct Token final
  {
    static constexpr float RADIUS_METRES = 1.1f;
    /** Tokens never get smaller than this share of the font size. */
    static constexpr float MIN_RADIUS_FONT_SHARE = 0.46f;
    static constexpr float SHIRT_SHARE = 0.74f;
    static constexpr float OUTLINE_SHARE = 0.12f;
    static constexpr float WEDGE_REACH = 1.62f;
    static constexpr float WEDGE_HALF_ANGLE = 0.42f;
    static constexpr float NUMBER_SHARE = 1.1f;
    /** Numbers smaller than this (pixels at scale 1) are not printed. */
    static constexpr float MIN_NUMBER_PIXELS = 7.5f;
    static constexpr float CARRIER_RING_SHARE = 1.5f;
    static constexpr float HOVER_RING_SHARE = 1.34f;
    static constexpr float PULSE_SPEED = 5.0f;
    static constexpr float SHADOW_OFFSET_SHARE = 0.22f;
    /** Players off the pitch (sent off, injured) wait faded by the bench. */
    static constexpr float OFF_PITCH_ALPHA = 0.45f;
    static constexpr ImU32 OUTLINE_COLOR = IM_COL32(10, 14, 20, 225);
    static constexpr ImU32 LIGHT_OUTLINE_COLOR = IM_COL32(245, 247, 250, 235);
    static constexpr ImU32 WEDGE_COLOR = IM_COL32(248, 250, 252, 235);
    static constexpr ImU32 SHADOW_COLOR = IM_COL32(0, 0, 0, 70);
    static constexpr ImU32 CARRIER_COLOR = IM_COL32(255, 214, 64, 255);
    static constexpr ImU32 HOVER_COLOR = IM_COL32(255, 255, 255, 230);
    static constexpr ImU32 CARD_COLOR = IM_COL32(250, 214, 40, 255);
    static constexpr ImU32 LABEL_BACK_COLOR = IM_COL32(6, 10, 20, 170);
    static constexpr ImU32 LABEL_TEXT_COLOR = IM_COL32(245, 247, 250, 255);
  };

  struct Ball final
  {
    /** Drawn larger than life (0.22 m) so it stays readable. */
    static constexpr float RADIUS_METRES = 0.36f;
    static constexpr float MIN_RADIUS_PIXELS = 2.5f;
    /** A lifted ball is drawn this share of its height up the screen. */
    static constexpr float LIFT_SHARE = 0.5f;
    /** Apparent growth per metre of height (closer to the eye). */
    static constexpr float GROWTH_PER_METRE = 0.035f;
    static constexpr float SHADOW_GROWTH = 0.16f;
    static constexpr float SHADOW_FADE = 0.3f;
    static constexpr std::uint8_t SHADOW_ALPHA = 120;
    /** Sun from the top left: the shadow slides this far per metre. */
    static constexpr float SHADOW_SLIDE = 0.22f;
    static constexpr float TRAIL_SECONDS = 0.22f;
    static constexpr float TRAIL_MIN_SPEED = 7.0f;
    static constexpr int TRAIL_SAMPLES = 10;
    /** A drawn ball moving further in one frame has jumped (a restart or
     * a playback skip): trails and paths are dropped. */
    static constexpr float JUMP_METRES = 20.0f;
    static constexpr ImU32 COLOR = IM_COL32(250, 250, 252, 255);
    static constexpr ImU32 OUTLINE_COLOR = IM_COL32(20, 24, 30, 200);
    static constexpr ImU32 PATCH_COLOR = IM_COL32(36, 40, 50, 255);
    static constexpr ImU32 TRAIL_COLOR = IM_COL32(255, 255, 255, 120);
  };

  /** Pass and shot paths shown briefly after the ball was struck. */
  struct Flight final
  {
    static constexpr int SAMPLES = 64;
    static constexpr float SAMPLE_SECONDS = 1.0f / 30.0f;
    static constexpr float FADE_SECONDS = 1.3f;
    /** Below this speed (m/s) a loose ball has stopped travelling. */
    static constexpr float STOP_SPEED = 2.5f;
    static constexpr float WIDTH_METRES = 0.16f;
    static constexpr ImU32 PASS_COLOR = IM_COL32(255, 255, 255, 150);
    static constexpr ImU32 SHOT_COLOR = IM_COL32(255, 132, 84, 210);
  };

  /** Offside flash along the defending line at the moment of the pass. */
  struct Offside final
  {
    static constexpr float SECONDS = 2.6f;
    static constexpr float DASH_METRES = 1.6f;
    static constexpr float PULSE_SPEED = 7.0f;
    static constexpr ImU32 COLOR = IM_COL32(255, 226, 64, 230);
  };

  /** Pitch-control overlay: who would reach each spot first. */
  struct Pressure final
  {
    static constexpr int COLUMNS = 21;
    static constexpr int ROWS = 14;
    /** Influence falls to half at this distance (metres). */
    static constexpr float REACH_METRES = 7.0f;
    /** Players are projected this far ahead along their run (s). */
    static constexpr float LOOKAHEAD_SECONDS = 0.5f;
    static constexpr float RESPONSE_RATE = 4.0f;
    static constexpr float MAX_ALPHA = 118.0f;
  };
};
