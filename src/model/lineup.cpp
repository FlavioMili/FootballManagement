// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "lineup.h"

#include <algorithm>
#include <cmath>
#include <sstream>

#include "database/gamedata.h"
#include "model/role_utils.h"

// ---------------- Constructor -----------------
Lineup::Lineup() { clear(); }

// -------------------- Goalkeeper --------------------
void Lineup::setGoalkeeper(const Player* gk)
{
  goalkeeper = gk;  // nullptr allowed
}

const Player* Lineup::getGoalkeeper() const { return goalkeeper; }

// -------------- Outfield Players ---------------
void Lineup::addOutfieldPlayer(const Player* player, Vector2F position)
{
  if (!player) return;
  outfield_players.push_back({player, position});
}

bool Lineup::moveOutfieldPlayer(PlayerID playerID, Vector2F newPosition)
{
  for (auto& posPlayer : outfield_players)
  {
    if (posPlayer.player && posPlayer.player->getId() == playerID)
    {
      posPlayer.position = newPosition;
      return true;
    }
  }
  return false;
}

void Lineup::removeOutfieldPlayer(PlayerID playerID)
{
  auto [first, last] = std::ranges::remove_if(
      outfield_players, [playerID](const PositionedPlayer& pp)
      { return pp.player && pp.player->getId() == playerID; });
  outfield_players.erase(first, last);
}

const std::vector<Lineup::PositionedPlayer>& Lineup::getOutfieldPlayers() const
{
  return outfield_players;
}

bool Lineup::swapPlayers(PlayerID benchPlayerID, PlayerID pitchPlayerID)
{
  // Find bench player
  auto benchIt =
      std::ranges::find_if(reserves, [benchPlayerID](const Player* p)
                           { return p && p->getId() == benchPlayerID; });
  if (benchIt == reserves.end()) return false;

  // Check if it's the goalkeeper
  if (goalkeeper && goalkeeper->getId() == pitchPlayerID)
  {
    const Player* temp = *benchIt;
    *benchIt = goalkeeper;
    goalkeeper = temp;
    return true;
  }

  // Find pitch player
  if (auto pitchIt = std::ranges::find_if(
          outfield_players, [pitchPlayerID](const PositionedPlayer& pp)
          { return pp.player && pp.player->getId() == pitchPlayerID; });
      pitchIt != outfield_players.end())
  {
    const Player* temp = *benchIt;
    *benchIt = pitchIt->player;
    pitchIt->player = temp;
    return true;
  }

  return false;
}

// -------------- Reserves ---------------
void Lineup::setReserves(const std::vector<const Player*>& subs)
{
  reserves = subs;  // copies the pointers; nullptrs allowed
}

const std::vector<const Player*>& Lineup::getReserves() const
{
  return reserves;
}

// --------------- Strategy ------------------
void Lineup::setStrategy(const Strategy& strat) { strategy = strat; }

const Strategy& Lineup::getStrategy() const { return strategy; }

// ---------- Debug / Visualisation --------------
std::string Lineup::toString() const
{
  std::ostringstream oss;
  oss << "Goalkeeper: " << (goalkeeper ? goalkeeper->getName() : "None")
      << "\n";
  oss << "Outfield Players:\n";
  for (const auto& posPlayer : outfield_players)
  {
    if (posPlayer.player)
    {
      oss << "- " << posPlayer.player->getName() << " at ("
          << posPlayer.position.x << ", " << posPlayer.position.y << ")\n";
    }
  }
  oss << "Reserves: ";
  for (auto* sub : reserves)
  {
    oss << (sub ? sub->getName() : "Empty") << " ";
  }
  oss << "\n";
  return oss.str();
}

void Lineup::generateStartingXI(const class GameData& gamedata,
                                const std::vector<PlayerID>& allPlayerIDs,
                                const StatsConfig& stats_config)
{
  // Clear previous lineup
  clear();
  reserves.clear();

  std::vector<const Player*> potentialOutfieldPlayers;
  const Player* bestGK = nullptr;

  // Separate GK from outfield
  for (const auto& playerID : allPlayerIDs)
  {
    const Player& p = gamedata.getPlayers().at(playerID);
    if (p.getRole() == PlayerRole::GK)
    {
      if (!bestGK ||
          p.getOverall(stats_config) > bestGK->getOverall(stats_config))
        bestGK = &p;
    }
    else
    {
      potentialOutfieldPlayers.push_back(&p);
    }
  }

  // Sort outfield players by overall descending to pick the best 10
  std::ranges::sort(
      potentialOutfieldPlayers, [&](const Player* a, const Player* b)
      { return a->getOverall(stats_config) > b->getOverall(stats_config); });

  // If no GK found, try to use the worst outfield player as GK
  if (!bestGK && !potentialOutfieldPlayers.empty())
  {
    bestGK = potentialOutfieldPlayers.back();
    potentialOutfieldPlayers.pop_back();
  }

  goalkeeper = bestGK;
  if (!goalkeeper) return;  // Still no players at all

  // Take top 10 (or less if not enough players)
  std::vector<const Player*> startingOutfield;
  for (size_t i = 0; i < potentialOutfieldPlayers.size(); ++i)
  {
    if (i < 10)
    {
      startingOutfield.push_back(potentialOutfieldPlayers[i]);
    }
    else
    {
      reserves.push_back(potentialOutfieldPlayers[i]);
    }
  }

  // Sort the starting 10 by their role's natural X coordinate descending
  // (Strikers first), then by Y ascending
  std::ranges::sort(startingOutfield,
                    [](const Player* a, const Player* b)
                    {
                      auto posA = RoleUtils::getBaseCoordinate(a->getRole());
                      auto posB = RoleUtils::getBaseCoordinate(b->getRole());
                      if (posA.x != posB.x) return posA.x > posB.x;
                      return posA.y < posB.y;
                    });

  // Fixed 4-4-2 formation coordinates (Horizontal Pitch: x is length, y is
  // width)
  static const std::vector<Vector2F> formation442 = {
      // 2 Strikers (Front: x = 0.8)
      {0.8f, 0.35f},
      {0.8f, 0.65f},
      // 4 Midfielders (Middle: x = 0.5)
      {0.5f, 0.15f},
      {0.5f, 0.35f},
      {0.5f, 0.65f},
      {0.5f, 0.85f},
      // 4 Defenders (Back: x = 0.2)
      {0.2f, 0.15f},
      {0.2f, 0.35f},
      {0.2f, 0.65f},
      {0.2f, 0.85f}};

  // Assign positions
  for (size_t i = 0; i < startingOutfield.size(); ++i)
  {
    if (i < formation442.size())
    {
      addOutfieldPlayer(startingOutfield[i], formation442[i]);
    }
    else
    {
      reserves.push_back(startingOutfield[i]);
    }
  }
}
