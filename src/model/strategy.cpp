#include "strategy.h"
#include "global.h"
#include <algorithm>
#include <cmath>

Strategy::Strategy() {
  sliders = StrategySliders{};
  goalkeeper = RoleWeights{0.5f, 0.5f, 1};
  for (int i = 0; i < 10; ++i) outfield[i] = RoleWeights{0.5f, 0.5f, 1};
}

// -------------------- Attack/Defense Weight Queries --------------------
float Strategy::getAttackWeight(int roleIndex, int gridPosition) const {
  if (roleIndex < 0 || roleIndex > 10) return 0.0f; // invalid index
  const RoleWeights* role = (roleIndex == 0) ? &goalkeeper : &outfield[roleIndex - 1];
  int defaultPosition = (roleIndex == 0) ? 0 : roleIndex - 1;
  auto [roleRow, roleCol] = toRowCol(defaultPosition);
  auto [cellRow, cellCol] = toRowCol(gridPosition);
  int dist = std::abs(cellRow - roleRow) + std::abs(cellCol - roleCol);
  return (dist <= role->movementRadius) ? role->attackWeight : 0.0f;
}

float Strategy::getDefenseWeight(int roleIndex, int gridPosition) const {
  if (roleIndex < 0 || roleIndex > 10) return 0.0f; // invalid index
  const RoleWeights* role = (roleIndex == 0) ? &goalkeeper : &outfield[roleIndex - 1];
  int defaultPosition = (roleIndex == 0) ? 0 : roleIndex - 1;
  auto [roleRow, roleCol] = toRowCol(defaultPosition);
  auto [cellRow, cellCol] = toRowCol(gridPosition);
  int dist = std::abs(cellRow - roleRow) + std::abs(cellCol - roleCol);
  return (dist <= role->movementRadius) ? role->defenseWeight : 0.0f;
}

// -------------------- Slider Setters/Getters --------------------
void Strategy::setAllSliders(const StrategySliders& newSliders) {
  sliders.pressing      = std::clamp(newSliders.pressing, 0.0f, 1.0f);
  sliders.riskTaking    = std::clamp(newSliders.riskTaking, 0.0f, 1.0f);
  sliders.offensiveBias = std::clamp(newSliders.offensiveBias, 0.0f, 1.0f);
  sliders.widthUsage    = std::clamp(newSliders.widthUsage, 0.0f, 1.0f);
  sliders.compactness   = std::clamp(newSliders.compactness, 0.0f, 1.0f);
}

StrategySliders Strategy::getSliders() const {
  return sliders;
}

// -------------------- Role Weight Setters --------------------
void Strategy::setOutfieldWeights(int playerIndex, float attack, float defense, int radius) {
  if (playerIndex < 0 || playerIndex >= 10) return;
  outfield[playerIndex].attackWeight  = std::clamp(attack, 0.0f, 1.0f);
  outfield[playerIndex].defenseWeight = std::clamp(defense, 0.0f, 1.0f);
  outfield[playerIndex].movementRadius = std::max(0, radius);
}

void Strategy::setAllOutfieldWeights(float attack, float defense, int radius) {
  for (int i = 0; i < 10; ++i)
    setOutfieldWeights(i, attack, defense, radius);
}

// -------------------- Role Weight Getters --------------------
RoleWeights& Strategy::getRole(int roleIndex) {
  return (roleIndex == 0) ? goalkeeper : outfield[roleIndex-1];
}

const RoleWeights& Strategy::getRole(int roleIndex) const {
  return (roleIndex == 0) ? goalkeeper : outfield[roleIndex-1];
}
