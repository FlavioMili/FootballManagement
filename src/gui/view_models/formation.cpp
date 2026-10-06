// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "gui/view_models/formation.h"

#include <algorithm>
#include <cmath>

#include "model/lineup.h"
#include "model/player.h"

namespace
{
using enum PlayerRole;

constexpr float MATCH_TOLERANCE = 0.03f;
constexpr double BASE_WEIGHT = 0.35;
constexpr double FIT_WEIGHT = 0.65;

bool isDefender(PlayerRole role)
{
  return role == CB || role == LB || role == RB;
}
bool isCentral(PlayerRole role)
{
  return role == CDM || role == CM || role == CAM;
}
bool isLeft(PlayerRole role) { return role == LB || role == LM || role == LW; }
bool isRight(PlayerRole role) { return role == RB || role == RM || role == RW; }
bool isWideMidfielder(PlayerRole role) { return role == LM || role == RM; }
bool isWinger(PlayerRole role) { return role == LW || role == RW; }
bool sameSide(PlayerRole a, PlayerRole b)
{
  return (isLeft(a) && isLeft(b)) || (isRight(a) && isRight(b));
}

struct Assignment
{
  const Player* player;
  size_t slot;
};

// Global greedy matching: repeatedly takes the best remaining (slot, player)
// pair. Weighted overall keeps natural roles strongly preferred while still
// letting a much better player cover an adjacent role.
std::vector<Assignment> assign(const Formation::Preset& preset,
                               std::span<const Player* const> candidates,
                               const StatsConfig& config)
{
  std::vector<double> overall;
  overall.reserve(candidates.size());
  for (const Player* player : candidates)
    overall.push_back(player->getOverall(config));

  std::vector<Assignment> result;
  std::vector<bool> used(candidates.size(), false);
  std::array<bool, 10> filled{};
  const size_t picks = std::min(preset.slots.size(), candidates.size());
  for (size_t pick = 0; pick < picks; ++pick)
  {
    double bestScore = -1.0;
    size_t bestSlot = 0;
    size_t bestCandidate = 0;
    for (size_t slot = 0; slot < preset.slots.size(); ++slot)
    {
      if (filled[slot]) continue;
      for (size_t index = 0; index < candidates.size(); ++index)
      {
        if (used[index]) continue;
        const auto fit = static_cast<double>(Formation::roleFit(
            candidates[index]->getRole(), preset.slots[slot].role));
        const double score = overall[index] * (BASE_WEIGHT + FIT_WEIGHT * fit);
        if (score > bestScore)
        {
          bestScore = score;
          bestSlot = slot;
          bestCandidate = index;
        }
      }
    }
    filled[bestSlot] = true;
    used[bestCandidate] = true;
    result.push_back({candidates[bestCandidate], bestSlot});
  }
  return result;
}
}  // namespace

