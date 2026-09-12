// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstddef>
#include <cstdint>

/**
 * Central tuning values for the renderer-independent match simulation.
 *
 * Keeping these values named and grouped makes balancing auditable and leaves
 * a clean migration path to data-driven tuning profiles later. Normalized
 * pitch coordinates use x for length (home attacks from 0 to 1) and y for
 * width.
 */
struct MatchTuning final
{
  MatchTuning(const MatchTuning&) = default;
  MatchTuning(MatchTuning&&) = default;
  MatchTuning& operator=(const MatchTuning&) = default;
  MatchTuning& operator=(MatchTuning&&) = default;

  /**
   * On-ball action tempo relative to the reference kinematics the values
   * below were written for. Speeds are multiplied by it, reaction delays and
   * cooldowns divided by it, and gravity multiplied by its square, so the
   * whole action speeds up without changing its shape.
   */
  struct Tempo final
  {
    static constexpr float ACTION = 2.0f;
  };

  struct Pitch final
  {
    static constexpr float LENGTH_METRES = 105.0f;
    static constexpr float WIDTH_METRES = 68.0f;
    static constexpr float CENTRE = 0.5f;
    static constexpr float GOAL_TOP = 0.446f;
    static constexpr float GOAL_BOTTOM = 0.554f;
    static constexpr float LINEUP_GOALKEEPER_X = 0.04f;
    static constexpr float GOALKEEPER_MIN_Y = 0.38f;
    static constexpr float GOALKEEPER_MAX_Y = 0.62f;
    static constexpr float GOALKEEPER_SWEEP_DEPTH = 0.18f;
    static constexpr float LEFT_GOAL_KICK_X = 0.065f;
    static constexpr float RIGHT_GOAL_KICK_X = 0.935f;
    static constexpr float LEFT_PENALTY_SPOT_X = 0.115f;
    static constexpr float RIGHT_PENALTY_SPOT_X = 0.885f;
    static constexpr float LEFT_PENALTY_AREA_EDGE = 0.17f;
    static constexpr float RIGHT_PENALTY_AREA_EDGE = 0.83f;
    static constexpr float RESTART_INSET = 0.01f;
    static constexpr float RESTART_LONGITUDINAL_MARGIN = 0.04f;
    static constexpr float HOME_KICKOFF_X = 0.49f;
    static constexpr float AWAY_KICKOFF_X = 0.51f;
    static constexpr float KICKOFF_FORMATION_INSET = 0.02f;
    static constexpr float KICKOFF_FORMATION_SCALE = 0.56f;
    static constexpr float PLAYER_MIN_X = 0.005f;
    static constexpr float PLAYER_MAX_X = 0.995f;
    static constexpr float PLAYER_MIN_Y = 0.01f;
    static constexpr float PLAYER_MAX_Y = 0.99f;
  };

  struct Timing final
  {
    static constexpr float FIXED_STEP_SECONDS = 1.0f / 60.0f;
    static constexpr float PHYSICS_REFERENCE_STEP_SECONDS =
        1.0f / 30.0f / Tempo::ACTION;
    static constexpr float MAX_FRAME_DELTA_SECONDS = 0.5f;
    // Prevent a delayed render frame from triggering an unbounded simulation
    // catch-up. Any remaining whole steps are deliberately dropped because a
    // responsive live view is more useful than replaying stale wall time.
    static constexpr int MAX_FIXED_STEPS_PER_UPDATE = 12;
    static constexpr float MATCH_MINUTES_PER_REAL_SECOND = 1.0f;
    static constexpr float HALF_TIME_MINUTE = 45.0f;
    static constexpr float FULL_TIME_MINUTE = 90.0f;
    static constexpr float HALF_TIME_PAUSE_SECONDS = 0.8f;
    static constexpr float KICKOFF_DELAY_SECONDS = 0.55f;
    static constexpr float THROW_IN_DELAY_SECONDS = 0.32f;
    static constexpr float GOAL_KICK_DELAY_SECONDS = 0.5f;
    static constexpr float CORNER_DELAY_SECONDS = 0.65f;
    static constexpr float FREE_KICK_DELAY_SECONDS = 0.45f;
    static constexpr float PENALTY_DELAY_SECONDS = 0.8f;
    static constexpr float GOAL_CELEBRATION_SECONDS = 2.5f;
    static constexpr float POSSESSION_TRANSITION_SECONDS = 2.4f / Tempo::ACTION;
    static constexpr std::size_t MAX_EVENTS = 1024;
  };

  struct Player final
  {
    static constexpr float DEFAULT_ATTRIBUTE = 0.5f;
    static constexpr float RATING_SCALE = 100.0f;
    // Real top speeds spread narrowly (about 7.5-10.4 m/s), so pace shifts
    // speed by roughly a third across the attribute range, not double it.
    static constexpr float BASE_MAX_SPEED = 0.23f * Tempo::ACTION;
    static constexpr float PACE_SPEED_BONUS = 0.10f * Tempo::ACTION;
    static constexpr float BASE_ACCELERATION = 0.75f * Tempo::ACTION;
    static constexpr float PACE_ACCELERATION_BONUS = 0.40f * Tempo::ACTION;
    static constexpr float TURN_RATE_RADIANS = 6.2831853f * Tempo::ACTION;
    static constexpr float MINIMUM_STAMINA = 0.35f;
    static constexpr float STAMINA_SPEED_BASE = 0.62f;
    static constexpr float STAMINA_SPEED_BONUS = 0.38f;
    static constexpr float MOVEMENT_FACING_THRESHOLD = 0.005f;
    static constexpr float TACTICAL_TARGET_RESPONSE_PER_SECOND =
        6.0f * Tempo::ACTION;
    static constexpr float URGENT_TARGET_RESPONSE_PER_SECOND =
        11.0f * Tempo::ACTION;
    static constexpr float ARRIVAL_SLOWING_DISTANCE = 0.045f;
    static constexpr float HOLD_SHAPE_SPEED_SCALE = 0.46f;
    static constexpr float CARRY_BALL_SPEED_SCALE = 0.78f;
    static constexpr float SUPPORT_SPEED_SCALE = 0.64f;
    static constexpr float ATTACKING_RUN_SPEED_SCALE = 0.96f;
    static constexpr float PRESS_SPEED_SCALE = 0.94f;
    static constexpr float COVER_SPEED_SCALE = 0.72f;
    static constexpr float MARKING_SPEED_SCALE = 0.60f;
    static constexpr float RECOVERY_SPEED_SCALE = 0.82f;
    static constexpr float GOALKEEPER_MOVEMENT_SPEED_SCALE = 0.54f;
    static constexpr float MINIMUM_BODY_SEPARATION_METRES = 1.8f;
    static constexpr float BODY_SEPARATION_SHARE = 0.5f;
    static constexpr float BODY_COLLISION_VELOCITY_RETAINED = 0.82f;
    static constexpr float SUBSTITUTION_SETTLE_SECONDS = 0.4f / Tempo::ACTION;
    static constexpr float LOOSE_BALL_LOOKAHEAD_SECONDS = 0.30f / Tempo::ACTION;
    static constexpr float COVER_LOOSE_BALL_OFFSET = 0.055f;
    static constexpr float PASS_RECEIVER_LOOKAHEAD_SECONDS =
        0.22f / Tempo::ACTION;
    static constexpr float MINIMUM_PURSUIT_SPEED = 0.05f * Tempo::ACTION;
    static constexpr float CURRENT_VELOCITY_PURSUIT_WEIGHT = 0.55f;
  };

