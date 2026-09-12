// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "global/types.h"

class Lineup;
class Player;
class Strategy;

/**
 * @brief Position group of a formation slot; it decides which roles the slot
 * can be given (see Tactics::familyForSlot).
 */
enum class RoleFamily : std::uint8_t
{
  Goalkeeper,
  FullBack,
  CentreBack,
  Holding,
  Central,
  Attacking,
  Wide,
  Striker,
  COUNT
};

/**
 * @brief How a player interprets his slot (values are persisted in the
 * tactic). Standard plays the position as drilled, with no extra bias.
 */
enum class TacticalRole : std::uint8_t
{
  Standard = 0,
  LineKeeper,      /*!< Stays on his line. */
  SweeperKeeper,   /*!< Starts high and comes out for balls in behind. */
  InsideFullBack,  /*!< Steps into midfield when his side has the ball. */
  Stopper,         /*!< Centre-back who steps out to meet his man. */
  CoverDefender,   /*!< Centre-back who drops off and sweeps behind. */
  Anchor,          /*!< Holds in front of the back line. */
  Playmaker,       /*!< Dictates with riskier, forward passes. */
  BoxToBox,        /*!< Covers both boxes, joins attacks late. */
  SecondStriker,   /*!< Runs beyond the striker from deep. */
  InsideForward,   /*!< Wide forward who cuts in to shoot. */
  TouchlineWinger, /*!< Hugs the line to beat his man and cross. */
  TargetForward,   /*!< Reference point for forward passes. */
  Poacher,         /*!< Lives on the last line, gambles in the box. */
  PressingForward, /*!< First defender, hunts the ball. */
  FalseNine,       /*!< Drops off the line to link play. */
  COUNT
};

/** @brief Attacking commitment of a role (persisted in the tactic). */
enum class RoleDuty : std::uint8_t
{
  Defend,
  Support,
  Attack,
  COUNT
};

/**
 * @brief Individual instruction against one opposing player (values are
 * persisted in OppositionInstructions; append new ones before COUNT).
 */
enum class OppositionInstruction : std::uint8_t
{
  None = 0,
  TightMark,    /*!< Stay tight to him; his marker follows him everywhere. */
  Press,        /*!< Close him down as soon as he receives. */
  ShowWeakFoot, /*!< Force him onto his weaker foot. */
  DoubleUp,     /*!< Two players close him down (wide threats). */
  COUNT
};

/**
 * @brief Measurable behaviour of a role and duty in the match engine.
 * Distances are metres, biases are engine utility units; zero everywhere is
 * the Standard role on a Support duty.
 */
struct RoleProfile
{
  /** Extra depth toward the opponents' goal while his side has the ball. */
  float possessionAdvanceMetres = 0.0f;
  /** Toward his touchline (+) or the middle (-) while his side has it. */
  float possessionWidthMetres = 0.0f;
  /** Higher (+) or deeper (-) in the defensive block. */
  float defensiveAdvanceMetres = 0.0f;
  /** How keen he is to make runs in behind and into the box. */
  float runBias = 0.0f;
  /** Full-backs: how keen to overlap; -1 or less never overlaps. */
  float overlapBias = 0.0f;
  /** Appetite for forward passes over safe ones. */
  float passDaring = 0.0f;
  /** How much team-mates look for him with forward passes. */
  float targetBias = 0.0f;
  /** Eagerness to close down the ball (pressing trigger). */
  float pressBias = 0.0f;
  /** Willingness to shoot. */
  float shotBias = 0.0f;
  /** Wide players: +1 cuts inside, -1 keeps to the line, 0 mixes. */
  float cutInside = 0.0f;
  /** Goalkeepers: set position further from (+) or nearer (-) his goal. */
  float keeperDepthMetres = 0.0f;
  /** Goalkeepers: readiness to sweep balls in behind (+) or not (-). */
  float keeperSweep = 0.0f;
};

/**
 * @brief Role, duty and in-possession spot of one outfield slot. Slots are
 * matched by their out-of-possession position (`anchor`, lineup coordinates:
 * own goal at x = 0), so they survive reordering of the lineup and return
 * when a formation is used again.
 */
struct SlotInstruction
{
  Vector2F anchor{0.0f, 0.0f};
  TacticalRole role = TacticalRole::Standard;
  RoleDuty duty = RoleDuty::Support;
  /** Shift of the slot while his side has the ball (lineup coordinates). */
  Vector2F possessionOffset{0.0f, 0.0f};
};

/** @brief An instruction against an opposing player for the next match. */
struct PlayerInstruction
{
  PlayerID player = 0;
  OppositionInstruction instruction = OppositionInstruction::None;
};

/** @brief Ready-made in-possession shapes offered on the tactics screen. */
enum class PossessionShape : std::uint8_t
{
  KeepShape,      /*!< Same shape with and without the ball. */
  FullBacksPush,  /*!< Both full-backs join the midfield line. */
  BuildWithThree, /*!< One full-back tucks in, the other pushes on. */
  NarrowFront,    /*!< Wide players come inside, full-backs give width. */
  COUNT
};

