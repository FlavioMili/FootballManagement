// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstddef>

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
  struct Pitch final
  {
    static constexpr float LENGTH_METRES = 105.0f;
    static constexpr float WIDTH_METRES = 68.0f;
    static constexpr float CENTRE = 0.5f;
    static constexpr float GOAL_TOP = 0.446f;
    static constexpr float GOAL_BOTTOM = 0.554f;
    static constexpr float LEFT_SHOT_TARGET = -0.015f;
    static constexpr float RIGHT_SHOT_TARGET = 1.015f;
    static constexpr float LEFT_GOALKEEPER_X = 0.035f;
    static constexpr float RIGHT_GOALKEEPER_X = 0.965f;
    static constexpr float LINEUP_GOALKEEPER_X = 0.04f;
    static constexpr float GOALKEEPER_MIN_Y = 0.38f;
    static constexpr float GOALKEEPER_MAX_Y = 0.62f;
    static constexpr float GOALKEEPER_SWEEP_DEPTH = 0.18f;
    static constexpr float GOALKEEPER_SWEEP_OFFSET = 0.035f;
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
    static constexpr float PHYSICS_REFERENCE_STEP_SECONDS = 1.0f / 30.0f;
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
    static constexpr float THROW_IN_DELAY_SECONDS = 0.45f;
    static constexpr float GOAL_KICK_DELAY_SECONDS = 0.7f;
    static constexpr float CORNER_DELAY_SECONDS = 0.65f;
    static constexpr float FREE_KICK_DELAY_SECONDS = 0.6f;
    static constexpr float PENALTY_DELAY_SECONDS = 0.8f;
    static constexpr float GOAL_CELEBRATION_SECONDS = 2.5f;
    static constexpr float POSSESSION_TRANSITION_SECONDS = 2.4f;
    static constexpr std::size_t MAX_EVENTS = 250;
  };

  struct Player final
  {
    static constexpr float DEFAULT_ATTRIBUTE = 0.5f;
    static constexpr float RATING_SCALE = 100.0f;
    static constexpr float BASE_MAX_SPEED = 0.20f;
    static constexpr float PACE_SPEED_BONUS = 0.16f;
    static constexpr float BASE_ACCELERATION = 0.65f;
    static constexpr float PACE_ACCELERATION_BONUS = 0.55f;
    static constexpr float TURN_RATE_RADIANS = 6.2831853f;
    static constexpr float MINIMUM_STAMINA = 0.35f;
    static constexpr float STAMINA_SPEED_BASE = 0.72f;
    static constexpr float STAMINA_SPEED_BONUS = 0.28f;
    static constexpr float IDLE_STAMINA_DRAIN = 0.0005f;
    static constexpr float MOVEMENT_STAMINA_DRAIN = 0.0016f;
    static constexpr float PRESSING_STAMINA_DRAIN = 0.0005f;
    static constexpr float HALF_TIME_STAMINA_RECOVERY = 0.015f;
    static constexpr float MOVEMENT_FACING_THRESHOLD = 0.005f;
    static constexpr float TACTICAL_TARGET_RESPONSE_PER_SECOND = 6.0f;
    static constexpr float URGENT_TARGET_RESPONSE_PER_SECOND = 11.0f;
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
    static constexpr float SUBSTITUTION_SETTLE_SECONDS = 0.4f;
    static constexpr float LOOSE_BALL_LOOKAHEAD_SECONDS = 0.30f;
    static constexpr float COVER_LOOSE_BALL_OFFSET = 0.055f;
    static constexpr float PASS_RECEIVER_LOOKAHEAD_SECONDS = 0.22f;
    static constexpr float MINIMUM_PURSUIT_SPEED = 0.05f;
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
    static constexpr std::size_t MAX_COMMITTED_RUNNERS = 2;
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
    static constexpr float PRESSER_CONTINUITY_SECONDS = 0.20f;
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
    static constexpr float MIN_DRIBBLE_TIME = 0.24f;
    static constexpr float MAX_DRIBBLE_TIME = 0.58f;

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
    static constexpr float LATE_GAME_MINUTE = 82.0f;
    static constexpr float LATE_LEAD_SPECULATIVE_PENALTY = 0.55f;
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
    static constexpr float BASE_PROGRESS_WEIGHT = 1.05f;
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
    static constexpr float WIDE_ATTACK_MINIMUM_Y = 0.24f;
    static constexpr float WIDE_ATTACK_MAXIMUM_Y = 0.76f;
    static constexpr float CENTRAL_TARGET_MINIMUM_Y = 0.30f;
    static constexpr float CENTRAL_TARGET_MAXIMUM_Y = 0.70f;
    static constexpr float CROSS_MINIMUM_PASSER_DEPTH = 0.67f;
    static constexpr float CROSS_MINIMUM_RECEIVER_DEPTH = 0.72f;
    static constexpr float CUTBACK_MINIMUM_PASSER_DEPTH = 0.86f;
    static constexpr float CUTBACK_MAXIMUM_PROGRESSION = 0.035f;
    static constexpr float CUTBACK_MINIMUM_PROGRESSION = -0.24f;
    static constexpr float CROSS_UTILITY_BONUS = 0.50f;
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
    static constexpr float ESTIMATED_BALL_SPEED = 0.75f;
    static constexpr float RECEIVER_LEAD_SCALE = 0.45f;
    static constexpr float TECHNICAL_ERROR = 0.026f;
    static constexpr float PRESSURE_ERROR = 0.014f;
    static constexpr float DISTANCE_ERROR = 0.012f;
    static constexpr float SPEED_DISTANCE_SCALE = 0.34f;
    static constexpr float MIN_BALL_SPEED = 0.38f;
    static constexpr float MAX_BALL_SPEED = 0.95f;
    static constexpr float BASE_BALL_SPEED = 0.82f;
    static constexpr float PASSING_SPEED_BONUS = 0.25f;
    static constexpr float GROUND_VERTICAL_SPEED = 0.04f;
    static constexpr float LOFTED_VERTICAL_SPEED = 0.32f;
    static constexpr float CROSS_VERTICAL_SPEED = 0.20f;
    static constexpr float MAX_CURVE = 0.18f;
    static constexpr float CURVE_SKILL_BASE = 0.4f;
    static constexpr float MIN_ACTION_COOLDOWN = 0.45f;
    static constexpr float MAX_ACTION_COOLDOWN = 0.9f;
    static constexpr float PASS_RELEASE_COOLDOWN = 0.07f;
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
    static constexpr float PENALTY_XG = 0.76f;
    static constexpr float MAX_SET_PIECE_XG = 0.82f;
    static constexpr float DEFAULT_GOALKEEPER_ABILITY = 0.45f;
    static constexpr float GOAL_PROBABILITY_BASE = 1.15f;
    static constexpr float GOALKEEPER_GOAL_REDUCTION = 0.35f;
    static constexpr float MIN_GOAL_PROBABILITY = 0.01f;
    static constexpr float MAX_GOAL_PROBABILITY = 0.80f;
    static constexpr float BASE_ON_TARGET_PROBABILITY = 0.30f;
    static constexpr float SHOOTING_ON_TARGET_BONUS = 0.48f;
    static constexpr float DISTANCE_ON_TARGET_DIVISOR = 180.0f;
    static constexpr float MIN_ON_TARGET_PROBABILITY = 0.22f;
    static constexpr float MAX_ON_TARGET_PROBABILITY = 0.78f;
    static constexpr float TARGET_POST_INSET = 0.006f;
    static constexpr float WIDE_TARGET_MIN_Y = 0.33f;
    static constexpr float WIDE_TARGET_MAX_Y = 0.67f;
    static constexpr float WIDE_TARGET_POST_MARGIN = 0.012f;
    static constexpr float WIDE_TARGET_TOP_SIDE_CHANCE = 0.5f;
    static constexpr float BASE_BALL_SPEED = 0.82f;
    static constexpr float SHOOTING_SPEED_BONUS = 0.35f;
    static constexpr float MIN_VERTICAL_SPEED = 0.02f;
    static constexpr float MAX_VERTICAL_SPEED = 0.14f;
    static constexpr float MAX_CURVE = 0.24f;
    static constexpr float CURVE_SKILL_BASE = 0.5f;
    static constexpr float BALL_FRICTION = 0.985f;
    static constexpr float RELEASE_COOLDOWN = 0.06f;
    static constexpr float MIN_ACTION_COOLDOWN = 0.7f;
    static constexpr float MAX_ACTION_COOLDOWN = 1.25f;
  };

  struct Defending final
  {
    static constexpr float TACKLE_DISTANCE = 0.032f;
    static constexpr float MIN_TACKLE_COOLDOWN = 0.55f;
    static constexpr float MAX_TACKLE_COOLDOWN = 1.1f;
    static constexpr float PRESSING_COOLDOWN_REDUCTION = 0.25f;
    static constexpr float TACKLE_COOLDOWN_BASE_MULTIPLIER = 1.1f;
    static constexpr float BASE_WIN_CHANCE = 0.12f;
    static constexpr float DEFENDING_WIN_BONUS = 0.42f;
    static constexpr float DRIBBLING_WIN_PENALTY = 0.27f;
    static constexpr float PRESSING_WIN_BONUS = 0.08f;
    static constexpr float MIN_WIN_CHANCE = 0.08f;
    static constexpr float MAX_WIN_CHANCE = 0.58f;
    static constexpr float BASE_FOUL_CHANCE = 0.055f;
    static constexpr float RISK_FOUL_BONUS = 0.055f;
    static constexpr float YELLOW_CARD_CHANCE = 0.14f;
    static constexpr float MIN_RECOVERY_COOLDOWN = 0.35f;
    static constexpr float MAX_RECOVERY_COOLDOWN = 0.75f;
    static constexpr float BLOCK_DISTANCE = 0.017f;
    static constexpr float BASE_BLOCK_CHANCE = 0.12f;
    static constexpr float DEFENDING_BLOCK_BONUS = 0.28f;
    static constexpr float DEFLECTION_SPEED_FACTOR = -0.22f;
    static constexpr float MAX_DEFLECTION_Y_SPEED = 0.18f;
    static constexpr float DEFLECTION_COOLDOWN = 0.09f;
  };

  struct Ball final
  {
    static constexpr float GRAVITY = 0.72f;
    static constexpr float BOUNCE_FACTOR = 0.28f;
    static constexpr float MIN_BOUNCE_SPEED = 0.025f;
    static constexpr float CURVE_DECAY = 0.92f;
    static constexpr float STOP_SPEED = 0.012f;
    static constexpr float GOALKEEPER_CONTROL_RADIUS = 0.042f;
    static constexpr float OUTFIELD_CONTROL_RADIUS = 0.026f;
    static constexpr float BASE_CONTROL_CHANCE = 0.60f;
    static constexpr float TOUCH_SKILL_BONUS = 0.45f;
    static constexpr float SPEED_CONTROL_PENALTY = 0.18f;
    static constexpr float MIN_CONTROL_CHANCE = 0.28f;
    static constexpr float MAX_CONTROL_CHANCE = 0.95f;
    static constexpr float FAILED_TRAP_TIME = 0.15f;
    static constexpr float FAILED_TOUCH_DELAY = 0.08f;
    static constexpr float BASE_TRAP_TIME = 0.10f;
    static constexpr float TRAP_SKILL_PENALTY = 0.18f;
    static constexpr float MIN_POST_TOUCH_DELAY = 0.2f;
    static constexpr float MAX_POST_TOUCH_DELAY = 0.55f;
    static constexpr float PASSING_TOUCH_WEIGHT = 0.85f;
    static constexpr float SAVE_DISTANCE = 0.045f;
    static constexpr float SAVE_DIVE_TIME = 0.35f;
    static constexpr float SAVE_ACTION_COOLDOWN = 0.65f;
    static constexpr float GOAL_NET_BALL_DEPTH = 0.035f;
  };

  struct Rules final
  {
    static constexpr int MAX_SUBSTITUTIONS_PER_TEAM = 5;
    static constexpr float HOME_FINAL_THIRD_START = 0.67f;
    static constexpr float AWAY_FINAL_THIRD_START = 0.33f;
  };

  struct Goalkeeper final
  {
    static constexpr float RUSH_DEPTH = 0.22f;
    static constexpr float RUSH_WIDTH_OFFSET = 0.20f;
    static constexpr float RUSH_ARRIVAL_RESPONSE = 12.0f;
    static constexpr float SWEEP_RESPONSE = 8.0f;
    static constexpr float SWEEP_FORWARD_GAIN = 1.6f;
    static constexpr float CLAIM_AIR_HEIGHT = 0.35f;
    static constexpr float CLAIM_BOX_DEPTH = 0.20f;
    static constexpr float CLAIM_RESPONSE = 12.0f;
    static constexpr float CLAIM_GRAB_DISTANCE = 0.05f;
    static constexpr float HOLD_TIME_SECONDS = 0.6f;
    static constexpr float DISTRIBUTE_TIME_SECONDS = 0.15f;
    static constexpr float RECOVER_TIME_SECONDS = 0.7f;
    static constexpr float LINE_RESET_RESPONSE = 4.0f;
    static constexpr float PARRY_XG_BASE = 0.12f;
    static constexpr float PARRY_XG_SENSITIVITY = 1.2f;
    static constexpr float PARRY_ABILITY_WEIGHT = 0.5f;
    static constexpr float PARRY_DEFLECTION_Y = 0.10f;
    static constexpr float PARRY_DEFLECTION_Z = 0.10f;
    static constexpr float PARRY_DEFLECTION_SPEED = 0.14f;
    static constexpr float RUSH_SHOT_PRESSURE_BONUS = 0.25f;
  };

  struct Statistics final
  {
    static constexpr float EVEN_POSSESSION_PERCENT = 50.0f;
    static constexpr float PERCENT_SCALE = 100.0f;
  };
};