  struct Shape final
  {
    static constexpr float MIN_WIDTH_SCALE = 0.72f;
    static constexpr float WIDTH_SLIDER_SCALE = 0.54f;
    static constexpr float BASE_LONGITUDINAL_SHIFT = 0.12f;
    static constexpr float COMPACTNESS_LONGITUDINAL_SHIFT = 0.12f;
    static constexpr float BASE_LATERAL_SHIFT = 0.10f;
    static constexpr float COMPACTNESS_LATERAL_SHIFT = 0.10f;
    static constexpr float CARRIER_BASE_ADVANCE = 0.10f;
    static constexpr float CARRIER_DRIBBLING_ADVANCE = 0.08f;
    static constexpr float CARRIER_RISK_ADVANCE = 0.05f;
    static constexpr float CARRIER_EVASION_RANGE = 0.10f;
    static constexpr float CARRIER_BASE_EVASION = 0.035f;
    static constexpr float CARRIER_DRIBBLING_EVASION = 0.045f;
    static constexpr float CARRIER_CENTRALITY = 0.16f;
    // Wide players carrying down the flank keep their lane to cross.
    static constexpr float WIDE_LANE_DEVIATION = 0.24f;
    static constexpr float SUPPORT_BASE_ADVANCE = 0.035f;
    static constexpr float SUPPORT_OFFENSIVE_ADVANCE = 0.10f;
    static constexpr std::size_t MAX_ACTIVE_SUPPORTERS = 3;
    static constexpr std::size_t NEAR_SUPPORT_SLOT = 0;
    static constexpr std::size_t SQUARE_SUPPORT_SLOT = 1;
    static constexpr std::size_t TRAILING_SUPPORT_SLOT = 2;
    static constexpr float SUPPORT_SELECTION_CONTINUITY_BONUS = 0.08f;
    static constexpr float NEAR_SUPPORT_DEPTH = 0.065f;
    static constexpr float NEAR_SUPPORT_WIDTH = 0.105f;
    static constexpr float SQUARE_SUPPORT_DEPTH = 0.025f;
    static constexpr float SQUARE_SUPPORT_WIDTH = 0.165f;
    static constexpr float TRAILING_SUPPORT_DEPTH = 0.125f;
    static constexpr float TRAILING_SUPPORT_WIDTH = 0.045f;
    static constexpr float TRANSITION_SUPPORT_FORWARD_BONUS = 0.055f;
    static constexpr float RUN_BASE_ADVANCE = 0.045f;
    static constexpr float RUN_RISK_ADVANCE = 0.09f;
    static constexpr float ONSIDE_RECOVERY = 0.055f;
    static constexpr float MAX_SUPPORT_DISTANCE = 0.34f;
    static constexpr float SUPPORT_LONGITUDINAL_PULL = 0.20f;
    static constexpr float SUPPORT_LATERAL_PULL = 0.14f;
    static constexpr float POSSESSION_PROGRESS_START = 0.25f;
    static constexpr float POSSESSION_BLOCK_PROGRESS = 0.24f;
    static constexpr std::size_t MIN_COMMITTED_RUNNERS = 1;
    static constexpr std::size_t MAX_COMMITTED_RUNNERS = 3;
    static constexpr float SECOND_RUNNER_PROGRESS_THRESHOLD = 0.38f;
    static constexpr float SECOND_RUNNER_ATTACK_THRESHOLD = 0.80f;
    static constexpr float STRIKER_RUN_PRIORITY = 0.42f;
    static constexpr float WINGER_RUN_PRIORITY = 0.34f;
    static constexpr float ATTACKING_MIDFIELDER_RUN_PRIORITY = 0.26f;
    static constexpr float MIDFIELDER_RUN_PRIORITY = 0.10f;
    static constexpr float RUN_PACE_PRIORITY = 0.22f;
    static constexpr float RUN_DEPTH_PRIORITY = 0.12f;
    static constexpr float RUN_SEPARATION_PRIORITY = 0.10f;
    static constexpr float RUN_CONTINUITY_PRIORITY = 0.24f;
    static constexpr float RUN_ONSIDE_BUFFER = 0.012f;
    static constexpr float RUN_DEPTH_TARGET_PULL = 0.82f;
    static constexpr float SECONDARY_RUN_DEPTH_STAGGER = 0.055f;
    static constexpr float RUN_CHANNEL_BLEND = 0.55f;
    static constexpr float LEFT_WIDE_ATTACK_CHANNEL = 0.18f;
    static constexpr float RIGHT_WIDE_ATTACK_CHANNEL = 0.82f;
    static constexpr float LEFT_INSIDE_FORWARD_CHANNEL = 0.34f;
    static constexpr float RIGHT_INSIDE_FORWARD_CHANNEL = 0.66f;
    static constexpr float LEFT_STRIKER_ATTACK_CHANNEL = 0.42f;
    static constexpr float RIGHT_STRIKER_ATTACK_CHANNEL = 0.58f;
    static constexpr float CENTRAL_ATTACK_CHANNEL = 0.50f;
    static constexpr float MINIMUM_RUN_CHANNEL_SEPARATION = 0.16f;
    static constexpr float FORWARD_SHORT_OPTION_DEPTH = 0.09f;
    static constexpr float FORWARD_SHORT_OPTION_LATERAL_SEPARATION = 0.13f;
    static constexpr float FINAL_THIRD_MIDFIELD_ARRIVAL = 0.14f;
    static constexpr float FINAL_THIRD_FULLBACK_OVERLAP = 0.17f;
    static constexpr float FINAL_THIRD_SELECTION_CONTINUITY = 0.16f;
    static constexpr float PRESSING_STANDOFF_BASE = 0.010f;
    static constexpr float PRESSING_STANDOFF_CAUTIOUS_BONUS = 0.018f;
    static constexpr float PRESSER_CONTINUITY_SECONDS = 0.20f / Tempo::ACTION;
    static constexpr float COVER_PRESS_MINIMUM = 0.35f;
    static constexpr float COVER_LANE_INTERCEPTION_POINT = 0.52f;
    static constexpr float COVER_LANE_GOAL_SIDE_OFFSET = 0.018f;
    static constexpr float COVER_OUTLET_MAX_DISTANCE = 0.34f;
    static constexpr float COVER_FORWARD_OPTION_BONUS = 0.08f;
    static constexpr float COVER_FALLBACK_DEPTH = 0.035f;
    static constexpr float COVER_FALLBACK_LATERAL_OFFSET = 0.055f;
    static constexpr float CHANNEL_WEIGHT = 1.4f;
    static constexpr float DANGER_DEPTH_WEIGHT = 0.12f;
    static constexpr float BASE_MARK_WEIGHT = 0.12f;
    static constexpr float COMPACTNESS_MARK_WEIGHT = 0.20f;
    static constexpr float SECOND_LOOSE_BALL_PRESS_THRESHOLD = 0.65f;
    static constexpr float ATTACKING_TRANSITION_SUPPORT_ADVANCE = 0.065f;
    static constexpr float ATTACKING_TRANSITION_RUN_ADVANCE = 0.13f;
    static constexpr float ATTACKING_TRANSITION_BALL_PULL = 0.12f;
    static constexpr float ATTACKING_TRANSITION_CARRIER_ADVANCE = 0.055f;
    static constexpr float IN_FLIGHT_SUPPORT_ADVANCE = 0.025f;
    static constexpr float IN_FLIGHT_SUPPORT_BALL_PULL = 0.05f;
    static constexpr float DEFENSIVE_TRANSITION_RECOVERY = 0.055f;
    static constexpr float DEFENSIVE_TRANSITION_BALL_COMPACTNESS = 0.16f;
  };