/** Engine weights of the role biases and the instructions. */
namespace TacticsTuning
{
/** Slots further than this from every stored anchor play Standard. */
inline constexpr float SLOT_MATCH_DISTANCE = 0.06f;
inline constexpr float RUN_BIAS_WEIGHT = 0.22f;
inline constexpr float OVERLAP_BIAS_WEIGHT = 0.25f;
inline constexpr float PRESS_BIAS_SECONDS = 0.6f;
inline constexpr float PASS_DARING_PROGRESS_WEIGHT = 3.0f;
inline constexpr float PASS_DARING_SAFETY_WEIGHT = 1.0f;
inline constexpr float TARGET_BIAS_WEIGHT = 1.0f;
inline constexpr float SHOT_BIAS_WEIGHT = 2.0f;
inline constexpr float CUT_INSIDE_SHIFT = 0.9f;
inline constexpr float KEEPER_SWEEP_DEPTH_GAIN = 0.5f;
inline constexpr float KEEPER_SWEEP_ADVANTAGE_GAIN = 0.15f;
/** Engagement rate gained per unit of press bias. */
inline constexpr float PRESS_BIAS_ENGAGE = 0.35f;

/** Tight marking: share of the way to the man and goal-side metres. */
inline constexpr float TIGHT_MARK_WEIGHT = 0.88f;
inline constexpr float TIGHT_MARK_GOAL_SIDE_METRES = 1.2f;
/** Completion lost by a pass to a man whose tight marker is this close. */
inline constexpr float TIGHT_MARK_REACH_METRES = 3.0f;
inline constexpr float TIGHT_MARK_COMPLETION_PENALTY = 0.10f;
/** Pressing order: stand-off share kept and engagement gained. */
inline constexpr float PRESS_ORDER_STANDOFF_SHARE = 0.45f;
inline constexpr float PRESS_ORDER_ENGAGE = 0.8f;
/** Weaker foot: presser shade (metres), execution error and shot penalty
 * at full pressure. */
inline constexpr float WEAK_FOOT_SHADE_METRES = 1.5f;
inline constexpr float WEAK_FOOT_ERROR_GAIN = 0.45f;
inline constexpr float WEAK_FOOT_SHOT_PENALTY = 0.35f;
/** Doubling up: second man's distance, engagement and take-on penalty. */
inline constexpr float DOUBLE_UP_SUPPORT_METRES = 2.5f;
inline constexpr float DOUBLE_UP_ENGAGE = 0.4f;
inline constexpr float DOUBLE_UP_TAKE_ON_PENALTY = 0.12f;
inline constexpr float DOUBLE_UP_REACH_METRES = 5.0f;

/** Team talks: full effect for the first minutes of a half, then fading
 * out; a talk at REFERENCE (the team-talk cap) gives the full swing. */
inline constexpr float TALK_FULL_MINUTES = 10.0f;
inline constexpr float TALK_FADE_END_MINUTES = 20.0f;
inline constexpr float TALK_REFERENCE = 0.012f;
/** Decision noise removed and pressing eagerness added at full swing. */
inline constexpr float TALK_COMPOSURE = 0.25f;
inline constexpr float TALK_PRESS_SECONDS = 0.3f;
inline constexpr float TALK_ENGAGE = 0.25f;
}  // namespace TacticsTuning

/**
 * @brief Roles, duties, in-possession shapes and their engine profiles.
 * Pure functions; the tactic data itself lives in Strategy.
 */
namespace Tactics
{
/** Position group of a slot from its lineup position. */
RoleFamily familyForSlot(Vector2F anchor);

/** Roles a family can use; Standard is always first. */
std::span<const TacticalRole> rolesFor(RoleFamily family);

/** Whether @p role is allowed for @p family. */
bool allows(RoleFamily family, TacticalRole role);

/** Whether the family's slots take a duty (goalkeepers do not). */
bool hasDuty(RoleFamily family);

/** Engine profile of a role played with a duty. */
RoleProfile profile(TacticalRole role, RoleDuty duty);

/** Language keys. */
const char* familyKey(RoleFamily family);
const char* roleKey(TacticalRole role);
const char* roleDescriptionKey(TacticalRole role);
const char* dutyKey(RoleDuty duty);
const char* possessionShapeKey(PossessionShape shape);

/**
 * Attributes that matter most for a role in @p family (names of the
 * player's stats, e.g. "Pace"; the label key is "STAT_" + name).
 */
std::span<const std::string_view> keyAttributes(RoleFamily family,
                                                TacticalRole role);

/**
 * Suitability of @p player for a role on the 0-100 attribute scale (the
 * weighted mean of its key attributes).
 */
float roleFit(const Player& player, RoleFamily family, TacticalRole role);

/** Duty that suits the player in a slot of @p family. */
RoleDuty suggestedDuty(const Player& player, RoleFamily family);

/**
 * Role the player suits best in a slot of @p family: Standard unless a
 * special role fits clearly better.
 */
TacticalRole suggestedRole(const Player& player, RoleFamily family);

/** In-possession shift of a slot for a ready-made shape. */
Vector2F possessionOffset(PossessionShape shape, Vector2F anchor);

/**
 * Gives every starter of @p lineup the role and duty that fit him (AI
 * clubs); in-possession offsets are cleared.
 */
void assignRolesToFit(Strategy& strategy, const Lineup& lineup);
}  // namespace Tactics
