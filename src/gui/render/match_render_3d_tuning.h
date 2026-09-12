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
 * Named tuning values for the 3D broadcast match view. Distances are metres,
 * angles radians, rates per real second.
 */
struct MatchRender3DTuning final
{
  struct Camera final
  {
    static constexpr float MAX_FRAME_SECONDS = 0.1f;
    static constexpr float TARGET_RATE = 3.2f;
    static constexpr float ANGLE_RATE = 2.6f;
    static constexpr float DISTANCE_RATE = 2.6f;
    static constexpr float FOV_RATE = 3.0f;
    static constexpr float ZOOM_STEP = 0.9f;
    static constexpr float MIN_ZOOM = 0.5f;
    static constexpr float MAX_ZOOM = 1.8f;
    static constexpr float MIN_EYE_HEIGHT = 2.0f;
    static constexpr float LOOKAHEAD_SECONDS = 0.35f;
    static constexpr float MAX_LOOKAHEAD = 9.0f;
    static constexpr float NEAR_PLANE = 0.5f;
    static constexpr float FAR_PLANE = 900.0f;
    /** Eye stays inside the bowl (in front of every stand). */
    static constexpr float EYE_MIN_X = -7.0f;
    static constexpr float EYE_MAX_X = 112.0f;
    static constexpr float EYE_MIN_Y = -5.0f;
    static constexpr float EYE_MAX_Y = 73.0f;
    static constexpr float STAND_CLEAR_HEIGHT = 40.0f;
    /** Steepest pitch used to keep a low camera out of the stands. */
    static constexpr float MAX_CLAMP_PITCH = 1.25f;
    /** A new attacking side must hold the ball this long to turn the end
     * camera. */
    static constexpr float ATTACK_SWITCH_SECONDS = 2.0f;
  };

  /** Official pitch markings (Laws of the Game), metres. */
  struct Markings final
  {
    static constexpr float LINE_WIDTH = 0.15f;
    static constexpr float PENALTY_AREA_DEPTH = 16.5f;
    static constexpr float PENALTY_AREA_WIDTH = 40.32f;
    static constexpr float GOAL_AREA_DEPTH = 5.5f;
    static constexpr float GOAL_AREA_WIDTH = 18.32f;
    static constexpr float PENALTY_SPOT_DISTANCE = 11.0f;
    static constexpr float CIRCLE_RADIUS = 9.15f;
    static constexpr float CORNER_ARC_RADIUS = 1.0f;
    static constexpr float SPOT_RADIUS = 0.22f;
    static constexpr int CIRCLE_SEGMENTS = 56;
    static constexpr int PENALTY_ARC_SEGMENTS = 18;
    static constexpr int CORNER_ARC_SEGMENTS = 6;
    static constexpr int SPOT_SEGMENTS = 8;
    static constexpr ImU32 COLOR = IM_COL32(242, 244, 240, 228);
  };

  struct Grass final
  {
    static constexpr int STRIPES = 18;
    static constexpr int WIDTH_BANDS = 6;
    static constexpr float SURROUND_X = 9.0f;
    static constexpr float SURROUND_Y = 7.0f;
    static constexpr float OUTSIDE_EXTENT = 140.0f;
    static constexpr float LIGHT_STRIPE_BOOST = 1.10f;
    static constexpr float CROSS_BAND_BOOST = 1.035f;
    static constexpr float VIGNETTE_LENGTH = 0.16f;
    static constexpr float VIGNETTE_WIDTH = 0.20f;
    static constexpr ImU32 PITCH_COLOR = IM_COL32(52, 128, 50, 255);
    static constexpr ImU32 SURROUND_COLOR = IM_COL32(40, 104, 44, 255);
    static constexpr ImU32 OUTSIDE_COLOR = IM_COL32(20, 24, 26, 255);
  };

  struct Goal final
  {
    static constexpr float WIDTH = 7.32f;
    static constexpr float HEIGHT = 2.44f;
    static constexpr float POST_SIZE = 0.12f;
    static constexpr float ROOF_DEPTH = 1.0f;
    static constexpr float ROOF_DROP = 0.25f;
    static constexpr float BASE_DEPTH = 2.0f;
    static constexpr int NET_ROOF_ROWS = 3;
    static constexpr int NET_BACK_ROWS = 7;
    static constexpr int NET_COLUMNS = 16;
    static constexpr int NET_SIDE_COLUMNS = 5;
    static constexpr ImU32 POST_COLOR = IM_COL32(248, 248, 250, 255);
    static constexpr ImU32 NET_FILL_COLOR = IM_COL32(235, 240, 245, 34);
    static constexpr ImU32 NET_LINE_COLOR = IM_COL32(236, 240, 246, 92);
    static constexpr ImU32 SUPPORT_COLOR = IM_COL32(70, 74, 82, 255);
  };