  struct Decision final
  {
    static constexpr float PRESSURE_RADIUS = 0.10f;
    static constexpr float BASE_SHOT_THRESHOLD = 0.012f;
    static constexpr float SHOT_RISK_THRESHOLD_REDUCTION = 0.005f;
    static constexpr float SHOT_SKILL_THRESHOLD_REDUCTION = 0.005f;
    static constexpr float BASE_SHOT_INCLINATION = 0.13f;
    static constexpr float SHOT_CHANCE_SCALE = 14.0f;
    static constexpr float PRESSURED_SHOT_BONUS = 0.30f;
    static constexpr float MAX_SHOT_CHANCE = 0.88f;
    static constexpr float MIN_SHOT_XG = 0.005f;
    static constexpr float BASE_PASS_CHANCE = 0.42f;
    static constexpr float PASSING_CHANCE_BONUS = 0.32f;
    static constexpr float PRESSURED_PASS_BONUS = 0.24f;
    static constexpr float RISK_PASS_PENALTY = 0.07f;
    static constexpr float BLOCKED_LANE_PENALTY = 0.20f;
    static constexpr float MIN_PASS_CHANCE = 0.22f;
    static constexpr float MAX_PASS_CHANCE = 0.93f;
    static constexpr float NEUTRAL_COMPLETION_PROBABILITY = 0.5f;
    static constexpr float COMPLETION_PASS_CHANCE_WEIGHT = 0.15f;
    static constexpr float MIN_DRIBBLE_TIME = 0.20f / Tempo::ACTION;
    static constexpr float MAX_DRIBBLE_TIME = 0.45f / Tempo::ACTION;

    // Scored action selection: every candidate (best pass, shot, carry,
    // shield) is measured in a shared utility currency and the closest
    // choices are resolved by a vision-scaled random perturbation.
    static constexpr float SHOT_SCORE_SCALE = 48.0f;
    static constexpr float SHOT_BASE_INCLINATION = 1.90f;
    static constexpr float FINAL_THIRD_SHOT_BONUS = 0.85f;
    static constexpr float SHOT_ELIGIBILITY_FLOOR = 0.005f;
    static constexpr float SHOT_ELIGIBILITY_RANGE = 0.015f;
    static constexpr float SHOT_SKILL_BONUS = 0.28f;
    static constexpr float SHOT_PRESSURE_PENALTY = 0.02f;
    static constexpr float CARRY_OPENNESS_WEIGHT = 1.50f;
    static constexpr float CARRY_DRIBBLING_BONUS = 0.45f;
    static constexpr float CARRY_PRESSURE_PENALTY = 0.30f;
    static constexpr float CARRY_RISK_BIAS = 0.20f;
    static constexpr float SHIELD_PRESSURE_THRESHOLD = 0.60f;
    static constexpr float SHIELD_BONUS = 0.40f;
    static constexpr float SHIELD_DRIBBLING = 0.12f;
    static constexpr float VISION_NOISE_SCALE = 0.45f;
    static constexpr float VISION_DECISION_WEIGHT = 0.6f;
    static constexpr float LATE_GAME_MINUTE = 82.0f;
    static constexpr float LATE_LEAD_SPECULATIVE_PENALTY = 0.55f;
    static constexpr int COMFORTABLE_LEAD = 2;
    static constexpr float COMFORTABLE_LEAD_SHOT_PENALTY = 2.0f;
    static constexpr float CLEAR_CHANCE_XG = 0.30f;
    static constexpr float COMFORTABLE_LEAD_CARRY_PENALTY = 0.5f;
    static constexpr float WIDE_SHOT_WIDTH_DEVIATION = 0.32f;
    static constexpr float WIDE_SHOT_DISCOUNT = 0.12f;
    static constexpr float WIDE_RECYCLE_BONUS = 0.85f;
  };

