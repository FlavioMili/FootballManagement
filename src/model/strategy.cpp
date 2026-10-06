// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "strategy.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

#include "global/global.h"

Strategy::Strategy()
{
  sliders = StrategySliders{};
  goalkeeper = RoleWeights{0.5f, 0.5f, 1};
  for (int i = 0; i < 10; ++i) outfield[i] = RoleWeights{0.5f, 0.5f, 1};
}

// -------------------- Attack/Defense Weight Queries --------------------
float Strategy::getAttackWeight(int roleIndex, int gridPosition) const
{
  if (roleIndex < 0 || roleIndex > 10) return 0.0f;  // invalid index
  const RoleWeights* role =
      (roleIndex == 0) ? &goalkeeper : &outfield[roleIndex - 1];
  int defaultPosition = (roleIndex == 0) ? 0 : roleIndex - 1;
  auto [roleRow, roleCol] = toRowCol(defaultPosition);
  auto [cellRow, cellCol] = toRowCol(gridPosition);
  int dist = std::abs(cellRow - roleRow) + std::abs(cellCol - roleCol);
  return (dist <= role->movementRadius) ? role->attackWeight : 0.0f;
}

float Strategy::getDefenseWeight(int roleIndex, int gridPosition) const
{
  if (roleIndex < 0 || roleIndex > 10) return 0.0f;  // invalid index
  const RoleWeights* role =
      (roleIndex == 0) ? &goalkeeper : &outfield[roleIndex - 1];
  int defaultPosition = (roleIndex == 0) ? 0 : roleIndex - 1;
  auto [roleRow, roleCol] = toRowCol(defaultPosition);
  auto [cellRow, cellCol] = toRowCol(gridPosition);
  int dist = std::abs(cellRow - roleRow) + std::abs(cellCol - roleCol);
  return (dist <= role->movementRadius) ? role->defenseWeight : 0.0f;
}

// -------------------- Slider Setters/Getters --------------------
void Strategy::setAllSliders(const StrategySliders& newSliders)
{
  sliders.pressing = std::clamp(newSliders.pressing, 0.0f, 1.0f);
  sliders.riskTaking = std::clamp(newSliders.riskTaking, 0.0f, 1.0f);
  sliders.offensiveBias = std::clamp(newSliders.offensiveBias, 0.0f, 1.0f);
  sliders.widthUsage = std::clamp(newSliders.widthUsage, 0.0f, 1.0f);
  sliders.compactness = std::clamp(newSliders.compactness, 0.0f, 1.0f);
}

StrategySliders Strategy::getSliders() const { return sliders; }

// -------------------- Role Weight Setters --------------------
void Strategy::setOutfieldWeights(int playerIndex, float attack, float defense,
                                  int radius)
{
  if (playerIndex < 0 || playerIndex >= 10) return;
  outfield[playerIndex].attackWeight = std::clamp(attack, 0.0f, 1.0f);
  outfield[playerIndex].defenseWeight = std::clamp(defense, 0.0f, 1.0f);
  outfield[playerIndex].movementRadius = std::max(0, radius);
}

void Strategy::setAllOutfieldWeights(float attack, float defense, int radius)
{
  for (int i = 0; i < 10; ++i) setOutfieldWeights(i, attack, defense, radius);
}

// -------------------- Role Weight Getters --------------------
RoleWeights& Strategy::getRole(int roleIndex)
{
  roleIndex = std::clamp(roleIndex, 0, 10);
  return (roleIndex == 0) ? goalkeeper : outfield[roleIndex - 1];
}

const RoleWeights& Strategy::getRole(int roleIndex) const
{
  roleIndex = std::clamp(roleIndex, 0, 10);
  return (roleIndex == 0) ? goalkeeper : outfield[roleIndex - 1];
}

// -------------------- Roles and in-possession shape --------------------
namespace
{
/** Anchors closer than this are the same slot. */
constexpr float SAME_SLOT_DISTANCE = 0.01f;
/** Slot instructions remembered across formations. */
constexpr std::size_t MAX_SLOT_INSTRUCTIONS = 40;

float anchorDistance(Vector2F first, Vector2F second)
{
  return std::hypot(first.x - second.x, first.y - second.y);
}

SlotInstruction sanitized(SlotInstruction instruction)
{
  const auto finite = [](float value) { return std::isfinite(value); };
  if (!finite(instruction.anchor.x) || !finite(instruction.anchor.y))
    instruction.anchor = {0.5f, 0.5f};
  instruction.anchor.x = std::clamp(instruction.anchor.x, 0.0f, 1.0f);
  instruction.anchor.y = std::clamp(instruction.anchor.y, 0.0f, 1.0f);
  if (instruction.role >= TacticalRole::COUNT)
    instruction.role = TacticalRole::Standard;
  if (instruction.duty >= RoleDuty::COUNT) instruction.duty = RoleDuty::Support;
  Vector2F& offset = instruction.possessionOffset;
  offset.x = finite(offset.x) ? offset.x : 0.0f;
  offset.y = finite(offset.y) ? offset.y : 0.0f;
  // The in-possession spot stays on the pitch.
  offset.x = std::clamp(instruction.anchor.x + offset.x, 0.02f, 0.98f) -
             instruction.anchor.x;
  offset.y = std::clamp(instruction.anchor.y + offset.y, 0.02f, 0.98f) -
             instruction.anchor.y;
  return instruction;
}
}  // namespace

void Strategy::setSlotInstructions(std::vector<SlotInstruction> instructions)
{
  slots.clear();
  for (const SlotInstruction& instruction : instructions) setSlot(instruction);
}

const SlotInstruction* Strategy::findSlot(Vector2F anchor) const
{
  const SlotInstruction* nearest = nullptr;
  float best = TacticsTuning::SLOT_MATCH_DISTANCE;
  for (const SlotInstruction& instruction : slots)
  {
    const float gap = anchorDistance(instruction.anchor, anchor);
    if (gap <= best)
    {
      best = gap;
      nearest = &instruction;
    }
  }
  return nearest;
}

void Strategy::setSlot(const SlotInstruction& instruction)
{
  const SlotInstruction clean = sanitized(instruction);
  std::erase_if(slots,
                [&clean](const SlotInstruction& stored)
                {
                  return anchorDistance(stored.anchor, clean.anchor) <
                         SAME_SLOT_DISTANCE;
                });
  slots.push_back(clean);
  // The oldest instructions (formations not used for a while) go first.
  if (slots.size() > MAX_SLOT_INSTRUCTIONS)
    slots.erase(slots.begin(),
                slots.begin() + static_cast<std::ptrdiff_t>(
                                    slots.size() - MAX_SLOT_INSTRUCTIONS));
}

void Strategy::setKeeperRole(TacticalRole role)
{
  keeper_role = Tactics::allows(RoleFamily::Goalkeeper, role)
                    ? role
                    : TacticalRole::Standard;
}

void Strategy::setOppositionOrders(std::vector<PlayerInstruction> orders)
{
  std::erase_if(orders,
                [](const PlayerInstruction& order)
                {
                  return order.player == 0 ||
                         order.instruction == OppositionInstruction::None ||
                         order.instruction >= OppositionInstruction::COUNT;
                });
  opposition_orders = std::move(orders);
}