namespace Formation
{

// clang-format off
const std::array<Preset, 5> PRESETS = {{
    {"4-4-2", {{{LB, {0.20f, 0.12f}}, {CB, {0.20f, 0.38f}}, {CB, {0.20f, 0.62f}},
                {RB, {0.20f, 0.88f}}, {LM, {0.43f, 0.12f}}, {CM, {0.43f, 0.38f}},
                {CM, {0.43f, 0.62f}}, {RM, {0.43f, 0.88f}}, {ST, {0.78f, 0.38f}},
                {ST, {0.78f, 0.62f}}}}},
    {"4-3-3", {{{LB, {0.20f, 0.12f}}, {CB, {0.20f, 0.38f}}, {CB, {0.20f, 0.62f}},
                {RB, {0.20f, 0.88f}}, {CM, {0.44f, 0.28f}}, {CDM, {0.38f, 0.50f}},
                {CM, {0.44f, 0.72f}}, {LW, {0.72f, 0.14f}}, {ST, {0.80f, 0.50f}},
                {RW, {0.72f, 0.86f}}}}},
    {"4-2-3-1", {{{LB, {0.20f, 0.12f}}, {CB, {0.20f, 0.38f}}, {CB, {0.20f, 0.62f}},
                  {RB, {0.20f, 0.88f}}, {CDM, {0.37f, 0.38f}}, {CDM, {0.37f, 0.62f}},
                  {LW, {0.60f, 0.15f}}, {CAM, {0.58f, 0.50f}}, {RW, {0.60f, 0.85f}},
                  {ST, {0.80f, 0.50f}}}}},
    {"3-5-2", {{{CB, {0.20f, 0.26f}}, {CB, {0.18f, 0.50f}}, {CB, {0.20f, 0.74f}},
                {LM, {0.46f, 0.09f}}, {CM, {0.45f, 0.32f}}, {CDM, {0.38f, 0.50f}},
                {CM, {0.45f, 0.68f}}, {RM, {0.46f, 0.91f}}, {ST, {0.78f, 0.38f}},
                {ST, {0.78f, 0.62f}}}}},
    {"5-3-2", {{{LB, {0.27f, 0.08f}}, {CB, {0.20f, 0.30f}}, {CB, {0.18f, 0.50f}},
                {CB, {0.20f, 0.70f}}, {RB, {0.27f, 0.92f}}, {CM, {0.44f, 0.28f}},
                {CM, {0.42f, 0.50f}}, {CM, {0.44f, 0.72f}}, {ST, {0.76f, 0.38f}},
                {ST, {0.76f, 0.62f}}}}},
}};
// clang-format on

float roleFit(PlayerRole actual, PlayerRole expected)
{
  if (actual == expected) return 1.0f;
  if (actual == GK || expected == GK) return 0.0f;
  if (isDefender(expected) && isDefender(actual))
    return expected == CB || actual == CB ? 0.6f : 0.75f;
  if (expected == CB && actual == CDM) return 0.55f;
  if (isCentral(expected) && isCentral(actual)) return 0.8f;
  if (isWideMidfielder(expected))
  {
    if (isWinger(actual) && sameSide(actual, expected)) return 0.8f;
    if (isWideMidfielder(actual) || isWinger(actual)) return 0.6f;
    if (isDefender(actual) && sameSide(actual, expected)) return 0.55f;
    if (isCentral(actual)) return 0.55f;
  }
  if (isWinger(expected))
  {
    if (isWideMidfielder(actual) && sameSide(actual, expected)) return 0.8f;
    if (isWinger(actual)) return 0.7f;
    if (actual == ST) return 0.6f;
    if (actual == CAM || isWideMidfielder(actual)) return 0.5f;
  }
  if (expected == ST && (isWinger(actual) || actual == CAM)) return 0.6f;
  if (expected == CAM && (isWinger(actual) || actual == ST)) return 0.55f;
  return 0.2f;
}

void applyPreset(Lineup& lineup, const Preset& preset,
                 const StatsConfig& config)
{
  std::vector<const Player*> outfield;
  std::vector<Lineup::PositionedPlayer> previous = lineup.getOutfieldPlayers();
  for (const auto& positioned : previous)
    if (positioned.player) outfield.push_back(positioned.player);
  const std::vector<Assignment> assignments = assign(preset, outfield, config);

  const Player* goalkeeper = lineup.getGoalkeeper();
  lineup.clear();
  lineup.setGoalkeeper(goalkeeper);
  for (const Assignment& assignment : assignments)
    lineup.addOutfieldPlayer(assignment.player,
                             preset.slots[assignment.slot].position);
  // More than ten outfielders cannot happen through the UI; keep any extra
  // at their previous spot rather than dropping them.
  for (const auto& positioned : previous)
  {
    if (!positioned.player) continue;
    const bool placed = std::ranges::any_of(
        assignments, [&positioned](const Assignment& assignment)
        { return assignment.player == positioned.player; });
    if (!placed)
      lineup.addOutfieldPlayer(positioned.player, positioned.position);
  }
}

void autoPick(Lineup& lineup, const Preset& preset,
              std::span<const Player* const> squad, const StatsConfig& config)
{
  const Player* goalkeeper = nullptr;
  std::vector<const Player*> outfield;
  for (const Player* player : squad)
  {
    if (player->getRole() != GK)
    {
      outfield.push_back(player);
      continue;
    }
    if (!goalkeeper ||
        player->getOverall(config) > goalkeeper->getOverall(config))
      goalkeeper = player;
  }
  if (!goalkeeper && !outfield.empty())
  {
    // Emergency keeper: the weakest outfielder, as in Lineup's own pick.
    const auto weakest = std::ranges::min_element(
        outfield, [&config](const Player* left, const Player* right)
        { return left->getOverall(config) < right->getOverall(config); });
    goalkeeper = *weakest;
    outfield.erase(weakest);
  }

  const std::vector<Assignment> assignments = assign(preset, outfield, config);
  lineup.clear();
  lineup.setGoalkeeper(goalkeeper);
  for (const Assignment& assignment : assignments)
    lineup.addOutfieldPlayer(assignment.player,
                             preset.slots[assignment.slot].position);

  std::vector<const Player*> reserves;
  for (const Player* player : squad)
  {
    const bool starting =
        player == goalkeeper ||
        std::ranges::any_of(assignments, [player](const Assignment& assignment)
                            { return assignment.player == player; });
    if (!starting) reserves.push_back(player);
  }
  // The matchday bench is picked from the rest, best first.
  std::ranges::stable_sort(
      reserves, [&config](const Player* left, const Player* right)
      { return left->getOverall(config) > right->getOverall(config); });
  lineup.setReserves(reserves);
}

void autoPickAvailable(Lineup& lineup, const Preset& preset,
                       std::span<const Player* const> squad,
                       const std::unordered_set<PlayerID>& unavailable,
                       const StatsConfig& config)
{
  std::vector<const Player*> available;
  for (const Player* player : squad)
    if (!unavailable.contains(player->getId())) available.push_back(player);
  // Injured and suspended players stay out of the matchday squad.
  autoPick(lineup, preset, available, config);
}

size_t unavailableStarters(const Lineup& lineup,
                           const std::unordered_set<PlayerID>& unavailable)
{
  size_t count = 0;
  if (const Player* goalkeeper = lineup.getGoalkeeper();
      goalkeeper && unavailable.contains(goalkeeper->getId()))
    ++count;
  for (const auto& positioned : lineup.getOutfieldPlayers())
    if (positioned.player && unavailable.contains(positioned.player->getId()))
      ++count;
  return count;
}

int detectPreset(const Lineup& lineup)
{
  const auto& outfield = lineup.getOutfieldPlayers();
  if (outfield.size() != 10) return -1;
  for (size_t presetIndex = 0; presetIndex < PRESETS.size(); ++presetIndex)
  {
    const Preset& preset = PRESETS[presetIndex];
    std::array<bool, 10> taken{};
    bool matches = true;
    for (const auto& positioned : outfield)
    {
      bool found = false;
      for (size_t slot = 0; slot < preset.slots.size() && !found; ++slot)
      {
        if (taken[slot]) continue;
        const Vector2F& target = preset.slots[slot].position;
        if (std::abs(target.x - positioned.position.x) <= MATCH_TOLERANCE &&
            std::abs(target.y - positioned.position.y) <= MATCH_TOLERANCE)
        {
          taken[slot] = true;
          found = true;
        }
      }
      if (!found)
      {
        matches = false;
        break;
      }
    }
    if (matches) return static_cast<int>(presetIndex);
  }
  return -1;
}

}  // namespace Formation