  struct Passing final
  {
    static constexpr float MIN_DISTANCE = 0.025f;
    static constexpr float MAX_DISTANCE = 0.52f;
    static constexpr float IDEAL_DISTANCE = 0.18f;
    static constexpr float OPENNESS_RADIUS = 0.12f;
    static constexpr float OPENNESS_WEIGHT = 1.15f;
    static constexpr float LANE_RISK_WEIGHT = 1.45f;
    static constexpr float BASE_PROGRESS_WEIGHT = 1.4f;
    static constexpr float OFFENSIVE_PROGRESS_WEIGHT = 1.15f;
    static constexpr float DISTANCE_PENALTY = 1.15f;
    static constexpr float SAFE_OUTLET_WEIGHT = 0.8f;
    static constexpr float FORWARD_ROLE_BONUS = 0.10f;
    static constexpr float MIN_ACCEPTABLE_OPTION_SCORE = -0.15f;
    static constexpr float PRESSURE_RELEASE_THRESHOLD = 0.55f;
    static constexpr float PRESSURE_RELEASE_MAX_PROGRESSION = 0.03f;
    static constexpr float PROGRESSIVE_PASS_MINIMUM = 0.035f;
    static constexpr float THROUGH_BALL_MINIMUM_PROGRESSION = 0.09f;
    static constexpr float SWITCH_PLAY_MINIMUM_WIDTH = 0.34f;
    static constexpr float WIDE_ATTACK_MINIMUM_Y = 0.30f;
    static constexpr float WIDE_ATTACK_MAXIMUM_Y = 0.70f;
    static constexpr float CENTRAL_TARGET_MINIMUM_Y = 0.30f;
    static constexpr float CENTRAL_TARGET_MAXIMUM_Y = 0.70f;
    static constexpr float CROSS_MINIMUM_PASSER_DEPTH = 0.62f;
    static constexpr float CROSS_MINIMUM_RECEIVER_DEPTH = 0.70f;
    static constexpr float CUTBACK_MINIMUM_PASSER_DEPTH = 0.86f;
    static constexpr float CUTBACK_MAXIMUM_PROGRESSION = 0.035f;
    static constexpr float CUTBACK_MINIMUM_PROGRESSION = -0.24f;
    static constexpr float CROSS_UTILITY_BONUS = 1.6f;
    static constexpr float CUTBACK_UTILITY_BONUS = 0.62f;
    static constexpr float CROSS_COMPLETION_PENALTY = 0.10f;
    static constexpr float CUTBACK_COMPLETION_BONUS = 0.07f;
    static constexpr float CROSS_TARGET_BLEND = 0.35f;
    static constexpr float BASE_COMPLETION_PROBABILITY = 0.58f;
    static constexpr float PASSING_COMPLETION_BONUS = 0.27f;
    static constexpr float OPENNESS_COMPLETION_BONUS = 0.15f;
    static constexpr float LANE_COMPLETION_PENALTY = 0.50f;
    static constexpr float DISTANCE_COMPLETION_PENALTY = 0.20f;
    static constexpr float PRESSURE_COMPLETION_PENALTY = 0.14f;
    static constexpr float MIN_COMPLETION_PROBABILITY = 0.05f;
    static constexpr float MAX_COMPLETION_PROBABILITY = 0.98f;
    static constexpr float COMPLETION_UTILITY_WEIGHT = 0.35f;
    static constexpr float ACTIVE_RUNNER_UTILITY_BONUS = 0.22f;
    static constexpr float THROUGH_BALL_FORWARD_LEAD = 0.055f;
    static constexpr float THROUGH_BALL_TARGET_BLEND = 0.60f;
    static constexpr float LOFTED_DISTANCE = 0.27f;
    static constexpr float PASS_PRESSURE_RADIUS = 0.09f;
    static constexpr float ESTIMATED_BALL_SPEED = 0.75f * Tempo::ACTION;
    static constexpr float RECEIVER_LEAD_SCALE = 0.45f;
    static constexpr float TECHNICAL_ERROR = 0.018f;
    static constexpr float PRESSURE_ERROR = 0.014f;
    static constexpr float DISTANCE_ERROR = 0.012f;
    static constexpr float SPEED_DISTANCE_SCALE = 0.34f / Tempo::ACTION;
    static constexpr float MIN_BALL_SPEED = 0.38f * Tempo::ACTION;
    static constexpr float MAX_BALL_SPEED = 0.95f * Tempo::ACTION;
    static constexpr float BASE_BALL_SPEED = 0.82f;
    static constexpr float PASSING_SPEED_BONUS = 0.25f;
    static constexpr float GROUND_VERTICAL_SPEED = 0.04f * Tempo::ACTION;
    static constexpr float GROUND_PASS_RELEASE_HEIGHT = 0.001f;
    static constexpr float LOFTED_ARRIVAL_HEIGHT_METRES = 1.1f;
    static constexpr float DELIVERY_HEIGHT_SPREAD = 0.6f;
    static constexpr float OFFSIDE_PERCEPTION_ERROR = 0.06f;
    // Runners time their runs imperfectly: they sometimes drift beyond the
    // line, and a passer who misreads it releases an offside pass.
    static constexpr float RUN_TIMING_GAMBLE = 0.05f;
    static constexpr float OFFSIDE_TIMING_WINDOW = 0.07f;
    static constexpr float OFFSIDE_TIMING_CHANCE = 0.6f;
    static constexpr std::uint32_t RUN_TIMING_EPOCH_STEPS = 45;
    static constexpr float MAX_CURVE = 0.18f * Tempo::ACTION;
    static constexpr float CURVE_SKILL_BASE = 0.4f;
    static constexpr float MIN_ACTION_COOLDOWN = 0.45f / Tempo::ACTION;
    static constexpr float MAX_ACTION_COOLDOWN = 0.9f / Tempo::ACTION;
    static constexpr float PASS_RELEASE_COOLDOWN = 0.07f / Tempo::ACTION;
    static constexpr float GROUND_FRICTION = 0.965f;
    static constexpr float LOFTED_FRICTION = 0.975f;
    static constexpr float LANE_START_MARGIN = 0.06f;
    static constexpr float LANE_END_MARGIN = 0.97f;
    static constexpr float BASE_INTERCEPTION_RADIUS = 0.026f;
    static constexpr float DEFENDING_INTERCEPTION_BONUS = 0.020f;
    static constexpr float PACE_INTERCEPTION_BONUS = 0.008f;
    static constexpr float BASE_INTERCEPTION_RISK = 0.55f;
    static constexpr float LATE_LANE_RISK = 0.35f;
    static constexpr float OFFSIDE_MARGIN = 0.004f;
  };

