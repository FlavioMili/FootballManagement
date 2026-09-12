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
 * The simulation runs in real match time: one simulated second is one second
 * of the match, speeds are in metres per second, accelerations in m/s^2 and
 * durations in seconds unless a name says otherwise. Pitch positions stay
 * normalised (x along the 105 m length, home attacking from 0 to 1; y across
 * the 68 m width). Tactical offsets (Shape) are therefore pitch fractions,
 * while radii, reaches and distances compared with the metric distance
 * between players are in metres.
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
    static constexpr float LINEUP_GOALKEEPER_X = 0.04f;
    static constexpr float GOALKEEPER_MIN_Y = 0.38f;
    static constexpr float GOALKEEPER_MAX_Y = 0.62f;
    static constexpr float GOALKEEPER_SWEEP_DEPTH = 0.18f;
    static constexpr float LEFT_GOAL_KICK_X = 0.065f;
    static constexpr float RIGHT_GOAL_KICK_X = 0.935f;
    static constexpr float LEFT_PENALTY_SPOT_X = 0.105f;
    static constexpr float RIGHT_PENALTY_SPOT_X = 0.895f;
    static constexpr float LEFT_PENALTY_AREA_EDGE = 0.157f;
    static constexpr float RIGHT_PENALTY_AREA_EDGE = 0.843f;
    static constexpr float PENALTY_AREA_HALF_WIDTH_METRES = 20.16f;
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
    /** Decision and body-physics step (10 Hz). */
    static constexpr float FIXED_STEP_SECONDS = 0.1f;
    /**
     * Team shape and off-ball targets refresh every this many steps (5 Hz),
     * and at once whenever possession or the ball's flight changes; bodies
     * move every step. Running load is accounted at the same rate.
     */
    static constexpr std::uint64_t TACTICAL_REFRESH_STEPS = 2;
    /** The free ball is integrated in this many sub-steps per fixed step. */
    static constexpr int BALL_SUBSTEPS = 4;
    /**
     * MatchFidelity::BACKGROUND merges up to this many fixed steps into one
     * while the ball is dead before a throw-in, goal kick, corner, free kick
     * or penalty (players only walk to their restart spots). Live play keeps
     * the 10 Hz step: a coarser one measurably changes shots and goals.
     */
    static constexpr std::uint32_t BACKGROUND_STOPPAGE_TICKS = 3;
    static constexpr float SECONDS_PER_MINUTE = 60.0f;
    // update() bounds catch-up work so a stalled render frame cannot freeze
    // the live view; remaining whole steps are dropped. Headless code uses
    // advance()/simulateToEnd(), which never drop steps.
    static constexpr float MAX_FRAME_DELTA_SECONDS = 4.0f;
    static constexpr int MAX_FIXED_STEPS_PER_UPDATE = 30;
    static constexpr float HALF_TIME_MINUTE = 45.0f;
    static constexpr float FULL_TIME_MINUTE = 90.0f;
    /** Knockout ties still level play two halves of extra time. */
    static constexpr float EXTRA_TIME_HALF_MINUTES = 15.0f;
    /** Breaks before extra time and at its half-time (clock stopped). */
    static constexpr float EXTRA_TIME_BREAK_SECONDS = 10.0f;
    static constexpr float EXTRA_TIME_HALF_TIME_SECONDS = 5.0f;
    /** Penalty shootout pacing: before the first kick, between kicks, and
     * the longest a kick can travel before it counts as missed. */
    static constexpr float SHOOTOUT_START_SECONDS = 20.0f;
    static constexpr float SHOOTOUT_KICK_INTERVAL_SECONDS = 12.0f;
    static constexpr float SHOOTOUT_MAX_FLIGHT_SECONDS = 3.0f;
    /** Kicks per side before sudden death. */
    static constexpr int SHOOTOUT_KICKS = 5;
    /** Interval in the tunnel; the clock is stopped and nothing is live. */
    static constexpr float HALF_TIME_PAUSE_SECONDS = 10.0f;
    // Restart delays from the ball going dead to the restart (real averages:
    // throw-in ~18 s, goal kick ~30 s, corner ~37 s), slightly longer here
    // because fewer balls go out of play than in a real match.
    static constexpr float KICKOFF_DELAY_SECONDS = 4.0f;
    static constexpr float THROW_IN_DELAY_SECONDS = 24.0f;
    static constexpr float GOAL_KICK_DELAY_SECONDS = 36.0f;
    static constexpr float CORNER_DELAY_SECONDS = 40.0f;
    static constexpr float FREE_KICK_DELAY_SECONDS = 24.0f;
    static constexpr float SET_PIECE_FREE_KICK_DELAY_SECONDS = 30.0f;
    static constexpr float PENALTY_DELAY_SECONDS = 55.0f;
    static constexpr float CARD_DELAY_SECONDS = 15.0f;
    static constexpr float INJURY_DELAY_SECONDS = 50.0f;
    static constexpr float SUBSTITUTION_DELAY_SECONDS = 9.0f;
    /** Goal to kick-off: celebration and walk back (clock running). */
    static constexpr float GOAL_CELEBRATION_SECONDS = 55.0f;
    static constexpr float POSSESSION_TRANSITION_SECONDS = 5.0f;
    static constexpr float RATING_REFRESH_SECONDS = 6.0f;
    static constexpr std::size_t MAX_EVENTS = 1024;
  };

  /**
   * Playback helpers for the live view. The simulation itself never depends
   * on them: the viewer chooses how many simulated seconds pass per wall
   * second (1 = real time), and highlight playback skips (simulates
   * headless) between predicted highlight windows.
   */
  struct Playback final
  {
    static constexpr float MIN_SPEED = 1.0f;
    static constexpr float MAX_SPEED = 30.0f;
    static constexpr float DEFAULT_SPEED = 1.0f;
    static constexpr float DEFAULT_HIGHLIGHT_SPEED = 1.0f;
    /** Build-up shown before a highlight trigger (shot, card, penalty...). */
    static constexpr float HIGHLIGHT_LEAD_SECONDS = 15.0f;
    /** Aftermath shown once the trigger happened. */
    static constexpr float HIGHLIGHT_TAIL_SECONDS = 6.0f;
    /** How far ahead a highlight prediction simulates before giving up. */
    static constexpr float PREDICTION_HORIZON_SECONDS = 900.0f;
    /** Highlight windows closer than this are merged into one. */
    static constexpr float MERGE_GAP_SECONDS = 4.0f;
  };

  /**
   * Player kinematics: an in-situ acceleration-speed profile
   * a(v) = A0 (1 - v / vmax) for speeding up, a stronger braking capacity and
   * a speed-dependent lateral (centripetal) limit so sharp turns at speed
   * force a player to slow down first.
   */
  struct Player final
  {
    static constexpr float DEFAULT_ATTRIBUTE = 0.5f;
    static constexpr float RATING_SCALE = 100.0f;
    // Attributes are stretched around a typical professional level so that
    // quality differences between squads matter as much as in real results;
    // the stretch saturates far from it so extreme gaps stay plausible.
    static constexpr float ATTRIBUTE_PIVOT = 0.66f;
    static constexpr float ATTRIBUTE_CONTRAST = 3.6f;
    /**
     * The absolute level of a match (mean raw outfield attribute of both
     * elevens) is partly normalised toward the reference level before the
     * stretch, so quality gaps decide as before but a weaker league does not
     * turn into a shooting gallery; the level shows in finishing precision
     * instead (per unit of raw level).
     */
    static constexpr float REFERENCE_LEVEL = 0.65f;
    static constexpr float LEVEL_NORMALISATION = 0.7f;
    static constexpr float LEVEL_FINISHING_GAIN = 2.0f;
    /** Above the reference finishing improves more slowly. */
    static constexpr float ELITE_FINISHING_GAIN = 1.0f;
    static constexpr float MIN_LEVEL_PRECISION = 0.6f;
    static constexpr float ATTRIBUTE_SATURATION = 0.38f;
    static constexpr float MIN_ATTRIBUTE = 0.02f;
    // Real top speeds cluster at 8-9 m/s with a record near 10.4 m/s.
    static constexpr float TOP_SPEED_BASE = 7.4f;
    static constexpr float TOP_SPEED_PACE = 2.6f;
    // Theoretical maximum acceleration A0 (~7 m/s^2 for a typical pro).
    static constexpr float ACCELERATION_BASE = 5.7f;
    static constexpr float ACCELERATION_PACE = 1.6f;
    static constexpr float ACCELERATION_PHYSICALITY = 0.6f;
    static constexpr float BRAKING_BASE = 6.0f;
    static constexpr float BRAKING_PHYSICALITY = 2.0f;
    /** Lateral acceleration a runner sustains at a standstill / top speed. */
    static constexpr float LATERAL_ACCELERATION_SLOW = 9.0f;
    static constexpr float LATERAL_ACCELERATION_FAST = 5.5f;
    /** Desired-speed approach to a target (comfortable deceleration). */
    static constexpr float ARRIVAL_DECELERATION = 2.0f;
    static constexpr float ARRIVAL_DEAD_ZONE_METRES = 0.35f;
    static constexpr float FACING_TURN_RATE_RADIANS = 9.0f;
    static constexpr float MOVEMENT_FACING_THRESHOLD = 0.3f;
    static constexpr float MINIMUM_STAMINA = 0.35f;
    // Fatigue lowers top speed far more than the first-step push.
    static constexpr float FATIGUE_TOP_SPEED_LOSS = 0.14f;
    static constexpr float FATIGUE_ACCELERATION_LOSS = 0.06f;
    /** Tired players also choose to run less hard (work-rate loss). */
    static constexpr float FATIGUE_WORK_RATE_LOSS = 0.12f;
    /** Top speed kept with an empty repeat-sprint reserve. */
    static constexpr float EMPTY_RESERVE_TOP_SPEED = 0.80f;
    // Intent speed caps as a share of the fresh top speed.
    static constexpr float HOLD_SHAPE_SPEED_SCALE = 0.36f;
    static constexpr float CARRY_BALL_SPEED_SCALE = 0.60f;
    static constexpr float SUPPORT_SPEED_SCALE = 0.46f;
    static constexpr float ATTACKING_RUN_SPEED_SCALE = 0.95f;
    static constexpr float PRESS_SPEED_SCALE = 0.88f;
    static constexpr float COVER_SPEED_SCALE = 0.55f;
    static constexpr float MARKING_SPEED_SCALE = 0.52f;
    static constexpr float RECOVERY_SPEED_SCALE = 0.58f;
    static constexpr float GOALKEEPER_MOVEMENT_SPEED_SCALE = 0.7f;
    static constexpr float RESTART_WALK_SPEED_SCALE = 0.22f;
    /** Distance from the target at which a player runs at the urgency cap. */
    static constexpr float URGENCY_DISTANCE_METRES = 25.0f;
    static constexpr float MAX_URGENCY_SPEED_SCALE = 0.56f;
    /** Perception/intent lag: how quickly a moving target is followed. */
    static constexpr float TACTICAL_TARGET_RESPONSE_PER_SECOND = 1.8f;
    static constexpr float URGENT_TARGET_RESPONSE_PER_SECOND = 4.5f;
    /** Response lost by a side with no familiarity with its tactics. */
    static constexpr float FAMILIARITY_RESPONSE_LOSS = 0.3f;
    /** Two bodies of ~0.4 m radius cannot overlap. */
    static constexpr float MINIMUM_BODY_SEPARATION_METRES = 0.8f;
    static constexpr float BODY_SEPARATION_SHARE = 0.5f;
    static constexpr float BODY_COLLISION_VELOCITY_RETAINED = 0.9f;
    static constexpr float SUBSTITUTION_SETTLE_SECONDS = 1.5f;
    static constexpr float MAX_LOOSE_BALL_LOOKAHEAD_SECONDS = 1.2f;
    static constexpr float COVER_LOOSE_BALL_OFFSET = 0.055f;
    static constexpr float PASS_RECEIVER_LOOKAHEAD_SECONDS = 0.8f;
    static constexpr float MINIMUM_PURSUIT_SPEED = 1.4f;
    static constexpr float CURRENT_VELOCITY_PURSUIT_WEIGHT = 0.55f;
    /** Teleports (restart repositioning) are not counted as running. */
    static constexpr float MAX_COUNTED_STEP_SPEED = 13.0f;
  };

  /**
   * Dribbling: a carrier on the move pushes the ball 0.6-3 m ahead and
   * catches it up at the next touch, so the ball is exposed between touches
   * and a heavy touch can lose it. Defenders jockey goal-side and time their
   * challenge for the moment the ball is away from the attacker's foot.
   */
  struct Dribble final
  {
    /** Below this speed the ball is kept at the feet (close control). */
    static constexpr float CLOSE_CONTROL_SPEED = 2.2f;
    static constexpr float FEET_METRES = 0.35f;
    /** The carrier can strike the ball only when it is this close. */
    static constexpr float KICK_REACH_METRES = 0.9f;
    static constexpr float TOUCH_BASE_METRES = 0.5f;
    static constexpr float TOUCH_METRES_PER_SPEED = 0.28f;
    static constexpr float MIN_TOUCH_METRES = 0.6f;
    static constexpr float MAX_TOUCH_METRES = 3.0f;
    static constexpr float TOUCH_INTERVAL_BASE = 1.25f;
    static constexpr float TOUCH_INTERVAL_PER_SPEED = 0.07f;
    static constexpr float TOUCH_INTERVAL_DRIBBLING = 0.2f;
    static constexpr float MIN_TOUCH_INTERVAL = 0.4f;
    static constexpr float MAX_TOUCH_INTERVAL = 1.2f;
    static constexpr float HEAVY_TOUCH_BASE = 0.045f;
    static constexpr float HEAVY_TOUCH_PRESSURE = 1.5f;
    static constexpr float HEAVY_TOUCH_PRESSURE_METRES = 3.0f;
    static constexpr float MIN_HEAVY_PUSH_SPEED = 2.5f;
    static constexpr float MAX_HEAVY_PUSH_SPEED = 5.0f;
    // Take-ons against a jockeying defender in front.
    static constexpr float TAKE_ON_RANGE_METRES = 2.8f;
    static constexpr float TAKE_ON_BASE = 0.42f;
    static constexpr float TAKE_ON_SKILL = 0.6f;
    static constexpr float TAKE_ON_PACE = 0.25f;
    static constexpr float MIN_TAKE_ON = 0.1f;
    static constexpr float MAX_TAKE_ON = 0.8f;
    static constexpr float TAKE_ON_ANGLE_RADIANS = 0.5f;
    static constexpr float TAKE_ON_TOUCH_METRES = 2.4f;
    /** A beaten defender is wrong-footed: momentum lost, no challenge. */
    static constexpr float BEATEN_SECONDS = 1.1f;
    static constexpr float BEATEN_VELOCITY_RETAINED = 0.25f;
  };

  /** Physical load statistics thresholds (m/s). */
  struct Load final
  {
    static constexpr float HIGH_INTENSITY_SPEED = 5.5f;
    static constexpr float SPRINT_SPEED = 7.0f;
    static constexpr float SPRINT_EXIT_SPEED = 6.5f;
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
    static constexpr float CARRIER_EVASION_RANGE_METRES = 8.0f;
    static constexpr float CARRIER_BASE_EVASION = 0.035f;
    static constexpr float CARRIER_DRIBBLING_EVASION = 0.045f;
    static constexpr float CARRIER_CENTRALITY = 0.16f;
    // Wide players carrying down the flank keep their lane to cross.
    static constexpr float WIDE_LANE_DEVIATION = 0.24f;
    /** In the final third a wide forward sometimes cuts inside instead,
     * more often when he shoots better than he crosses. */
    static constexpr float CUT_INSIDE_BASE = 0.50f;
    static constexpr float CUT_INSIDE_PREFERENCE = 1.0f;
    static constexpr float MIN_CUT_INSIDE = 0.10f;
    static constexpr float MAX_CUT_INSIDE = 0.85f;
    static constexpr float CUT_INSIDE_PULL = 0.8f;
    static constexpr float SUPPORT_BASE_ADVANCE = 0.035f;
    static constexpr float SUPPORT_OFFENSIVE_ADVANCE = 0.10f;
    static constexpr std::size_t MAX_ACTIVE_SUPPORTERS = 3;
    static constexpr std::size_t NEAR_SUPPORT_SLOT = 0;
    static constexpr std::size_t SQUARE_SUPPORT_SLOT = 1;
    static constexpr std::size_t TRAILING_SUPPORT_SLOT = 2;
    static constexpr float SUPPORT_SELECTION_CONTINUITY_METRES = 7.0f;
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
    static constexpr float MAX_SUPPORT_DISTANCE_METRES = 29.0f;
    static constexpr float SUPPORT_LONGITUDINAL_PULL = 0.20f;
    static constexpr float SUPPORT_LATERAL_PULL = 0.14f;
    static constexpr float POSSESSION_PROGRESS_START = 0.25f;
    static constexpr float POSSESSION_BLOCK_PROGRESS = 0.24f;
    static constexpr std::size_t MIN_COMMITTED_RUNNERS = 1;
    static constexpr std::size_t MAX_COMMITTED_RUNNERS = 3;
    static constexpr std::size_t MAX_STRIKER_RUNNERS = 1;
    static constexpr float SECOND_RUNNER_PROGRESS_THRESHOLD = 0.38f;
    static constexpr float SECOND_RUNNER_ATTACK_THRESHOLD = 0.80f;
    static constexpr float STRIKER_RUN_PRIORITY = 0.32f;
    static constexpr float WINGER_RUN_PRIORITY = 0.30f;
    static constexpr float ATTACKING_MIDFIELDER_RUN_PRIORITY = 0.30f;
    static constexpr float MIDFIELDER_RUN_PRIORITY = 0.26f;
    static constexpr float HOLDING_MIDFIELDER_RUN_PRIORITY = 0.08f;
    static constexpr float RUN_PACE_PRIORITY = 0.22f;
    static constexpr float RUN_DEPTH_PRIORITY = 0.12f;
    static constexpr float RUN_SEPARATION_PRIORITY = 0.10f;
    static constexpr float RUN_CONTINUITY_PRIORITY = 0.24f;
    /** Spread of the per-attack variation in who makes the runs. */
    static constexpr float RUN_VARIETY_PRIORITY = 0.15f;
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
    /** Late runs into the box in the final third (pitch fractions from the
     * goal line and from the centre, away from the ball's side). */
    static constexpr float MIDFIELD_ARRIVAL_DEPTH = 0.12f;
    static constexpr float MIDFIELD_ARRIVAL_WIDTH = 0.05f;
    static constexpr float FAR_POST_ARRIVAL_DEPTH = 0.07f;
    static constexpr float FAR_POST_ARRIVAL_WIDTH = 0.10f;
    /** The far-post run needs the ball out wide. */
    static constexpr float FAR_POST_MIN_BALL_WIDTH = 0.14f;
    static constexpr float FINAL_THIRD_FULLBACK_OVERLAP = 0.17f;
    static constexpr float FINAL_THIRD_SELECTION_CONTINUITY = 0.16f;
    // A presser jockeys goal-side 1.5-2.5 m off the carrier.
    static constexpr float PRESSING_STANDOFF_BASE = 0.014f;
    static constexpr float PRESSING_STANDOFF_CAUTIOUS_BONUS = 0.01f;
    static constexpr float PRESSER_CONTINUITY_SECONDS = 0.75f;
    static constexpr float COVER_PRESS_MINIMUM = 0.35f;
    static constexpr float COVER_LANE_INTERCEPTION_POINT = 0.52f;
    static constexpr float COVER_LANE_GOAL_SIDE_OFFSET = 0.018f;
    static constexpr float COVER_OUTLET_MAX_DISTANCE_METRES = 29.0f;
    static constexpr float COVER_FORWARD_OPTION_METRES = 7.0f;
    static constexpr float COVER_FALLBACK_DEPTH = 0.035f;
    static constexpr float COVER_FALLBACK_LATERAL_OFFSET = 0.055f;
    static constexpr float CHANNEL_WEIGHT = 1.4f;
    static constexpr float DANGER_DEPTH_WEIGHT = 0.12f;
    static constexpr float BASE_MARK_WEIGHT = 0.12f;
    static constexpr float COMPACTNESS_MARK_WEIGHT = 0.20f;
    static constexpr std::uint64_t MARK_REFRESH_STEPS = 20;
    static constexpr float TIGHT_MARK_DANGER_DEPTH = 0.62f;
    static constexpr float TIGHT_MARK_WEIGHT = 0.7f;
    static constexpr float GOAL_SIDE_MARK_METRES = 1.5f;
    static constexpr float SECOND_LOOSE_BALL_PRESS_THRESHOLD = 0.9f;
    static constexpr float ATTACKING_TRANSITION_SUPPORT_ADVANCE = 0.065f;
    static constexpr float ATTACKING_TRANSITION_RUN_ADVANCE = 0.13f;
    static constexpr float ATTACKING_TRANSITION_BALL_PULL = 0.12f;
    static constexpr float ATTACKING_TRANSITION_CARRIER_ADVANCE = 0.055f;
    static constexpr float IN_FLIGHT_SUPPORT_ADVANCE_PER_SECOND = 0.03f;
    static constexpr float IN_FLIGHT_SUPPORT_BALL_PULL_PER_SECOND = 0.25f;
    static constexpr float DEFENSIVE_TRANSITION_RECOVERY = 0.055f;
    static constexpr float DEFENSIVE_TRANSITION_BALL_COMPACTNESS = 0.16f;
    // Out-of-possession block (depths measured from the own goal line).
    static constexpr float BLOCK_LINE_BASE = -0.03f;
    static constexpr float BLOCK_LINE_BALL_FACTOR = 0.5f;
    static constexpr float BLOCK_LINE_PRESSING = 0.08f;
    static constexpr float BLOCK_LINE_MIN = 0.06f;
    static constexpr float BLOCK_LINE_MAX = 0.42f;
    static constexpr float BLOCK_SHORT_BALL_DEPTH = 0.2f;
    static constexpr float BLOCK_LONG_BALL_DEPTH = 0.7f;
    static constexpr float BLOCK_SHORT_LENGTH = 0.16f;
    static constexpr float BLOCK_LONG_LENGTH = 0.36f;
    static constexpr float BLOCK_COMPACTNESS_SQUEEZE = 0.2f;
    static constexpr float BLOCK_FORMATION_BACK = 0.18f;
    static constexpr float BLOCK_FORMATION_FRONT = 0.80f;
    static constexpr float BLOCK_WIDTH_SCALE = 0.72f;
    static constexpr float BLOCK_COMPACTNESS_NARROWING = 0.12f;
    static constexpr float BLOCK_BALL_SIDE_SHIFT = 0.35f;
  };

  struct Decision final
  {
    static constexpr float PRESSURE_RADIUS_METRES = 8.5f;
    static constexpr float BASE_SHOT_THRESHOLD = 0.056f;
    static constexpr float MIN_SHOT_XG = 0.005f;
    /** Time on the ball before a carrier re-decides while dribbling. */
    static constexpr float MIN_DRIBBLE_TIME = 0.5f;
    static constexpr float MAX_DRIBBLE_TIME = 1.1f;

    // Scored action selection: every candidate (best pass, shot, carry,
    // shield) is measured in a shared utility currency and the closest
    // choices are resolved by a vision-scaled random perturbation.
    static constexpr float SHOT_SCORE_SCALE = 26.0f;
    static constexpr float SHOT_BASE_INCLINATION = 1.5f;
    static constexpr float FINAL_THIRD_SHOT_BONUS = 0.3f;
    static constexpr float SHOT_ELIGIBILITY_FLOOR = 0.012f;
    static constexpr float SHOT_ELIGIBILITY_RANGE = 0.04f;
    static constexpr float SHOT_SKILL_BONUS = 0.28f;
    static constexpr float SHOT_PRESSURE_PENALTY = 0.02f;
    static constexpr float CARRY_OPENNESS_WEIGHT = 1.50f;
    static constexpr float CARRY_DRIBBLING_BONUS = 0.45f;
    static constexpr float CARRY_PRESSURE_PENALTY = 0.30f;
    static constexpr float CARRY_RISK_BIAS = 0.20f;
    static constexpr float CARRY_HOLD_PENALTY_PER_SECOND = 0.45f;
    static constexpr float SHIELD_PRESSURE_THRESHOLD = 0.60f;
    /** Carry appetite of a wide forward cutting inside (see Shape). */
    static constexpr float CUT_INSIDE_CARRY_BONUS = 0.8f;
    static constexpr float CUT_INSIDE_SHOT_BONUS = 0.6f;
    static constexpr float SHIELD_BONUS = 0.40f;
    static constexpr float SHIELD_DRIBBLING = 0.12f;
    static constexpr float VISION_NOISE_SCALE = 0.45f;
    static constexpr float VISION_DECISION_WEIGHT = 0.6f;
    /** Extra decision noise for a side with no tactical familiarity. */
    static constexpr float FAMILIARITY_NOISE_GAIN = 0.8f;
    static constexpr float LATE_GAME_MINUTE = 82.0f;
    static constexpr float LATE_LEAD_SPECULATIVE_PENALTY = 0.55f;
    static constexpr int COMFORTABLE_LEAD = 2;
    static constexpr float COMFORTABLE_LEAD_SHOT_PENALTY = 3.0f;
    static constexpr float CLEAR_CHANCE_XG = 0.30f;
    static constexpr float COMFORTABLE_LEAD_CARRY_PENALTY = 0.5f;
    static constexpr float WIDE_SHOT_WIDTH_DEVIATION = 0.32f;
    static constexpr float WIDE_SHOT_DISCOUNT = 0.12f;
    static constexpr float WIDE_RECYCLE_BONUS = 0.85f;
  };

  struct Passing final
  {
    static constexpr float MIN_DISTANCE_METRES = 2.5f;
    static constexpr float MAX_DISTANCE_METRES = 50.0f;
    static constexpr float IDEAL_DISTANCE_METRES = 16.0f;
    static constexpr float OPENNESS_RADIUS_METRES = 10.0f;
    static constexpr float OPENNESS_WEIGHT = 1.15f;
    static constexpr float LANE_RISK_WEIGHT = 1.45f;
    static constexpr float BASE_PROGRESS_WEIGHT = 1.4f;
    static constexpr float OFFENSIVE_PROGRESS_WEIGHT = 1.15f;
    /** Utility lost per metre away from the ideal pass length. */
    static constexpr float DISTANCE_PENALTY_PER_METRE = 0.0135f;
    static constexpr float SAFE_OUTLET_WEIGHT = 0.8f;
    static constexpr float FORWARD_ROLE_BONUS = 0.0f;
    static constexpr float MIN_ACCEPTABLE_OPTION_SCORE = -0.15f;
    static constexpr float PRESSURE_RELEASE_THRESHOLD = 0.55f;
    static constexpr float PRESSURE_RELEASE_MAX_PROGRESSION = 0.03f;
    static constexpr float PROGRESSIVE_PASS_MINIMUM = 0.035f;
    /** Space behind the opposing line (metres to their goal line) that makes
     * a ball over or through it attractive, and the range to full effect. */
    static constexpr float SPACE_BEHIND_MIN_METRES = 30.0f;
    static constexpr float SPACE_BEHIND_RANGE_METRES = 25.0f;
    static constexpr float SPACE_BEHIND_UTILITY = 1.2f;
    static constexpr float SPACE_BEHIND_COMPLETION_BONUS = 0.15f;
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
    static constexpr float CROSS_UTILITY_BONUS = 0.7f;
    static constexpr float CUTBACK_UTILITY_BONUS = 0.62f;
    static constexpr float CROSS_COMPLETION_PENALTY = 0.10f;
    static constexpr float CUTBACK_COMPLETION_BONUS = 0.07f;
    static constexpr float CROSS_TARGET_BLEND = 0.35f;
    static constexpr float BASE_COMPLETION_PROBABILITY = 0.58f;
    static constexpr float PASSING_COMPLETION_BONUS = 0.27f;
    static constexpr float OPENNESS_COMPLETION_BONUS = 0.15f;
    static constexpr float LANE_COMPLETION_PENALTY = 0.50f;
    static constexpr float DISTANCE_COMPLETION_PENALTY = 0.42f;
    static constexpr float LONG_PASS_REFERENCE_METRES = 45.0f;
    static constexpr float PRESSURE_COMPLETION_PENALTY = 0.14f;
    static constexpr float MIN_COMPLETION_PROBABILITY = 0.05f;
    static constexpr float MAX_COMPLETION_PROBABILITY = 0.98f;
    static constexpr float COMPLETION_UTILITY_WEIGHT = 1.5f;
    /** Per unit of risk-taking above neutral: weight gained by progress and
     * lost by the completion chance (below neutral the other way round). */
    static constexpr float RISK_PROGRESS_GAIN = 0.0f;
    static constexpr float RISK_SAFETY_GAIN = 0.0f;
    static constexpr float ACTIVE_RUNNER_UTILITY_BONUS = 0.16f;
    /** Utility per unit of the receiver's shooting chance (final third). */
    static constexpr float SHOT_CREATION_WEIGHT = 1.0f;
    static constexpr float THROUGH_BALL_FORWARD_LEAD = 0.055f;
    static constexpr float THROUGH_BALL_TARGET_BLEND = 0.60f;
    static constexpr float LOFTED_DISTANCE_METRES = 30.0f;
    static constexpr float PASS_PRESSURE_RADIUS_METRES = 7.5f;
    /** Average ball speed used to lead a moving receiver. */
    static constexpr float ESTIMATED_BALL_SPEED = 13.0f;
    static constexpr float RECEIVER_LEAD_SCALE = 0.45f;
    // Lateral execution error in metres (distance error per metre passed).
    static constexpr float TECHNICAL_ERROR_METRES = 3.6f;
    static constexpr float PRESSURE_ERROR_METRES = 1.2f;
    static constexpr float DISTANCE_ERROR = 0.012f;
    static constexpr float BACK_PASS_ERROR_SCALE = 0.35f;
    static constexpr float MAX_BACK_PASS_METRES = 28.0f;
    // Ground passes are struck so they arrive at a controllable pace.
    static constexpr float ARRIVAL_SPEED_BASE = 6.5f;
    static constexpr float ARRIVAL_SPEED_PASSING = 3.0f;
    static constexpr float ARRIVAL_SPEED_PER_METRE = 0.05f;
    static constexpr float MIN_GROUND_SPEED = 7.0f;
    static constexpr float MAX_GROUND_SPEED = 24.0f;
    // Lofted balls: horizontal launch speed grows with the distance.
    static constexpr float LOFTED_BASE_SPEED = 11.0f;
    static constexpr float LOFTED_SPEED_PER_METRE = 0.22f;
    static constexpr float MAX_LOFTED_SPEED = 24.0f;
    static constexpr float LOFTED_ARRIVAL_HEIGHT_METRES = 1.1f;
    static constexpr float DELIVERY_HEIGHT_SPREAD = 0.6f;
    static constexpr float OFFSIDE_PERCEPTION_ERROR = 0.06f;
    // Runners time their runs imperfectly: they sometimes drift beyond the
    // line, and a passer who misreads it releases an offside pass.
    static constexpr float RUN_TIMING_GAMBLE = 0.02f;
    static constexpr float OFFSIDE_TIMING_WINDOW = 0.03f;
    static constexpr float OFFSIDE_TIMING_CHANCE = 0.7f;
    static constexpr float RUN_TIMING_EPOCH_SECONDS = 5.0f;
    /** Heading rotation (rad/s) of a curled ground pass. */
    static constexpr float MAX_CURVE = 0.12f;
    static constexpr float CURVE_SKILL_BASE = 0.4f;
    static constexpr float MIN_ACTION_COOLDOWN = 0.8f;
    static constexpr float MAX_ACTION_COOLDOWN = 1.4f;
    /** The kicker cannot touch his own kick again for this long. */
    static constexpr float KICKER_LOCKOUT_SECONDS = 0.35f;
    static constexpr float LANE_START_MARGIN = 0.06f;
    static constexpr float LANE_END_MARGIN = 0.97f;
    static constexpr float BASE_INTERCEPTION_RADIUS_METRES = 2.2f;
    static constexpr float DEFENDING_INTERCEPTION_METRES = 1.7f;
    static constexpr float PACE_INTERCEPTION_METRES = 0.7f;
    static constexpr float BASE_INTERCEPTION_RISK = 0.55f;
    static constexpr float LATE_LANE_RISK = 0.35f;
    static constexpr float OFFSIDE_MARGIN = 0.004f;
  };

  struct Shooting final
  {
    static constexpr float PRESSURE_RADIUS_METRES = 4.0f;
    static constexpr float PRESSURE_PENALTY = 0.45f;
    // Logistic xG location model (Soccermatics fit on Wyscout PL 2017/18):
    // a central shot from 11 m is worth ~0.18, from 20 m ~0.06.
    static constexpr float XG_INTERCEPT = 0.5103f;
    static constexpr float XG_ANGLE = 0.6338f;
    static constexpr float XG_DISTANCE = -0.2798f;
    static constexpr float XG_LINE = 0.1243f;
    static constexpr float XG_LATERAL = -0.0300f;
    static constexpr float XG_LINE_SQUARED = 0.0014f;
    static constexpr float XG_LATERAL_SQUARED = 0.0041f;
    static constexpr float XG_ANGLE_LINE = -0.1251f;
    static constexpr float XG_MAX_DISTANCE_METRES = 38.0f;
    static constexpr float BASE_SKILL_FACTOR = 0.62f;
    static constexpr float SHOOTING_SKILL_FACTOR = 0.45f;
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
    static constexpr float ERROR_BASE_METRES = 3.45f;
    static constexpr float ERROR_SKILL_METRES = 2.4f;
    static constexpr float ERROR_PRESSURE_METRES = 1.8f;
    static constexpr float ERROR_REFERENCE_METRES = 16.0f;
    static constexpr float ERROR_MIN_DISTANCE_SCALE = 0.55f;
    static constexpr float PENALTY_ERROR_SCALE = 0.16f;
    static constexpr float VERTICAL_ERROR_SHARE = 0.6f;
    static constexpr float MIN_CROSSING_HEIGHT_METRES = 0.12f;
    static constexpr float MAX_CROSSING_HEIGHT_METRES = 6.0f;
    // Struck shots leave the foot at 21-33 m/s (lab instep kicks ~28 m/s).
    static constexpr float BASE_BALL_SPEED = 21.0f;
    static constexpr float SHOOTING_SPEED_BONUS = 9.0f;
    static constexpr float POWER_SPEED_BONUS = 3.0f;
    static constexpr float MIN_SPEED_VARIATION = 0.88f;
    static constexpr float RELEASE_HEIGHT_METRES = 0.2f;
    static constexpr float WOODWORK_BAND_METRES = 0.10f;
    static constexpr float WOODWORK_REBOUND = 0.35f;
    static constexpr float MAX_SET_PIECE_XG = 0.82f;
    static constexpr float MIN_GOAL_PROBABILITY = 0.01f;
    static constexpr float MIN_ACTION_COOLDOWN = 1.5f;
    static constexpr float MAX_ACTION_COOLDOWN = 2.5f;
  };

  struct Defending final
  {
    /** Reach of a standing challenge and of a sliding one (to the ball). */
    static constexpr float TACKLE_DISTANCE_METRES = 1.3f;
    static constexpr float TACKLE_DEFENDING_REACH_METRES = 0.3f;
    static constexpr float SLIDE_TACKLE_DISTANCE_METRES = 2.2f;
    /** Engagement waits for the ball to be away from the attacker's foot. */
    static constexpr float CLOSE_CONTROL_ENGAGE_SHARE = 0.25f;
    static constexpr float EXPOSED_ENGAGE_SHARE = 1.75f;
    static constexpr float EXPOSURE_WIN_BONUS = 0.35f;
    static constexpr float SHIELD_PHYSICALITY_PENALTY = 0.15f;
    static constexpr float SLIDE_WIN_PENALTY = 0.05f;
    static constexpr float FROM_BEHIND_WIN_PENALTY = 0.10f;
    static constexpr float FROM_BEHIND_FOUL_FACTOR = 2.2f;
    static constexpr float SLIDE_FOUL_FACTOR = 1.6f;
    static constexpr float EXPOSED_FOUL_RELIEF = 0.6f;
    static constexpr float ENGAGE_RATE_PER_SECOND = 0.35f;
    static constexpr float PRESSING_ENGAGE_BONUS = 0.3f;
    static constexpr float FINAL_THIRD_ENGAGE_FACTOR = 2.5f;
    static constexpr float MIN_TACKLE_COOLDOWN = 1.4f;
    static constexpr float MAX_TACKLE_COOLDOWN = 3.2f;
    static constexpr float PRESSING_COOLDOWN_REDUCTION = 0.25f;
    static constexpr float TACKLE_COOLDOWN_BASE_MULTIPLIER = 1.1f;
    static constexpr float BASE_WIN_CHANCE = 0.12f;
    static constexpr float DEFENDING_WIN_BONUS = 0.42f;
    static constexpr float DRIBBLING_WIN_PENALTY = 0.27f;
    // Pressing wins more challenges by making more of them, not better ones:
    // rushed challenges win slightly less often and foul more.
    static constexpr float PRESSING_WIN_EFFECT = -0.07f;
    static constexpr float PRESSING_FOUL_BONUS = 0.06f;
    static constexpr float MIN_WIN_CHANCE = 0.08f;
    static constexpr float MAX_WIN_CHANCE = 0.58f;
    static constexpr float PHYSICALITY_DUEL_WEIGHT = 0.12f;
    static constexpr float BASE_FOUL_CHANCE = 0.24f;
    static constexpr float RISK_FOUL_BONUS = 0.10f;
    static constexpr float TECHNIQUE_FOUL_BONUS = 0.10f;
    static constexpr float WINNING_TACKLE_FOUL_SHARE = 0.25f;
    static constexpr float PENALTY_AREA_FOUL_SCALE = 0.13f;
    // A won challenge often only pokes the ball loose.
    static constexpr float POKE_LOOSE_CHANCE = 0.45f;
    static constexpr float MIN_POKE_SPEED = 3.5f;
    static constexpr float MAX_POKE_SPEED = 8.0f;
    // Defenders pressed in their own third with no safe pass clear it.
    static constexpr float CLEARANCE_MAX_DEPTH = 0.33f;
    static constexpr float CLEARANCE_MIN_DISTANCE_METRES = 30.0f;
    static constexpr float CLEARANCE_MAX_DISTANCE_METRES = 50.0f;
    static constexpr float CLEARANCE_SPEED = 22.0f;
    static constexpr float CLEARANCE_TOUCHLINE_BIAS = 0.35f;
    static constexpr float MIN_RECOVERY_COOLDOWN = 0.6f;
    static constexpr float MAX_RECOVERY_COOLDOWN = 1.2f;
    /** Reach of an outfield player throwing his body at a shot. */
    static constexpr float BLOCK_DISTANCE_METRES = 1.6f;
    static constexpr float BASE_BLOCK_CHANCE = 0.55f;
    static constexpr float DEFENDING_BLOCK_BONUS = 0.25f;
    static constexpr float DEFLECTION_SPEED_FACTOR = -0.22f;
    static constexpr float MAX_DEFLECTION_Y_SPEED = 5.0f;
  };

  /**
   * Ball physics in SI units: quadratic air drag with a drag crisis, gravity,
   * a restitution bounce, rolling resistance on the grass and curve as a
   * heading rotation (a Magnus-effect stand-in).
   */
  struct Ball final
  {
    static constexpr float GRAVITY = 9.81f;
    static constexpr float AIR_DENSITY = 1.225f;
    static constexpr float MASS_KG = 0.43f;
    static constexpr float CROSS_SECTION_M2 = 0.038f;
    static constexpr float DRAG_SUBCRITICAL = 0.45f;
    static constexpr float DRAG_SUPERCRITICAL = 0.24f;
    static constexpr float DRAG_CRISIS_LOW_SPEED = 8.0f;
    static constexpr float DRAG_CRISIS_HIGH_SPEED = 14.0f;
    static constexpr float BOUNCE_RESTITUTION = 0.62f;
    static constexpr float BOUNCE_TANGENTIAL_RETAINED = 0.84f;
    static constexpr float MIN_BOUNCE_SPEED = 1.0f;
    static constexpr float ROLLING_DECELERATION = 0.75f;
    static constexpr float ROLLING_VISCOUS_PER_SECOND = 0.05f;
    static constexpr float CURVE_DECAY_PER_SECOND = 0.35f;
    static constexpr float STOP_SPEED = 0.2f;
    /** A spent shot slower than this is a loose ball again. */
    static constexpr float DEAD_SHOT_SPEED = 6.0f;
    static constexpr float MAX_FLIGHT_SECONDS = 6.0f;
    // Reach for first touches: feet for a low ball, body up to the chest.
    static constexpr float GOALKEEPER_CONTROL_RADIUS_METRES = 1.8f;
    static constexpr float OUTFIELD_CONTROL_RADIUS_METRES = 1.0f;
    static constexpr float CHEST_CONTROL_RADIUS_METRES = 0.7f;
    static constexpr float LOW_BALL_METRES = 0.5f;
    static constexpr float BASE_CONTROL_CHANCE = 0.74f;
    static constexpr float TOUCH_SKILL_BONUS = 0.45f;
    static constexpr float SPEED_CONTROL_PENALTY = 0.0065f;
    static constexpr float MIN_CONTROL_CHANCE = 0.28f;
    static constexpr float MAX_CONTROL_CHANCE = 0.97f;
    // An opponent in the lane only gets a foot to it some of the time: it
    // depends on how far he has to stretch and how fast the ball is.
    static constexpr float INTERCEPT_BASE_CHANCE = 0.62f;
    static constexpr float INTERCEPT_DEFENDING_BONUS = 0.35f;
    static constexpr float INTERCEPT_SPEED_PENALTY = 0.018f;
    static constexpr float INTERCEPT_MIN_CHANCE = 0.08f;
    static constexpr float FAILED_TRAP_TIME = 0.55f;
    static constexpr float FAILED_TOUCH_LOCKOUT = 0.3f;
    static constexpr float HEAVY_TOUCH_RETAINED = 0.25f;
    static constexpr float MIN_HEAVY_TOUCH_SPEED = 1.5f;
    static constexpr float MAX_HEAVY_TOUCH_SPEED = 4.5f;
    static constexpr float BASE_TRAP_TIME = 0.25f;
    static constexpr float TRAP_SKILL_PENALTY = 0.35f;
    static constexpr float MIN_POST_TOUCH_DELAY = 0.5f;
    static constexpr float MAX_POST_TOUCH_DELAY = 0.9f;
    static constexpr float PASSING_TOUCH_WEIGHT = 0.85f;
    static constexpr float KEEPER_FEET_WEIGHT = 0.7f;
    static constexpr float SAVE_DISTANCE_METRES = 1.8f;
    static constexpr float SAVE_DIVE_TIME = 1.2f;
    /** A keeper holding the ball waits for his team to push up. */
    static constexpr float MIN_KEEPER_HOLD_SECONDS = 2.5f;
    static constexpr float MAX_KEEPER_HOLD_SECONDS = 5.0f;
    static constexpr float GOAL_NET_BALL_DEPTH = 0.02f;
  };

  struct Rules final
  {
    static constexpr int MAX_SUBSTITUTIONS_PER_TEAM = 5;
    /** One more change (and window) once a tie goes to extra time. */
    static constexpr int EXTRA_TIME_SUBSTITUTIONS = 1;
    static constexpr long MINIMUM_PLAYERS = 7;
    static constexpr float PARKED_PLAYER_OFFSET = 0.06f;
    static constexpr float PARKED_PLAYER_SPACING = 0.025f;
    static constexpr float DEFENSIVE_SLOT_DEPTH = 0.30f;
    static constexpr float SHORT_HANDED_DROP = 0.02f;
    // Crowd-driven home advantage on execution; referee bias is modelled in
    // Discipline (card biases).
    static constexpr float HOME_EXECUTION_BONUS = 0.15f;
    /** Home crowd lift on technical and mental attributes (stretched
     * units), worth a few rating points. */
    static constexpr float HOME_ATTRIBUTE_BONUS = 0.19f;
    /** Execution, decision and duel edge per player of numerical advantage
     * (a side reduced to ten is stretched and loses confidence). */
    static constexpr float NUMERICAL_EDGE_PER_PLAYER = 0.20f;
    static constexpr int MAX_NUMERICAL_EDGE_PLAYERS = 3;
    /** Weight of the team edge on the attributes used in duels. */
    static constexpr float DUEL_EDGE_WEIGHT = 0.8f;
    static constexpr float HOME_FINAL_THIRD_START = 0.67f;
    static constexpr float AWAY_FINAL_THIRD_START = 0.33f;
  };

  /** Goalkeeper model in real seconds, metres and m/s. */
  struct Goalkeeper final
  {
    // A set keeper reads the striker's body shape, so his effective
    // reaction to a struck ball is shorter than a raw 0.21 s reaction time.
    static constexpr float REACTION_BASE_SECONDS = 0.12f;
    static constexpr float REACTION_SKILL_SECONDS = 0.08f;
    static constexpr float SCREENED_REACTION_SECONDS = 0.12f;
    static constexpr float SCREEN_WIDTH_METRES = 0.9f;
    static constexpr float READ_ERROR_METRES = 0.8f;
    static constexpr float DIVE_POST_MARGIN = 0.02f;
    static constexpr float DIVE_ACCELERATION_METRES = 14.0f;
    static constexpr float DIVE_BASE_SPEED_METRES = 4.2f;
    static constexpr float DIVE_SKILL_SPEED_METRES = 1.6f;
    static constexpr float BODY_REACH_METRES = 2.0f;
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
    static constexpr float PARRY_MAX_VERTICAL_SPEED = 4.0f;
    static constexpr float RECOVER_TIME_SECONDS = 1.1f;
    static constexpr float DISTRIBUTE_TIME_SECONDS = 0.6f;
    static constexpr float PENALTY_READ_BASE = 0.0f;
    static constexpr float PENALTY_READ_SKILL = 0.12f;
    static constexpr float PENALTY_STAY_CHANCE = 0.08f;
    static constexpr float PENALTY_DIVE_METRES = 1.2f;
    /** Even a keeper who guesses right rarely reaches a placed penalty. */
    static constexpr float PENALTY_SAVE_FACTOR = 0.30f;
    static constexpr float CLAIM_LOOKAHEAD_SECONDS = 0.95f;
    static constexpr float CLAIM_BOX_DEPTH_METRES = 6.0f;
    static constexpr float SWEEP_ADVANTAGE = 1.25f;
    static constexpr float RUSH_COVER_DISTANCE_METRES = 5.0f;
    static constexpr float RUSH_CLOSING_SHARE = 0.55f;
    static constexpr float NEAR_DEPTH_METRES = 0.8f;
    static constexpr float DEPTH_PER_METRE = 0.11f;
    static constexpr float SWEEPER_DISTANCE_METRES = 35.0f;
    static constexpr float SWEEPER_DEPTH_PER_METRE = 0.18f;
    static constexpr float MAX_DEPTH_METRES = 16.0f;
  };

  struct SetPiece final
  {
    static constexpr float CROSSING_FREE_KICK_METRES = 45.0f;
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
    /** The wall hides the strike: the keeper reacts this much later. */
    static constexpr float WALL_SCREEN_SECONDS = 0.55f;
    /** A dead ball is struck with more precision and aimed higher and
     * tighter to the post than a shot on the move. */
    static constexpr float FREE_KICK_ERROR_SCALE = 0.5f;
    static constexpr float FREE_KICK_AIM_INSET_SPREAD = 0.6f;
    static constexpr float FREE_KICK_AIM_MIN_HEIGHT_METRES = 1.2f;
    static constexpr float SHORT_CORNER_CHANCE = 0.12f;
    static constexpr float SHORT_CORNER_MAX_DISTANCE_METRES = 14.0f;
    static constexpr float NEAR_POST_CHANCE = 0.42f;
    static constexpr float TARGET_RANDOMNESS = 0.6f;
    /** Weight of a box attacker's heading reach when choosing the target. */
    static constexpr float TARGET_THREAT_WEIGHT = 0.0f;
    static constexpr std::size_t REST_DEFENDERS = 3;
    static constexpr float MARKING_GOAL_SIDE_OFFSET = 0.008f;
    static constexpr float PENALTY_WAIT_OFFSET = 0.01f;
    static constexpr float SET_PIECE_PHASE_SECONDS = 20.0f;
    // Defensive clearances and blocks near the own goal line often go out
    // for a corner.
    static constexpr float CLEARANCE_BEHIND_CHANCE = 0.6f;
    /** A defender heading a delivery away near his goal line. */
    static constexpr float HEADER_BEHIND_CHANCE = 0.33f;
    /** Chance that a defender's header or touch of a delivery in front of
     * his goal skews toward it (own goals), and its speed and lift. */
    static constexpr float OWN_GOAL_TOUCH_CHANCE = 0.04f;
    static constexpr float OWN_GOAL_TOUCH_DEPTH_METRES = 9.0f;
    static constexpr float OWN_GOAL_TOUCH_WIDTH_METRES = 9.0f;
    static constexpr float OWN_GOAL_MIN_SPEED = 7.0f;
    static constexpr float OWN_GOAL_MAX_SPEED = 13.0f;
    static constexpr float OWN_GOAL_MAX_LIFT = 2.0f;
    static constexpr float BLOCK_BEHIND_CHANCE = 0.6f;
    static constexpr float BLOCK_BEHIND_DEPTH = 0.20f;
    static constexpr float CROSS_CLEARANCE_DEPTH = 0.25f;
  };

  /**
   * Unit conventions. Pitch x is normalised by LENGTH_METRES and y by
   * WIDTH_METRES. The public ball height `MatchBall::z` stays in length units
   * (z * BALL_Z_METRES = metres) for renderers; velocities are in m/s.
   */
  struct Units final
  {
    static constexpr float BALL_Z_METRES = 105.0f;
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
    /** Bounds of a referee drawn around a league's own mean strictness. */
    static constexpr float MIN_CONTEXT_STRICTNESS = 0.3f;
    static constexpr float MAX_CONTEXT_STRICTNESS = 2.8f;
    static constexpr float RECKLESS_SHARE = 0.17f;
    static constexpr float CARELESS_YELLOW_CHANCE = 0.024f;
    static constexpr float RECKLESS_YELLOW_CHANCE = 0.235f;
    static constexpr float TACTICAL_YELLOW_CHANCE = 0.24f;
    static constexpr float SERIOUS_FOUL_PLAY_SHARE = 0.0015f;
    static constexpr float DOGSO_RED_CHANCE = 0.38f;
    static constexpr float PENALTY_AREA_DOGSO_RED_CHANCE = 0.18f;
    static constexpr float AWAY_CARD_BIAS = 1.07f;
    static constexpr float HOME_CARD_BIAS = 0.94f;
    static constexpr float BOOKED_PLAYER_CAUTION = 0.25f;
    static constexpr float ADVANTAGE_WINDOW_SECONDS = 3.0f;
    static constexpr float ADVANTAGE_MIN_OPENNESS = 0.45f;
    static constexpr float ADVANTAGE_PLAY_CHANCE = 0.45f;
    static constexpr float DOGSO_MAX_DISTANCE_METRES = 28.0f;
    static constexpr float DOGSO_MAX_WIDTH_DEVIATION = 0.22f;
  };

  struct Injury final
  {
    // Non-contact hazard per match minute for a fresh player at moderate
    // intensity; low condition and sprinting raise it.
    static constexpr float BASE_HAZARD_PER_MINUTE = 0.00009f;
    static constexpr float FATIGUE_HAZARD_MULTIPLIER = 3.0f;
    static constexpr float INTENSITY_HAZARD_MULTIPLIER = 1.5f;
    static constexpr float CONTACT_INJURY_CHANCE = 0.006f;
    static constexpr float RECKLESS_CONTACT_INJURY_CHANCE = 0.03f;
    static constexpr float INJURED_SPEED_SCALE = 0.55f;
    static constexpr float CHECK_INTERVAL_MINUTES = 1.0f;
  };

  struct Stoppage final
  {
    static constexpr float FIRST_HALF_BASE_MINUTES = 1.5f;
    static constexpr float SECOND_HALF_BASE_MINUTES = 2.5f;
    static constexpr float EXTRA_TIME_BASE_MINUTES = 0.5f;
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
    static constexpr float EARLIEST_TACTICAL_MINUTE = 68.0f;
    static constexpr float FATIGUE_THRESHOLD = 0.63f;
    static constexpr float FATIGUE_THRESHOLD_LATE_GAIN = 0.12f;
    static constexpr float LATE_GAME_MINUTE = 80.0f;
    static constexpr float CARD_RISK_MINUTE = 64.0f;
    static constexpr float CARD_RISK_NEED = 0.22f;
    static constexpr float TRAILING_CHASE_MINUTE = 66.0f;
    static constexpr float LEADING_PROTECT_MINUTE = 75.0f;
    static constexpr float TACTICAL_NEED = 0.18f;
    static constexpr float MINIMUM_NEED = 0.22f;
    /** Share of the minimum need that suffices in the last minutes. */
    static constexpr float LATE_NEED_SHARE = 0.4f;
    /** Share of the minimum need that joins a change already being made. */
    static constexpr float WINDOW_NEED_SHARE = 0.9f;
    static constexpr float MINUTE_NEED_GAIN = 0.012f;
    static constexpr float FATIGUE_NEED_SCALE = 2.2f;
    /** Forwards lose their sharpness first and are replaced most often. */
    static constexpr float ATTACKER_FATIGUE_NEED_FACTOR = 1.6f;
    /** From this minute a tiring forward is also freshened up tactically
     * (need per minute, scaled by the condition he has lost). */
    static constexpr float ATTACKER_ROTATION_MINUTE = 64.0f;
    static constexpr float ATTACKER_ROTATION_NEED_PER_MINUTE = 0.045f;
    static constexpr float ROLE_FIT_BONUS = 0.5f;
  };

  /**
   * Two-pool energy model per real second. The slow pool (`stamina`, the
   * match condition) drains with running intensity and caps top speed late
   * in the game; the fast pool (repeat-sprint reserve) empties during
   * high-intensity bursts and refills within a couple of minutes.
   */
  struct Fatigue final
  {
    static constexpr float IDLE_DRAIN = 0.000030f;
    static constexpr float RUNNING_DRAIN = 0.00080f;
    static constexpr float SPRINT_THRESHOLD = 0.78f;
    static constexpr float SPRINT_DRAIN = 0.00030f;
    static constexpr float PRESSING_DRAIN = 0.00015f;
    /** Extra drain per second for the whole side out of possession, per unit
     * of pressing above the neutral setting. */
    static constexpr float TEAM_PRESSING_NEUTRAL = 0.5f;
    static constexpr float TEAM_PRESSING_DRAIN = 0.00040f;
    static constexpr float ENDURANCE_BASE = 1.40f;
    static constexpr float ENDURANCE_RELIEF = 0.80f;
    static constexpr float HALF_TIME_RECOVERY = 0.06f;
    static constexpr float EXTRA_TIME_BREAK_RECOVERY = 0.03f;
    static constexpr float EXTRA_TIME_HALF_TIME_RECOVERY = 0.01f;
    static constexpr float TECHNIQUE_ERROR_GAIN = 0.35f;
    /** Reserve drained per second at full sprint (empties in ~25 s). */
    static constexpr float RESERVE_DRAIN_PER_SECOND = 0.04f;
    static constexpr float RESERVE_RECOVERY_SECONDS = 90.0f;
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
    static constexpr float DUEL_RADIUS_METRES = 1.1f;
    static constexpr float GOALKEEPER_CLAIM_RADIUS_METRES = 2.5f;
    static constexpr float BASE_CLAIM_CHANCE = 0.50f;
    static constexpr float CROWDING_PENALTY = 0.08f;
    static constexpr float SKILL_CLAIM_BONUS = 0.24f;
    static constexpr float DUEL_FOUL_CHANCE = 0.08f;
    /** Defenders facing a delivery win first contact more often. */
    static constexpr float DEFENDER_DUEL_ADVANTAGE = 1.3f;
    static constexpr float HEADER_SHOT_MIN_DEPTH = 0.84f;
    /** Team-mates' lofted balls are cushioned down below this depth. */
    static constexpr float HEADER_KNOCK_DOWN_MIN_DEPTH = 0.72f;
    static constexpr float KNOCK_DOWN_MAX_BACKWARD_METRES = 4.0f;
    static constexpr float HEADER_SHOT_WIDTH = 0.18f;
    static constexpr float HEADER_SHOT_MIN_XG = 0.06f;
    static constexpr float HEADER_CLEARANCE_SPEED = 17.0f;
    /** Weakest defensive header as a share of the clearance speed. */
    static constexpr float MIN_CLEARANCE_SHARE = 0.45f;
    static constexpr float HEADER_PASS_SPEED = 10.0f;
    static constexpr float HEADER_LIFT = 5.0f;
    static constexpr float HEADER_ACCURACY_PENALTY = 0.96f;
    /** Share of physicality (against shooting) in heading precision. */
    static constexpr float HEADING_PHYSICAL_SHARE = 0.3f;
    static constexpr float HEADER_SPEED_SCALE = 0.7f;
    static constexpr float ARRIVAL_HEIGHT_METRES = 2.1f;
  };

  /**
   * Touchline instructions: a shout nudges the team sliders (and shot
   * appetite) and fades linearly; a team talk scales execution, decisions and
   * work rate by at most a few per cent for one half.
   */
  struct Touchline final
  {
    static constexpr float SHOUT_DURATION_SECONDS = 600.0f;
    static constexpr float SHOUT_SLIDER_STEP = 0.2f;
    static constexpr float SHOUT_SHOT_BIAS = 0.6f;
    static constexpr float ENCOURAGE_WORK_RATE = 0.03f;
    static constexpr float MAX_TEAM_TALK_MODIFIER = 0.05f;
    // Score effects on the sliders per goal of lead (at most two), from
    // SCORE_EFFECT_BASE of full strength at kick-off to full at 90 minutes.
    static constexpr int SCORE_EFFECT_MAX_GOALS = 3;
    static constexpr float SCORE_EFFECT_BASE = 0.4f;
    static constexpr float SCORE_EFFECT_OFFENSIVE = 0.10f;
    static constexpr float SCORE_EFFECT_RISK = 0.10f;
    static constexpr float SCORE_EFFECT_PRESSING = 0.08f;
    static constexpr float SCORE_EFFECT_COMPACTNESS = 0.08f;
    /** A side two or more goals up also stops committing men forward (per
     * goal of lead from Decision::COMFORTABLE_LEAD, at full urgency). */
    static constexpr float GAME_MANAGEMENT_OFFENSIVE = 0.12f;
    // A clearly weaker side sits deeper and more compact and commits fewer
    // men forward, from UNDERDOG_GAP_START of mean (stretched) outfield
    // quality below the opponent to full effect UNDERDOG_GAP_RANGE later.
    static constexpr float UNDERDOG_GAP_START = 0.08f;
    static constexpr float UNDERDOG_GAP_RANGE = 0.30f;
    static constexpr float UNDERDOG_PRESSING = 0.35f;
    static constexpr float UNDERDOG_COMPACTNESS = 0.40f;
    static constexpr float UNDERDOG_OFFENSIVE = 0.30f;
    static constexpr float UNDERDOG_RISK = 0.25f;
    // AI managers react to the score late in the game.
    static constexpr float AI_CHASE_MINUTE = 65.0f;
    static constexpr float AI_SHOOT_ON_SIGHT_MINUTE = 82.0f;
    static constexpr float AI_PROTECT_MINUTE = 75.0f;
  };

  /**
   * External control of one player (play mode). The controlled player's body
   * is integrated at a finer fixed rate than the AI step so a stick feels
   * responsive; everything else keeps the common step.
   */
  struct Control final
  {
    /** Kinematic sub-steps per fixed step for the controlled player (50 Hz). */
    static constexpr int PHYSICS_SUBSTEPS = 5;
    /** Run speed without the sprint button, as a share of top speed. */
    static constexpr float JOG_SPEED_SHARE = 0.62f;
    /** How long a pressed action waits for a chance to be performed. */
    static constexpr float ACTION_BUFFER_SECONDS = 1.0f;
    /** A pass goes to the team-mate best aligned with the aim within this
     * cosine (60 degrees) ... */
    static constexpr float PASS_MIN_ALIGNMENT = 0.5f;
    /** ... preferring nearer team-mates by this much per metre. */
    static constexpr float PASS_DISTANCE_WEIGHT = 0.006f;
  };

  /** Accepted ranges of MatchContext (league character). */
  struct Context final
  {
    static constexpr float MIN_GOAL_RATE_SCALE = 0.5f;
    static constexpr float MAX_GOAL_RATE_SCALE = 1.5f;
    /** Shot precision grows as goalRateScale^FINISHING_EXPONENT. */
    static constexpr float FINISHING_EXPONENT = 1.7f;
    static constexpr float MIN_REFEREE_STRICTNESS = 0.5f;
    static constexpr float MAX_REFEREE_STRICTNESS = 2.0f;
    static constexpr float MAX_REFEREE_SD = 0.5f;
    static constexpr float MAX_HOME_ADVANTAGE_SCALE = 2.0f;
  };

  struct Statistics final
  {
    static constexpr float EVEN_POSSESSION_PERCENT = 50.0f;
    static constexpr float PERCENT_SCALE = 100.0f;
  };
};