  struct CornerFlag final
  {
    static constexpr float POLE_HEIGHT = 1.6f;
    static constexpr float POLE_WIDTH = 0.05f;
    static constexpr float FLAG_LENGTH = 0.5f;
    static constexpr float FLAG_HEIGHT = 0.35f;
    static constexpr float WAVE_SPEED = 2.4f;
    static constexpr float WAVE_AMPLITUDE = 0.12f;
    static constexpr ImU32 POLE_COLOR = IM_COL32(240, 240, 240, 255);
    static constexpr ImU32 FLAG_COLOR = IM_COL32(250, 214, 40, 255);
  };

  struct Boards final
  {
    static constexpr float SIDE_OFFSET = 4.0f;
    static constexpr float END_OFFSET = 5.0f;
    static constexpr float HEIGHT = 0.9f;
    static constexpr float LENGTH = 8.0f;
    static constexpr float GAP = 0.15f;
    static constexpr float TEXT_FONT_SIZE = 32.0f;
    static constexpr float TEXT_HEIGHT_RATIO = 0.62f;
    static constexpr float TEXT_WIDTH_RATIO = 0.86f;
    static constexpr float MIN_TEXT_PIXELS = 4.0f;
    static constexpr float ROTATE_SECONDS = 14.0f;
    static constexpr ImU32 BACK_COLOR = IM_COL32(34, 36, 42, 255);
  };

  struct Stands final
  {
    static constexpr float SIDE_FRONT = 7.0f;
    static constexpr float END_FRONT = 9.0f;
    static constexpr int SIDE_SECTIONS = 9;
    static constexpr int END_SECTIONS = 6;
    static constexpr float FRONT_WALL_HEIGHT = 1.3f;
    static constexpr float LOWER_DEPTH = 20.0f;
    static constexpr float LOWER_TOP = 11.0f;
    static constexpr float CONCOURSE_TOP = 14.5f;
    static constexpr float UPPER_DEPTH = 38.0f;
    static constexpr float UPPER_TOP = 30.0f;
    static constexpr float BACK_WALL_TOP = 34.0f;
    static constexpr float ROOF_FRONT_DEPTH = 13.0f;
    static constexpr float ROOF_BACK_DEPTH = 38.0f;
    static constexpr float ROOF_FRONT_HEIGHT = 31.5f;
    static constexpr float ROOF_BACK_HEIGHT = 34.0f;
    static constexpr float FASCIA_HEIGHT = 1.8f;
    static constexpr float WINDOW_INSET = 1.1f;
    static constexpr ImU32 CONCRETE_COLOR = IM_COL32(78, 82, 92, 255);
    static constexpr ImU32 WALL_COLOR = IM_COL32(40, 44, 54, 255);
    static constexpr ImU32 SEAT_COLOR = IM_COL32(40, 52, 88, 255);
    static constexpr ImU32 ROOF_UNDER_COLOR = IM_COL32(30, 33, 40, 255);
    static constexpr ImU32 ROOF_TOP_COLOR = IM_COL32(84, 90, 102, 255);
    static constexpr ImU32 FASCIA_COLOR = IM_COL32(24, 30, 52, 255);
    static constexpr ImU32 WINDOW_COLOR = IM_COL32(176, 150, 104, 255);
    static constexpr float BACK_ROW_LIGHT = 0.5f;
  };

  struct Crowd final
  {
    static constexpr float SEAT_SPACING = 0.9f;
    static constexpr float ROW_DEPTH = 1.1f;
    static constexpr float DOT_WIDTH = 0.62f;
    static constexpr float DOT_HEIGHT = 0.95f;
    static constexpr float JITTER = 0.18f;
    static constexpr float EMPTY_SEAT_RATIO = 0.12f;
    static constexpr float HOME_SHARE = 0.34f;
    static constexpr float AWAY_END_SHARE = 0.55f;
    static constexpr float MIN_DOT_PIXELS = 0.8f;
    /** Dots wider than this get a separate head block. */
    static constexpr float HEAD_DETAIL_PIXELS = 5.0f;
    static constexpr float BODY_SHARE = 0.68f;
    static constexpr float NECK_GAP = 0.04f;
    static constexpr float HEAD_WIDTH_SHARE = 0.56f;
    static constexpr std::uint32_t SEED = 20260927U;
  };