  struct Shooting final
  {
    static constexpr float PRESSURE_RADIUS = 0.085f;
    static constexpr float PRESSURE_PENALTY = 0.46f;
    static constexpr float DISTANCE_MIDPOINT_METRES = 15.0f;
    static constexpr float DISTANCE_CURVE_METRES = 4.7f;
    static constexpr float GOAL_ANGLE_REFERENCE_RADIANS = 0.55f;
    static constexpr float MIN_ANGLE_FACTOR = 0.12f;
    static constexpr float BASE_ANGLE_FACTOR = 0.35f;
    static constexpr float ANGLE_FACTOR_BONUS = 0.65f;
    static constexpr float CENTRALITY_METRES = 24.0f;
    static constexpr float BASE_CENTRALITY = 0.78f;
    static constexpr float CENTRALITY_BONUS = 0.22f;
    static constexpr float BASE_SKILL_FACTOR = 0.62f;
    static constexpr float SHOOTING_SKILL_FACTOR = 0.54f;
    static constexpr float MIN_OPEN_PLAY_XG = 0.005f;
    static constexpr float MAX_OPEN_PLAY_XG = 0.64f;
    static constexpr float PENALTY_XG = 0.78f;
    static constexpr float HEADER_XG_FACTOR = 0.45f;
    static constexpr float GOAL_HALF_WIDTH_METRES = 3.66f;
    static constexpr float KEEPER_CENTRED_METRES = 0.3f;
    static constexpr float NEAR_SIDE_AIM_CHANCE = 0.25f;
    // Share of shots simply struck hard at the frame rather than placed.
    static constexpr float POWER_SHOT_BASE_CHANCE = 0.75f;
    static constexpr float POWER_SHOT_SKILL_REDUCTION = 0.10f;
    static constexpr float POWER_SHOT_WIDTH_METRES = 1.3f;
    static constexpr float AIM_POST_INSET_BASE = 0.45f;
    static constexpr float AIM_POST_INSET_SKILL = 0.5f;
    static constexpr float AIM_INSET_SPREAD = 1.2f;
    static constexpr float PENALTY_AIM_INSET_SPREAD = 0.5f;
    static constexpr float AIM_MIN_HEIGHT_METRES = 0.2f;
    static constexpr float AIM_MAX_HEIGHT_METRES = 1.9f;
    static constexpr float HEADER_AIM_MAX_HEIGHT_METRES = 1.4f;
    static constexpr float ERROR_BASE_METRES = 0.95f;
    static constexpr float ERROR_SKILL_METRES = 1.4f;
    static constexpr float ERROR_PRESSURE_METRES = 0.9f;
    static constexpr float ERROR_REFERENCE_METRES = 16.0f;
    static constexpr float ERROR_MIN_DISTANCE_SCALE = 0.45f;
    static constexpr float PENALTY_ERROR_SCALE = 0.35f;
    static constexpr float VERTICAL_ERROR_SHARE = 0.6f;
    static constexpr float MIN_CROSSING_HEIGHT_METRES = 0.12f;
    static constexpr float MAX_CROSSING_HEIGHT_METRES = 6.0f;
    static constexpr float POWER_SPEED_BONUS = 0.10f * Tempo::ACTION;
    static constexpr float MIN_SPEED_VARIATION = 0.88f;
    static constexpr float RELEASE_HEIGHT_METRES = 0.2f;
    static constexpr float WOODWORK_BAND_METRES = 0.10f;
    static constexpr float WOODWORK_REBOUND = 0.35f;
    static constexpr float MAX_SET_PIECE_XG = 0.82f;
    static constexpr float MIN_GOAL_PROBABILITY = 0.01f;
    static constexpr float BASE_BALL_SPEED = 0.82f * Tempo::ACTION;
    static constexpr float SHOOTING_SPEED_BONUS = 0.35f * Tempo::ACTION;
    static constexpr float BALL_FRICTION = 0.985f;
    static constexpr float RELEASE_COOLDOWN = 0.06f / Tempo::ACTION;
    static constexpr float MIN_ACTION_COOLDOWN = 0.7f / Tempo::ACTION;
    static constexpr float MAX_ACTION_COOLDOWN = 1.25f / Tempo::ACTION;
  };

  struct Defending final
  {
    static constexpr float TACKLE_DISTANCE = 0.040f;
    static constexpr float MIN_TACKLE_COOLDOWN = 0.30f / Tempo::ACTION;
    static constexpr float MAX_TACKLE_COOLDOWN = 0.70f / Tempo::ACTION;
    static constexpr float PRESSING_COOLDOWN_REDUCTION = 0.25f;
    static constexpr float TACKLE_COOLDOWN_BASE_MULTIPLIER = 1.1f;
    static constexpr float BASE_WIN_CHANCE = 0.12f;
    static constexpr float DEFENDING_WIN_BONUS = 0.42f;
    static constexpr float DRIBBLING_WIN_PENALTY = 0.27f;
    static constexpr float PRESSING_WIN_BONUS = 0.08f;
    static constexpr float MIN_WIN_CHANCE = 0.08f;
    static constexpr float MAX_WIN_CHANCE = 0.58f;
    static constexpr float PHYSICALITY_DUEL_WEIGHT = 0.12f;
    static constexpr float BASE_FOUL_CHANCE = 0.75f;
    static constexpr float RISK_FOUL_BONUS = 0.10f;
    static constexpr float TECHNIQUE_FOUL_BONUS = 0.10f;
    static constexpr float WINNING_TACKLE_FOUL_SHARE = 0.25f;
    static constexpr float PENALTY_AREA_FOUL_SCALE = 0.25f;
    // A won challenge often only pokes the ball loose.
    static constexpr float POKE_LOOSE_CHANCE = 0.45f;
    static constexpr float MIN_POKE_SPEED = 0.22f * Tempo::ACTION;
    static constexpr float MAX_POKE_SPEED = 0.45f * Tempo::ACTION;
    // Defenders pressed in their own third with no safe pass clear it.
    static constexpr float CLEARANCE_MAX_DEPTH = 0.33f;
    static constexpr float CLEARANCE_MIN_DISTANCE = 0.28f;
    static constexpr float CLEARANCE_MAX_DISTANCE = 0.48f;
    static constexpr float CLEARANCE_SPEED = 0.9f * Tempo::ACTION;
    static constexpr float CLEARANCE_TOUCHLINE_BIAS = 0.35f;
    static constexpr float MIN_RECOVERY_COOLDOWN = 0.35f / Tempo::ACTION;
    static constexpr float MAX_RECOVERY_COOLDOWN = 0.75f / Tempo::ACTION;
    static constexpr float BLOCK_DISTANCE = 0.022f;
    static constexpr float BASE_BLOCK_CHANCE = 0.40f;
    static constexpr float DEFENDING_BLOCK_BONUS = 0.25f;
    static constexpr float DEFLECTION_SPEED_FACTOR = -0.22f;
    static constexpr float MAX_DEFLECTION_Y_SPEED = 0.18f * Tempo::ACTION;
    static constexpr float DEFLECTION_COOLDOWN = 0.09f / Tempo::ACTION;
  };

