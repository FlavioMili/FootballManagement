// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "lineup.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <sstream>

#include "database/gamedata.h"
#include "model/role_utils.h"

// ---------------- Constructor -----------------
Lineup::Lineup() { clear(); }

// -------------------- Goalkeeper --------------------
void Lineup::setGoalkeeper(const Player* gk)
{
  goalkeeper = gk;  // nullptr allowed
  if (gk)
  {
    removeOutfieldPlayer(gk->getId());
    std::erase_if(reserves, [gk](const Player* player)
                  { return player && player->getId() == gk->getId(); });
  }
}

const Player* Lineup::getGoalkeeper() const { return goalkeeper; }

// -------------- Outfield Players ---------------
void Lineup::addOutfieldPlayer(const Player* player, Vector2F position)
{
  if (!player) return;
  if (goalkeeper && goalkeeper->getId() == player->getId()) return;
  if (std::ranges::any_of(outfield_players,
                          [player](const auto& positioned)
                          {
                            return positioned.player &&
                                   positioned.player->getId() ==
                                       player->getId();
                          }))
  {
    return;
  }
  position.x = std::clamp(position.x, 0.0f, 1.0f);
  position.y = std::clamp(position.y, 0.0f, 1.0f);
  outfield_players.push_back({player, position});
}

bool Lineup::moveOutfieldPlayer(PlayerID playerID, Vector2F newPosition)
{
  for (auto& posPlayer : outfield_players)
  {
    if (posPlayer.player && posPlayer.player->getId() == playerID)
    {
      posPlayer.position = {std::clamp(newPosition.x, 0.0f, 1.0f),
                            std::clamp(newPosition.y, 0.0f, 1.0f)};
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
  reserves.clear();
  for (const Player* player : subs)
  {
    if (!player || (goalkeeper && goalkeeper->getId() == player->getId()) ||
        std::ranges::any_of(outfield_players,
                            [player](const auto& positioned)
                            {
                              return positioned.player &&
                                     positioned.player->getId() ==
                                         player->getId();
                            }) ||
        std::ranges::contains(reserves, player))
    {
      continue;
    }
    reserves.push_back(player);
  }
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
      {
        if (bestGK) reserves.push_back(bestGK);
        bestGK = &p;
      }
      else
      {
        reserves.push_back(&p);
      }
    }
    else
    {
      potentialOutfieldPlayers.push_back(&p);
    }
  }

  // If no GK found, try to use the worst outfield player as GK
  if (!bestGK && !potentialOutfieldPlayers.empty())
  {
    const auto emergencyGoalkeeper = std::ranges::min_element(
        potentialOutfieldPlayers, [&](const Player* a, const Player* b)
        { return a->getOverall(stats_config) < b->getOverall(stats_config); });
    bestGK = *emergencyGoalkeeper;
    potentialOutfieldPlayers.erase(emergencyGoalkeeper);
  }

  goalkeeper = bestGK;
  if (!goalkeeper) return;  // Still no players at all

  // Fill a balanced 4-4-2. Pure overall sorting routinely produced teams with
  // no defenders because ratings from unlike roles are not interchangeable.
  static constexpr std::array<PlayerRole, 10> SLOT_ROLES = {
      PlayerRole::LB, PlayerRole::CB, PlayerRole::CB, PlayerRole::RB,
      PlayerRole::LM, PlayerRole::CM, PlayerRole::CM, PlayerRole::RM,
      PlayerRole::ST, PlayerRole::ST};
  static constexpr std::array<Vector2F, 10> SLOT_POSITIONS = {
      Vector2F{0.20f, 0.12f}, Vector2F{0.20f, 0.38f}, Vector2F{0.20f, 0.62f},
      Vector2F{0.20f, 0.88f}, Vector2F{0.43f, 0.12f}, Vector2F{0.43f, 0.38f},
      Vector2F{0.43f, 0.62f}, Vector2F{0.43f, 0.88f}, Vector2F{0.78f, 0.38f},
      Vector2F{0.78f, 0.62f}};

  const auto isDefender = [](PlayerRole role)
  {
    return role == PlayerRole::LB || role == PlayerRole::CB ||
           role == PlayerRole::RB;
  };
  const auto isCentralMidfielder = [](PlayerRole role)
  {
    return role == PlayerRole::CDM || role == PlayerRole::CM ||
           role == PlayerRole::CAM;
  };
  const auto roleFit = [&](PlayerRole actual, PlayerRole expected)
  {
    if (actual == expected) return 30.0;
    if (expected == PlayerRole::CB && isDefender(actual)) return 16.0;
    if ((expected == PlayerRole::LB || expected == PlayerRole::RB) &&
        isDefender(actual))
      return 13.0;
    if (expected == PlayerRole::CM && isCentralMidfielder(actual)) return 24.0;
    if (expected == PlayerRole::LM &&
        (actual == PlayerRole::LW || actual == PlayerRole::LB))
      return 18.0;
    if (expected == PlayerRole::RM &&
        (actual == PlayerRole::RW || actual == PlayerRole::RB))
      return 18.0;
    if ((expected == PlayerRole::LM || expected == PlayerRole::RM) &&
        (isCentralMidfielder(actual) || actual == PlayerRole::LM ||
         actual == PlayerRole::RM || actual == PlayerRole::LW ||
         actual == PlayerRole::RW))
      return 10.0;
    if (expected == PlayerRole::ST &&
        (actual == PlayerRole::LW || actual == PlayerRole::RW))
      return 15.0;
    return -15.0;
  };

  for (size_t slot = 0;
       slot < SLOT_ROLES.size() && !potentialOutfieldPlayers.empty(); ++slot)
  {
    auto best = potentialOutfieldPlayers.end();
    double bestScore = -std::numeric_limits<double>::infinity();
    for (auto candidate = potentialOutfieldPlayers.begin();
         candidate != potentialOutfieldPlayers.end(); ++candidate)
    {
      const double score = (*candidate)->getOverall(stats_config) +
                           roleFit((*candidate)->getRole(), SLOT_ROLES[slot]);
      if (score > bestScore)
      {
        bestScore = score;
        best = candidate;
      }
    }
    addOutfieldPlayer(*best, SLOT_POSITIONS[slot]);
    potentialOutfieldPlayers.erase(best);
  }

  reserves.insert(reserves.end(), potentialOutfieldPlayers.begin(),
                  potentialOutfieldPlayers.end());
}