  struct Floodlight final
  {
    static constexpr float CORNER_OFFSET_X = 32.0f;
    static constexpr float CORNER_OFFSET_Y = 30.0f;
    static constexpr float MAST_HEIGHT = 58.0f;
    static constexpr float MAST_SIZE = 1.4f;
    static constexpr float HEAD_WIDTH = 11.0f;
    static constexpr float HEAD_HEIGHT = 6.0f;
    static constexpr float HEAD_TILT = 0.55f;
    static constexpr float GLOW_RADIUS = 9.0f;
    static constexpr ImU32 MAST_COLOR = IM_COL32(92, 96, 106, 255);
    static constexpr ImU32 LAMP_COLOR = IM_COL32(255, 252, 232, 255);
    static constexpr ImU32 GLOW_COLOR = IM_COL32(255, 246, 214, 255);
  };

  /** Low-poly footballer proportions (metres at scale 1) and animation. */
  struct Player final
  {
    static constexpr float SCALE = 1.12f;
    static constexpr float REFERENCE_HEIGHT_CM = 180.0f;
    static constexpr float MIN_HEIGHT_SCALE = 0.93f;
    static constexpr float MAX_HEIGHT_SCALE = 1.07f;
    static constexpr float HIP_HEIGHT = 0.9f;
    static constexpr float HIP_SPREAD = 0.1f;
    static constexpr float THIGH_LENGTH = 0.45f;
    static constexpr float THIGH_HALF_WIDTH = 0.078f;
    static constexpr float SHIN_LENGTH = 0.43f;
    static constexpr float SHIN_HALF_WIDTH = 0.062f;
    static constexpr float BOOT_HALF_LENGTH = 0.12f;
    static constexpr float BOOT_HALF_WIDTH = 0.055f;
    static constexpr float BOOT_HALF_HEIGHT = 0.045f;
    static constexpr float BOOT_FORWARD = 0.05f;
    static constexpr float SHORTS_HEIGHT = 0.84f;
    static constexpr float SHORTS_HALF_DEPTH = 0.125f;
    static constexpr float SHORTS_HALF_WIDTH = 0.195f;
    static constexpr float SHORTS_HALF_HEIGHT = 0.135f;
    static constexpr float TORSO_BASE = 0.96f;
    static constexpr float TORSO_LENGTH = 0.53f;
    static constexpr float WAIST_HALF_DEPTH = 0.11f;
    static constexpr float WAIST_HALF_WIDTH = 0.175f;
    static constexpr float CHEST_HALF_DEPTH = 0.125f;
    static constexpr float CHEST_HALF_WIDTH = 0.235f;
    static constexpr float COLLAR_HALF_SIZE = 0.09f;
    static constexpr float COLLAR_HALF_HEIGHT = 0.025f;
    static constexpr float SHOULDER_SPREAD = 0.26f;
    static constexpr float SHOULDER_DROP = 0.06f;
    static constexpr float UPPER_ARM_LENGTH = 0.29f;
    static constexpr float UPPER_ARM_HALF_WIDTH = 0.058f;
    static constexpr float FOREARM_LENGTH = 0.26f;
    static constexpr float FOREARM_HALF_WIDTH = 0.046f;
    static constexpr float NECK_LENGTH = 0.17f;
    static constexpr float HEAD_RADIUS = 0.118f;
    static constexpr float THIGH_SWING = 0.62f;
    static constexpr float KNEE_FLEX = 1.05f;
    static constexpr float ARM_SWING = 0.7f;
    static constexpr float ELBOW_BEND = 0.35f;
    static constexpr float ELBOW_RUN_BEND = 0.9f;
    static constexpr float RUN_LEAN = 0.2f;
    static constexpr float RUN_BOB = 0.05f;
    /** World metres per full stride cycle; drives the leg phase. */
    static constexpr float STRIDE_CYCLE_METRES = 7.0f;
    static constexpr float MAX_CADENCE_HZ = 4.0f;
    static constexpr float FULL_STRIDE_SPEED = 9.0f;
    static constexpr float STRIDE_RATE = 7.0f;
    static constexpr float TELEPORT_METRES = 12.0f;
    static constexpr float CULL_MARGIN_PIXELS = 80.0f;
    static constexpr ImU32 BOOT_COLOR = IM_COL32(26, 26, 30, 255);
  };

  struct Ball final
  {
    /**
     * Engine ball height is in normalised pitch units. Mapping it with the
     * pitch width keeps the highest simulated shot (z ~ 0.034) just under
     * the 2.44 m crossbar and lofted passes around 5 m.
     */
    static constexpr float HEIGHT_SCALE = 68.0f;
    static constexpr float VISUAL_RADIUS = 0.24f;
    static constexpr float MIN_PIXELS = 2.6f;
    static constexpr float SHADOW_RADIUS = 0.26f;
    static constexpr float SHADOW_GROWTH = 0.12f;
    static constexpr float SHADOW_FADE = 0.25f;
    static constexpr ImU32 COLOR = IM_COL32(244, 245, 248, 255);
    static constexpr ImU32 SHADE_COLOR = IM_COL32(150, 156, 170, 255);
    static constexpr ImU32 PATCH_COLOR = IM_COL32(40, 44, 56, 255);
  };