  struct Ball final
  {
    static constexpr float GRAVITY = 0.72f * Tempo::ACTION * Tempo::ACTION;
    static constexpr float BOUNCE_FACTOR = 0.28f;
    static constexpr float MIN_BOUNCE_SPEED = 0.025f * Tempo::ACTION;
    static constexpr float CURVE_DECAY = 0.92f;
    static constexpr float STOP_SPEED = 0.012f * Tempo::ACTION;
    static constexpr float GOALKEEPER_CONTROL_RADIUS = 0.042f;
    static constexpr float OUTFIELD_CONTROL_RADIUS = 0.026f;
    static constexpr float BASE_CONTROL_CHANCE = 0.72f;
    static constexpr float TOUCH_SKILL_BONUS = 0.45f;
    static constexpr float SPEED_CONTROL_PENALTY = 0.18f / Tempo::ACTION;
    static constexpr float MIN_CONTROL_CHANCE = 0.28f;
    static constexpr float MAX_CONTROL_CHANCE = 0.95f;
    static constexpr float FAILED_TRAP_TIME = 0.15f / Tempo::ACTION;
    static constexpr float FAILED_TOUCH_DELAY = 0.08f / Tempo::ACTION;
    static constexpr float HEAVY_TOUCH_RETAINED = 0.25f;
    static constexpr float MIN_HEAVY_TOUCH_SPEED = 0.05f * Tempo::ACTION;
    static constexpr float MAX_HEAVY_TOUCH_SPEED = 0.16f * Tempo::ACTION;
    static constexpr float BASE_TRAP_TIME = 0.10f / Tempo::ACTION;
    static constexpr float TRAP_SKILL_PENALTY = 0.18f / Tempo::ACTION;
    static constexpr float MIN_POST_TOUCH_DELAY = 0.12f / Tempo::ACTION;
    static constexpr float MAX_POST_TOUCH_DELAY = 0.38f / Tempo::ACTION;
    static constexpr float PASSING_TOUCH_WEIGHT = 0.85f;
    static constexpr float SAVE_DISTANCE = 0.045f;
    static constexpr int MAX_FLIGHT_STEPS = 900;
    static constexpr float SAVE_DIVE_TIME = 0.35f / Tempo::ACTION;
    static constexpr float SAVE_ACTION_COOLDOWN = 0.65f / Tempo::ACTION;
    static constexpr float GOAL_NET_BALL_DEPTH = 0.035f;
  };

  struct Rules final
  {
    static constexpr int MAX_SUBSTITUTIONS_PER_TEAM = 5;
    static constexpr long MINIMUM_PLAYERS = 7;
    static constexpr float PARKED_PLAYER_OFFSET = 0.06f;
    static constexpr float PARKED_PLAYER_SPACING = 0.025f;
    static constexpr float DEFENSIVE_SLOT_DEPTH = 0.30f;
    static constexpr float SHORT_HANDED_DROP = 0.02f;
    // Crowd-driven home advantage on execution; referee bias is modelled in
    // Discipline (card biases).
    static constexpr float HOME_EXECUTION_BONUS = 0.05f;
    static constexpr float HOME_FINAL_THIRD_START = 0.67f;
    static constexpr float AWAY_FINAL_THIRD_START = 0.33f;
  };

  /**
   * Goalkeeper model. Real-world reaction and dive values (seconds, metres,
   * m/s) are converted with Units::ACTION_SECONDS_PER_SIM_SECOND.
   */
  struct Goalkeeper final
  {
    static constexpr float REACTION_BASE_SECONDS = 0.21f;
    static constexpr float REACTION_SKILL_SECONDS = 0.06f;
    static constexpr float SCREENED_REACTION_SECONDS = 0.10f;
    static constexpr float SCREEN_WIDTH_METRES = 0.9f;
    static constexpr float READ_ERROR_METRES = 0.8f;
    static constexpr float DIVE_POST_MARGIN = 0.02f;
    static constexpr float DIVE_ACCELERATION_METRES = 9.0f;
    static constexpr float DIVE_BASE_SPEED_METRES = 3.6f;
    static constexpr float DIVE_SKILL_SPEED_METRES = 1.6f;
    static constexpr float BODY_REACH_METRES = 1.3f;
    static constexpr float HEIGHT_REACH_GAIN = 0.8f;
    static constexpr float HIGH_BALL_METRES = 1.9f;
    static constexpr float HIGH_BALL_REACH_SCALE = 0.85f;
    static constexpr float COMFORT_SPEED_METRES = 24.0f;
    static constexpr float SAVE_BASE = 0.97f;
    static constexpr float SAVE_STRETCH_PENALTY = 0.35f;
    static constexpr float SAVE_SPEED_PENALTY = 0.015f;
    static constexpr float SAVE_SKILL_BONUS = 0.22f;
    static constexpr float MIN_SAVE_CHANCE = 0.05f;
    static constexpr float MAX_SAVE_CHANCE = 0.98f;
    static constexpr float HOLD_BASE = 0.35f;
    static constexpr float HOLD_SKILL = 0.40f;
    static constexpr float HOLD_STRETCH_PENALTY = 0.45f;
    static constexpr float HOLD_SPEED_PENALTY = 0.02f;
    static constexpr float MIN_HOLD_CHANCE = 0.05f;
    static constexpr float MAX_HOLD_CHANCE = 0.90f;
    static constexpr float TIP_OVER_METRES = 2.05f;
    static constexpr float TIP_AROUND_STRETCH = 0.5f;
    static constexpr float PARRY_SPEED_SHARE = 0.35f;
    static constexpr float PARRY_DEFLECTION_Z = 0.08f * Tempo::ACTION;
    static constexpr float PARRY_COOLDOWN = 0.06f / Tempo::ACTION;
    static constexpr float RECOVER_TIME_SECONDS = 0.30f / Tempo::ACTION;
    static constexpr float DISTRIBUTE_TIME_SECONDS = 0.15f / Tempo::ACTION;
    static constexpr float PENALTY_READ_BASE = 0.0f;
    static constexpr float PENALTY_READ_SKILL = 0.12f;
    static constexpr float PENALTY_STAY_CHANCE = 0.08f;
    static constexpr float PENALTY_DIVE_METRES = 1.2f;
    static constexpr float PENALTY_PRE_MOVE_METRES = 0.0f;
    static constexpr float PENALTY_START_SPEED_SHARE = 0.0f;
    static constexpr float CLAIM_LOOKAHEAD_SECONDS = 0.25f / Tempo::ACTION;
    static constexpr float CLAIM_BOX_DEPTH = 0.12f;
    static constexpr float SWEEP_ADVANTAGE = 1.25f;
    static constexpr float RUSH_COVER_DISTANCE = 0.06f;
    static constexpr float RUSH_CLOSING_SHARE = 0.55f;
    static constexpr float NEAR_DEPTH_METRES = 0.8f;
    static constexpr float DEPTH_PER_METRE = 0.11f;
    static constexpr float SWEEPER_DISTANCE_METRES = 35.0f;
    static constexpr float SWEEPER_DEPTH_PER_METRE = 0.18f;
    static constexpr float MAX_DEPTH_METRES = 16.0f;
  };

  struct SetPiece final
  {
    static constexpr float CROSSING_FREE_KICK_METRES = 36.0f;
    static constexpr float DIRECT_FREE_KICK_METRES = 30.0f;
    static constexpr float DIRECT_FREE_KICK_WIDTH_METRES = 16.0f;
    static constexpr float CROSS_MIN_WIDTH_METRES = 16.0f;
    static constexpr float DIRECT_SHOT_BASE = 0.55f;
    static constexpr float DIRECT_SHOT_SKILL = 0.45f;
    static constexpr float DIRECT_SHOT_DISTANCE_PENALTY = 0.012f;
    static constexpr float DIRECT_FREE_KICK_XG = 0.065f;
    static constexpr float WALL_BLOCK_BASE = 0.34f;
    static constexpr float WALL_BLOCK_SKILL = 0.20f;
    static constexpr float WALL_REBOUND_SPEED_SHARE = 0.35f;
    static constexpr float WALL_SCREEN_SECONDS = 0.05f;
    static constexpr float SHORT_CORNER_CHANCE = 0.12f;
    static constexpr float SHORT_CORNER_MAX_DISTANCE = 0.16f;
    static constexpr float NEAR_POST_CHANCE = 0.42f;
    static constexpr float TARGET_RANDOMNESS = 0.6f;
    static constexpr std::size_t REST_DEFENDERS = 3;
    static constexpr float MARKING_GOAL_SIDE_OFFSET = 0.008f;
    static constexpr float PENALTY_WAIT_OFFSET = 0.01f;
    static constexpr float SET_PIECE_PHASE_SECONDS = 1.2f / Tempo::ACTION;
    // Defensive clearances and blocks near the own goal line often go out
    // for a corner.
    static constexpr float CLEARANCE_BEHIND_CHANCE = 0.7f;
    static constexpr float BLOCK_BEHIND_CHANCE = 0.6f;
    static constexpr float BLOCK_BEHIND_DEPTH = 0.20f;
    static constexpr float CROSS_CLEARANCE_DEPTH = 0.25f;
  };

  /**
   * Unit conventions. Pitch x is normalised by LENGTH_METRES, y by
   * WIDTH_METRES and ball height z by LENGTH_METRES (so z * 105 = metres).
   *
   * The clock is compressed (one simulated second is one match minute) while
   * on-ball action plays roughly ACTION_SECONDS_PER_SIM_SECOND times faster
   * than real time, so real-world reaction and dive times are divided by
   * that factor. Players only cover a small part of a real match distance in
   * 90 simulated seconds; DISTANCE_REPORT_SCALE maps the simulated path to an
   * estimated real-match distance for reporting only.
   */
  struct Units final
  {
    static constexpr float BALL_Z_METRES = 105.0f;
    static constexpr float ACTION_SECONDS_PER_SIM_SECOND = 3.8f * Tempo::ACTION;
    static constexpr float DISTANCE_REPORT_SCALE = 12.5f / Tempo::ACTION;
    static constexpr float CROSSBAR_HEIGHT_METRES = 2.44f;
    static constexpr float BALL_RADIUS_METRES = 0.11f;
    static constexpr float DEFAULT_PLAYER_HEIGHT_METRES = 1.80f;
  };

  struct Rating final
  {
    static constexpr float BASELINE = 6.0f;
    static constexpr float MINIMUM = 3.0f;
    static constexpr float MAXIMUM = 10.0f;
    static constexpr float GOAL = 1.05f;
    static constexpr float OWN_GOAL = -0.9f;
    static constexpr float ASSIST = 0.65f;
    static constexpr float SHOT_ON_TARGET = 0.12f;
    static constexpr float SHOT_OFF_TARGET = -0.04f;
    static constexpr float KEY_PASS = 0.18f;
    static constexpr float COMPLETED_PASS = 0.012f;
    static constexpr float FAILED_PASS = -0.035f;
    static constexpr float TACKLE_WON = 0.13f;
    static constexpr float INTERCEPTION = 0.10f;
    static constexpr float CLEARANCE = 0.05f;
    static constexpr float AERIAL_WON = 0.04f;
    static constexpr float SAVE = 0.32f;
    static constexpr float GOALKEEPER_GOAL_CONCEDED = -0.38f;
    static constexpr float DEFENDER_GOAL_CONCEDED = -0.14f;
    static constexpr float CLEAN_SHEET_GOALKEEPER = 0.6f;
    static constexpr float CLEAN_SHEET_DEFENDER = 0.35f;
    static constexpr float CLEAN_SHEET_MINIMUM_MINUTES = 60.0f;
    static constexpr float FOUL = -0.06f;
    static constexpr float YELLOW_CARD = -0.35f;
    static constexpr float RED_CARD = -1.6f;
    static constexpr float RESULT_BONUS = 0.25f;
    static constexpr float FULL_MATCH_MINUTES = 90.0f;
  };