  struct Shadow final
  {
    static constexpr float CONTACT_RADIUS = 0.42f;
    static constexpr float BLADE_LENGTH = 1.9f;
    static constexpr float BLADE_HALF_WIDTH = 0.24f;
    static constexpr int SEGMENTS = 10;
    static constexpr ImU32 CONTACT_COLOR = IM_COL32(0, 0, 0, 78);
    static constexpr ImU32 BLADE_COLOR = IM_COL32(0, 0, 0, 34);
    static constexpr float RING_RADIUS = 0.78f;
    static constexpr int RING_SEGMENTS = 20;
    static constexpr float RING_THICKNESS = 2.2f;
    static constexpr float RING_PULSE = 0.08f;
    static constexpr float RING_PULSE_SPEED = 5.0f;
    static constexpr ImU32 RING_COLOR = IM_COL32(255, 214, 64, 235);
  };

  struct Light final
  {
    static constexpr float AMBIENT = 0.52f;
    static constexpr float DIFFUSE = 0.55f;
    static constexpr float SKY = 0.12f;
    static constexpr float DIRECTION_X = 0.3f;
    static constexpr float DIRECTION_Y = -0.62f;
    static constexpr float DIRECTION_Z = 0.72f;
  };

  struct Sky final
  {
    static constexpr float HORIZON_DISTANCE = 1000.0f;
    static constexpr ImU32 TOP_COLOR = IM_COL32(6, 10, 26, 255);
    static constexpr ImU32 MIDDLE_COLOR = IM_COL32(22, 34, 72, 255);
    static constexpr ImU32 HORIZON_COLOR = IM_COL32(86, 78, 112, 255);
    static constexpr ImU32 GROUND_COLOR = IM_COL32(14, 16, 20, 255);
    static constexpr float VIGNETTE_EDGE = 0.16f;
    static constexpr ImU32 VIGNETTE_COLOR = IM_COL32(0, 0, 0, 96);
  };

  struct Hud final
  {
    static constexpr float MARGIN = 12.0f;
    static constexpr float PADDING = 7.0f;
    static constexpr float CHIP_WIDTH = 6.0f;
    static constexpr float ROUNDING = 4.0f;
    static constexpr float LABEL_OFFSET = 6.0f;
    static constexpr float LABEL_PADDING = 3.0f;
    static constexpr float HOVER_PADDING = 6.0f;
    static constexpr ImU32 PANEL_COLOR = IM_COL32(8, 12, 24, 210);
    static constexpr ImU32 SCORE_PANEL_COLOR = IM_COL32(250, 250, 252, 240);
    static constexpr ImU32 SCORE_TEXT_COLOR = IM_COL32(12, 16, 28, 255);
    static constexpr ImU32 TEXT_COLOR = IM_COL32(245, 247, 250, 255);
    static constexpr ImU32 LABEL_BACK_COLOR = IM_COL32(6, 10, 20, 150);
    static constexpr ImU32 DEBUG_TARGET_COLOR = IM_COL32(255, 215, 64, 170);
  };

  struct Broadcast final
  {
    static constexpr float EYE_Y = -31.0f;
    static constexpr float EYE_HEIGHT = 21.0f;
    static constexpr float RAIL_FOLLOW = 0.55f;
    static constexpr float TARGET_WIDTH_FOLLOW = 0.5f;
    static constexpr float TARGET_MIN_X = 12.0f;
    static constexpr float TARGET_MAX_X = 93.0f;
    static constexpr float TARGET_MIN_Y = 14.0f;
    static constexpr float TARGET_MAX_Y = 54.0f;
    static constexpr float FOV = 0.42f;
  };

  struct Tactical final
  {
    static constexpr float LENGTH_FOLLOW = 0.15f;
    static constexpr float TARGET_Y_OFFSET = 1.5f;
    static constexpr float PITCH = 1.08f;
    static constexpr float DISTANCE = 100.0f;
    static constexpr float FOV = 0.66f;
  };

  struct End final
  {
    static constexpr float LEAD = 12.0f;
    static constexpr float WIDTH_FOLLOW = 0.6f;
    static constexpr float TARGET_MIN_X = 8.0f;
    static constexpr float TARGET_MAX_X = 97.0f;
    static constexpr float PITCH = 0.36f;
    static constexpr float DISTANCE = 40.0f;
    static constexpr float FOV = 0.78f;
  };

  struct Follow final
  {
    static constexpr float TARGET_HEIGHT = 1.0f;
    static constexpr float YAW_RATE = 1.6f;
    static constexpr float PITCH = 0.3f;
    static constexpr float DISTANCE = 15.0f;
    static constexpr float FOV = 0.84f;
  };
};