  struct Discipline final
  {
    static constexpr float STRICTNESS_SD = 0.13f;
    static constexpr float MIN_STRICTNESS = 0.65f;
    static constexpr float MAX_STRICTNESS = 1.4f;
    static constexpr float RECKLESS_SHARE = 0.17f;
    static constexpr float CARELESS_YELLOW_CHANCE = 0.026f;
    static constexpr float RECKLESS_YELLOW_CHANCE = 0.30f;
    static constexpr float TACTICAL_YELLOW_CHANCE = 0.34f;
    static constexpr float SERIOUS_FOUL_PLAY_SHARE = 0.0015f;
    static constexpr float DOGSO_RED_CHANCE = 0.85f;
    static constexpr float PENALTY_AREA_DOGSO_RED_CHANCE = 0.25f;
    static constexpr float AWAY_CARD_BIAS = 1.07f;
    static constexpr float HOME_CARD_BIAS = 0.94f;
    static constexpr float BOOKED_PLAYER_CAUTION = 0.35f;
    static constexpr float ADVANTAGE_WINDOW_SECONDS = 0.8f / Tempo::ACTION;
    static constexpr float ADVANTAGE_MIN_OPENNESS = 0.45f;
    static constexpr float ADVANTAGE_PLAY_CHANCE = 0.45f;
    static constexpr float DOGSO_MAX_DISTANCE_METRES = 28.0f;
    static constexpr float DOGSO_MAX_WIDTH_DEVIATION = 0.22f;
  };

  struct Injury final
  {
    // Non-contact hazard per simulated second (one match minute) for a fresh
    // player at moderate intensity; low condition and sprinting raise it.
    static constexpr float BASE_HAZARD_PER_SECOND = 0.00009f;
    static constexpr float FATIGUE_HAZARD_MULTIPLIER = 3.0f;
    static constexpr float INTENSITY_HAZARD_MULTIPLIER = 1.5f;
    static constexpr float CONTACT_INJURY_CHANCE = 0.006f;
    static constexpr float RECKLESS_CONTACT_INJURY_CHANCE = 0.03f;
    static constexpr float INJURED_SPEED_SCALE = 0.55f;
    static constexpr float CHECK_INTERVAL_SECONDS = 1.0f;
  };

  struct Stoppage final
  {
    static constexpr float FIRST_HALF_BASE_MINUTES = 1.0f;
    static constexpr float SECOND_HALF_BASE_MINUTES = 1.8f;
    static constexpr float MINUTES_PER_GOAL = 0.7f;
    static constexpr float MINUTES_PER_SUBSTITUTION = 0.3f;
    static constexpr float MINUTES_PER_CARD = 0.3f;
    static constexpr float MINUTES_PER_INJURY = 1.2f;
    static constexpr float MINUTES_PER_PENALTY = 0.5f;
    static constexpr int MIN_ADDED_MINUTES = 1;
    static constexpr int MAX_ADDED_MINUTES = 12;
    // A dangerous attack (corner, penalty, shot in flight) is allowed to
    // finish; this bounds how long the referee waits past the added time.
    static constexpr float MAX_OVERRUN_MINUTES = 1.5f;
  };

  struct Substitution final
  {
    static constexpr int MAX_WINDOWS = 3;
    static constexpr float EARLIEST_TACTICAL_MINUTE = 56.0f;
    static constexpr float FATIGUE_THRESHOLD = 0.70f;
    static constexpr float FATIGUE_THRESHOLD_LATE_GAIN = 0.12f;
    static constexpr float LATE_GAME_MINUTE = 80.0f;
    static constexpr float CARD_RISK_MINUTE = 58.0f;
    static constexpr float CARD_RISK_NEED = 0.22f;
    static constexpr float TRAILING_CHASE_MINUTE = 62.0f;
    static constexpr float LEADING_PROTECT_MINUTE = 75.0f;
    static constexpr float TACTICAL_NEED = 0.18f;
    static constexpr float MINIMUM_NEED = 0.26f;
    static constexpr float MINUTE_NEED_GAIN = 0.012f;
    static constexpr float FATIGUE_NEED_SCALE = 2.2f;
    static constexpr float ROLE_FIT_BONUS = 0.5f;
  };

  /**
   * Stamina (match condition) model. Drain grows with the square of the
   * running speed plus an extra sprint term, and is scaled by the Stamina
   * attribute, so tired players lose top speed and technique late on.
   */
  struct Fatigue final
  {
    static constexpr float IDLE_DRAIN = 0.0030f;
    static constexpr float RUNNING_DRAIN = 0.022f;
    static constexpr float SPRINT_THRESHOLD = 0.78f;
    static constexpr float SPRINT_DRAIN = 0.020f;
    static constexpr float PRESSING_DRAIN = 0.004f;
    static constexpr float ENDURANCE_BASE = 1.40f;
    static constexpr float ENDURANCE_RELIEF = 0.80f;
    static constexpr float HALF_TIME_RECOVERY = 0.06f;
    static constexpr float TECHNIQUE_ERROR_GAIN = 0.35f;
  };

  struct Aerial final
  {
    static constexpr float CONTROL_CEILING_METRES = 1.55f;
    static constexpr float BASE_JUMP_METRES = 0.32f;
    static constexpr float PHYSICALITY_JUMP_METRES = 0.38f;
    static constexpr float HEAD_OFFSET_METRES = 0.08f;
    static constexpr float GOALKEEPER_ARM_REACH_RATIO = 1.26f;
    static constexpr float GOALKEEPER_BASE_JUMP_METRES = 0.30f;
    static constexpr float GOALKEEPER_SKILL_JUMP_METRES = 0.30f;
    static constexpr float DUEL_RADIUS = 0.030f;
    static constexpr float GOALKEEPER_CLAIM_RADIUS = 0.050f;
    static constexpr float BASE_CLAIM_CHANCE = 0.72f;
    static constexpr float CROWDING_PENALTY = 0.08f;
    static constexpr float SKILL_CLAIM_BONUS = 0.24f;
    static constexpr float DUEL_FOUL_CHANCE = 0.08f;
    static constexpr float HEADER_SHOT_MIN_DEPTH = 0.84f;
    static constexpr float HEADER_SHOT_WIDTH = 0.18f;
    static constexpr float HEADER_CLEARANCE_SPEED = 0.62f * Tempo::ACTION;
    static constexpr float HEADER_PASS_SPEED = 0.38f * Tempo::ACTION;
    static constexpr float HEADER_LIFT = 0.18f * Tempo::ACTION;
    static constexpr float HEADER_ACCURACY_PENALTY = 1.6f;
    static constexpr float HEADER_SPEED_SCALE = 0.62f;
    static constexpr float ARRIVAL_HEIGHT_METRES = 2.1f;
  };

  struct Statistics final
  {
    static constexpr float EVEN_POSSESSION_PERCENT = 50.0f;
    static constexpr float PERCENT_SCALE = 100.0f;
  };
};
