// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "match_engine.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>

#include "model/role_utils.h"

namespace
{
float distance(Vector2F a, Vector2F b)
{
  float dx = a.x - b.x;
  float dy = a.y - b.y;
  return std::sqrt(dx * dx + dy * dy);
}

Vector2F normalize(Vector2F v)
{
  if (float len = std::sqrt(v.x * v.x + v.y * v.y); len > 0.0001f)
  {
    return {v.x / len, v.y / len};
  }
  return {0.0f, 0.0f};
}
}  // namespace

MatchEngine::MatchEngine(const Lineup& home_lineup, const Lineup& away_lineup,
                         const Strategy& home_strat, const Strategy& away_strat,
                         const StatsConfig& config)
    : statsConfig(config), homeStrategy(home_strat), awayStrategy(away_strat)
{
  initializePlayers(home_lineup, true);
  initializePlayers(away_lineup, false);

  logEvent("Match started.");
  resetPositions(false);  // Give Home team the kickoff
}

void MatchEngine::initializePlayers(const Lineup& lineup, bool isHomeTeam)
{
  if (const Player* gk = lineup.getGoalkeeper())
  {
    MatchPlayer mp;
    mp.player = gk;
    mp.isHomeTeam = isHomeTeam;
    mp.basePosition =
        isHomeTeam ? Vector2F{0.05f, 0.5f} : Vector2F{0.95f, 0.5f};
    mp.position = mp.basePosition;
    mp.velocity = {0.0f, 0.0f};
    mp.maxSpeed = 0.05f;  // GK is slower
    mp.acceleration = 0.1f;
    players.push_back(mp);
  }

  for (const auto& posPlayer : lineup.getOutfieldPlayers())
  {
    if (!posPlayer.player) continue;

    MatchPlayer mp;
    mp.player = posPlayer.player;
    mp.isHomeTeam = isHomeTeam;

    float px = posPlayer.position.x;
    float py = posPlayer.position.y;

    if (isHomeTeam)
    {
      mp.basePosition = {px, py};
    }
    else
    {
      mp.basePosition = {1.0f - px, 1.0f - py};
    }

    mp.position = mp.basePosition;
    mp.velocity = {0.0f, 0.0f};

    // Scale speed by stats (make stats matter much more)
    float ovr = mp.player->getOverall(statsConfig) / 100.0f;
    mp.maxSpeed = 0.05f + ovr * 0.15f;
    mp.acceleration = 0.1f + ovr * 0.2f;

    players.push_back(mp);
  }
}

void MatchEngine::substitutePlayer(uint32_t outPlayerId, const Player* inPlayer)
{
  for (auto& mp : players)
  {
    if (mp.player && mp.player->getId() == outPlayerId)
    {
      mp.player = inPlayer;
      float ovr = inPlayer->getOverall(statsConfig) / 100.0f;
      mp.maxSpeed = 0.05f + ovr * 0.15f;
      mp.acceleration = 0.1f + ovr * 0.2f;
      logEvent("Substitution: " + inPlayer->getName() + " comes on.");

      if (ball.possessedBy && ball.possessedBy->getId() == outPlayerId)
      {
        ball.possessedBy = nullptr;  // Drop ball if subbed while possessing
      }
      break;
    }
  }
}

void MatchEngine::update(float deltaTime)
{
  if (matchTimeMinutes >= 90.0f) return;

  float simDt = deltaTime * TIME_SCALE / 60.0f;  // dt in minutes
  matchTimeMinutes += simDt;

  if (ball.passCooldown > 0.0f)
  {
    ball.passCooldown -= deltaTime;
  }

  calculateForces(deltaTime);
  resolvePossessionAndPassing(deltaTime);
  checkGoals();

  // Update ball physics
  ball.position.x += ball.velocity.x * deltaTime;
  ball.position.y += ball.velocity.y * deltaTime;
  ball.velocity.x *= ball.friction;
  ball.velocity.y *= ball.friction;

  // Bounce ball off walls
  if (ball.position.x < 0.0f)
  {
    ball.position.x = 0.0f;
    ball.velocity.x *= -1;
  }
  if (ball.position.x > 1.0f)
  {
    ball.position.x = 1.0f;
    ball.velocity.x *= -1;
  }
  if (ball.position.y < 0.0f)
  {
    ball.position.y = 0.0f;
    ball.velocity.y *= -1;
  }
  if (ball.position.y > 1.0f)
  {
    ball.position.y = 1.0f;
    ball.velocity.y *= -1;
  }
}

void MatchEngine::calculateForces(float dt)
{
  for (auto& mp : players)
  {
    Vector2F force = {0.0f, 0.0f};

    if (ball.possessedBy == mp.player)
    {
      // Carrier runs towards opponent goal, drifting slightly towards center
      Vector2F toGoal =
          mp.isHomeTeam ? Vector2F{1.0f - mp.position.x, 0.5f - mp.position.y}
                        : Vector2F{0.0f - mp.position.x, 0.5f - mp.position.y};
      toGoal.y += (mp.position.y > 0.5f) ? -0.1f : 0.1f;

      Vector2F normToGoal = normalize(toGoal);
      force.x += normToGoal.x * 1.0f;
      force.y += normToGoal.y * 1.0f;
    }
    else
    {
      // Tactical Pull
      Vector2F base = mp.basePosition;

      bool teamHasBall = false;
      if (ball.possessedBy)
      {
        auto it =
            std::ranges::find_if(players, [this](const MatchPlayer& p)
                                 { return p.player == ball.possessedBy; });
        if (it != players.end() && it->isHomeTeam == mp.isHomeTeam)
        {
          teamHasBall = true;
        }
      }

      if (teamHasBall)
      {
        // Attackers push forward and spread out vertically
        if (mp.player->getRole() != PlayerRole::GK)
        {
          float risk = mp.isHomeTeam ? homeStrategy.getSliders().riskTaking
                                     : awayStrategy.getSliders().riskTaking;
          float pushAmount =
              0.1f + risk * 0.2f;  // Risk determines how far forward they push

          base.x += mp.isHomeTeam ? pushAmount : -pushAmount;

          float width = mp.isHomeTeam ? homeStrategy.getSliders().widthUsage
                                      : awayStrategy.getSliders().widthUsage;
          float spread = (mp.position.y > 0.5f) ? (0.05f + width * 0.1f)
                                                : -(0.05f + width * 0.1f);
          base.y += spread;
        }
      }
      else
      {
        // Defenders drop back and squeeze the center
        if (mp.player->getRole() != PlayerRole::GK)
        {
          float compactness = mp.isHomeTeam
                                  ? homeStrategy.getSliders().compactness
                                  : awayStrategy.getSliders().compactness;
          float dropAmount =
              0.05f + compactness * 0.15f;  // Compactness pulls them back more

          base.x -= mp.isHomeTeam ? dropAmount : -dropAmount;

          float squeeze = 0.02f + compactness * 0.08f;
          if (base.y > 0.5f + squeeze) base.y -= squeeze;
          if (base.y < 0.5f - squeeze) base.y += squeeze;
        }
      }

      // Goalkeeper logic
      if (mp.player->getRole() == PlayerRole::GK)
      {
        float goalX = mp.isHomeTeam ? 0.05f : 0.95f;
        base.x = goalX;
        base.y = 0.5f;

        // Follow ball y slightly
        float dy = ball.position.y - 0.5f;
        base.y += std::clamp(dy * 0.3f, -0.15f, 0.15f);

        // If ball is loose or opponent has it, and it's close, rush out
        float distToBall = distance(mp.position, ball.position);
        if (!teamHasBall && distToBall < 0.15f)
        {
          base.x = ball.position.x;
          base.y = ball.position.y;
          mp.maxSpeed = 0.8f;  // Burst speed
        }
        else
        {
          float ovr = mp.player->getOverall(statsConfig) / 100.0f;
          mp.maxSpeed = 0.05f + ovr * 0.15f;
        }
      }

      Vector2F toBase = {base.x - mp.position.x, base.y - mp.position.y};
      force.x += toBase.x * 0.5f;
      force.y += toBase.y * 0.5f;

      // Press the ball if opponent has it, or if it's loose
      if (!teamHasBall && mp.player->getRole() != PlayerRole::GK)
      {
        Vector2F toBall = {ball.position.x - mp.position.x,
                           ball.position.y - mp.position.y};
        float distToBall = distance(mp.position, ball.position);

        float pressing = mp.isHomeTeam ? homeStrategy.getSliders().pressing
                                       : awayStrategy.getSliders().pressing;
        float pressRadius =
            0.1f + pressing * 0.3f;  // High pressing = larger engage radius

        if (distToBall < pressRadius)
        {
          Vector2F normToBall = normalize(toBall);
          float pressAggression = 0.5f + pressing * 1.0f;
          force.x += normToBall.x * pressAggression;
          force.y += normToBall.y * pressAggression;
        }
      }
    }

    // 3. Repulsion from other players
    for (const auto& other : players)
    {
      if (&other == &mp) continue;
      Vector2F toOther = {other.position.x - mp.position.x,
                          other.position.y - mp.position.y};
      float dist = distance(mp.position, other.position);
      if (dist < 0.05f && dist > 0.001f)
      {
        Vector2F normToOther = normalize(toOther);
        force.x -= normToOther.x * (0.05f - dist) * 10.0f;
        force.y -= normToOther.y * (0.05f - dist) * 10.0f;
      }
    }

    // Apply force to velocity
    mp.velocity.x += force.x * mp.acceleration * dt;
    mp.velocity.y += force.y * mp.acceleration * dt;

    // Clamp velocity to max speed
    if (float speed = std::sqrt(mp.velocity.x * mp.velocity.x +
                                mp.velocity.y * mp.velocity.y);
        speed > mp.maxSpeed)
    {
      mp.velocity.x = (mp.velocity.x / speed) * mp.maxSpeed;
      mp.velocity.y = (mp.velocity.y / speed) * mp.maxSpeed;
    }

    // Update position
    mp.position.x += mp.velocity.x * dt;
    mp.position.y += mp.velocity.y * dt;

    // Clamp position to pitch
    mp.position.x = std::clamp(mp.position.x, 0.0f, 1.0f);
    mp.position.y = std::clamp(mp.position.y, 0.0f, 1.0f);
  }
}

void MatchEngine::resolvePossessionAndPassing(float dt)
{
  if (ball.possessedBy)
  {
    // Make the ball stick to the possessing player
    auto it = std::ranges::find_if(players, [this](const MatchPlayer& mp)
                                   { return mp.player == ball.possessedBy; });
    if (it != players.end())
    {
      ball.position = it->position;
      ball.velocity = {0.0f, 0.0f};

      // Random passing logic
      static std::mt19937 rng(42);
      std::uniform_real_distribution<float> dist(0.0f, 1.0f);

      // Decision making based on time, not frames. Roughly 1 action every 2 sim
      // seconds.
      if (dist(rng) < 0.5f * dt)
      {
        bool isHome = it->isHomeTeam;
        float distToGoal = distance(
            it->position, isHome ? Vector2F{1.0f, 0.5f} : Vector2F{0.0f, 0.5f});

        if (distToGoal < 0.15f || (distToGoal < 0.25f && dist(rng) < 0.2f))
        {
          // Shoot! (Must be close to goal, or occasionally a long shot)
          ball.lastPossessor = ball.possessedBy;
          ball.passCooldown = 0.5f;
          ball.possessedBy = nullptr;

          Vector2F toGoal =
              isHome ? Vector2F{1.0f - it->position.x, 0.5f - it->position.y}
                     : Vector2F{0.0f - it->position.x, 0.5f - it->position.y};
          Vector2F norm = normalize(toGoal);

          float ovr = it->player->getOverall(statsConfig) / 100.0f;
          // Add inaccuracy
          float errorMargin = 0.3f - ovr * 0.25f;
          norm.y += (dist(rng) - 0.5f) * errorMargin;
          norm.x += (dist(rng) - 0.5f) * (errorMargin * 0.5f);
          norm = normalize(norm);

          // Scale shot power
          float shotSpeed = 1.0f + ovr * 0.5f;
          ball.velocity = {norm.x * shotSpeed, norm.y * shotSpeed};
          logEvent(it->player->getName() + " shoots!");
        }
        else
        {
          // Pass to someone further forward or safe sideways pass
          const MatchPlayer* target = nullptr;
          float bestScore = -1.0f;
          for (const auto& other : players)
          {
            if (other.isHomeTeam == isHome && other.player != it->player)
            {
              float dx = isHome ? (other.position.x - it->position.x)
                                : (it->position.x - other.position.x);

              // Prefer players who are ahead or slightly sideways
              if (dx > -0.1f)
              {
                float d = distance(it->position, other.position);
                if (d > 0.1f && d < 0.5f)
                {
                  // Score based on distance and how far forward they are
                  float score = (dx * 2.0f) + (1.0f / d);
                  if (score > bestScore)
                  {
                    bestScore = score;
                    target = &other;
                  }
                }
              }
            }
          }
          if (target)
          {
            ball.lastPossessor = ball.possessedBy;
            ball.passCooldown = 0.5f;
            ball.possessedBy = nullptr;

            // Lead the pass slightly
            Vector2F toTarget = {
                target->position.x + target->velocity.x * 0.5f - it->position.x,
                target->position.y + target->velocity.y * 0.5f -
                    it->position.y};
            Vector2F norm = normalize(toTarget);

            float ovr = it->player->getOverall(statsConfig) / 100.0f;
            float errorMargin = 0.2f - ovr * 0.15f;
            norm.y += (dist(rng) - 0.5f) * errorMargin;
            norm.x += (dist(rng) - 0.5f) * (errorMargin * 0.5f);
            norm = normalize(norm);

            float passSpeed = 0.8f + ovr * 0.4f;
            ball.velocity = {norm.x * passSpeed, norm.y * passSpeed};
            logEvent(it->player->getName() + " passes to " +
                     target->player->getName());
          }
        }
      }
    }
  }
  else
  {
    // Find closest player to grab the ball
    const MatchPlayer* closest = nullptr;
    float minDist = 0.03f;  // Grab radius

    for (auto& mp : players)
    {
      if (mp.tackleCooldown > 0.0f)
      {
        mp.tackleCooldown -= dt;
        continue;
      }
      if (ball.passCooldown > 0.0f && mp.player == ball.lastPossessor) continue;

      float dist = distance(mp.position, ball.position);
      if (dist < minDist)
      {
        minDist = dist;
        closest = &mp;
      }
    }

    if (closest)
    {
      ball.possessedBy = closest->player;
      ball.lastPossessor = closest->player;
      ball.passCooldown = 0.0f;
      logEvent(closest->player->getName() + " controls the ball.");
    }
  }

  // Tackle logic: If an opponent is very close to the ball carrier, they can
  // try to tackle
  if (ball.possessedBy)
  {
    auto carrierIt =
        std::ranges::find_if(players, [this](const MatchPlayer& mp)
                             { return mp.player == ball.possessedBy; });
    if (carrierIt != players.end())
    {
      for (const auto& defender : players)
      {
        if (defender.isHomeTeam == carrierIt->isHomeTeam) continue;
        if (defender.tackleCooldown > 0.0f) continue;

        float dist = distance(defender.position, carrierIt->position);
        if (dist < 0.04f)
        {  // Tackle range
          static std::mt19937 rng(1337);
          std::uniform_real_distribution<float> distRand(0.0f, 1.0f);

          float defOvr = defender.player->getOverall(statsConfig) / 100.0f;
          float atkOvr = carrierIt->player->getOverall(statsConfig) / 100.0f;

          // Base tackle success rate modified by stats difference
          float tackleChance = 0.3f + (defOvr - atkOvr) * 0.5f;
          tackleChance = std::clamp(tackleChance, 0.05f, 0.8f);

          if (distRand(rng) < tackleChance * dt * 2.0f)
          {  // Tackle check over time
            ball.possessedBy = defender.player;
            ball.lastPossessor = defender.player;
            ball.passCooldown = 0.5f;
            carrierIt->tackleCooldown = 1.0f;  // Carrier is stunned briefly
            logEvent(defender.player->getName() + " tackles the ball from " +
                     carrierIt->player->getName() + "!");
            break;
          }
        }
      }
    }
  }
}

void MatchEngine::checkGoals()
{
  if (ball.position.x >= 0.99f && std::abs(ball.position.y - 0.5f) < 0.1f)
  {
    homeScore++;
    logEvent("GOAL for Home Team!");
    resetPositions(false);
  }
  else if (ball.position.x <= 0.01f && std::abs(ball.position.y - 0.5f) < 0.1f)
  {
    awayScore++;
    logEvent("GOAL for Away Team!");
    resetPositions(true);
  }
}

void MatchEngine::resetPositions(bool homeConceded)
{
  ball.position = {0.5f, 0.5f};
  ball.velocity = {0.0f, 0.0f};
  ball.possessedBy = nullptr;

  for (auto& mp : players)
  {
    mp.position = mp.basePosition;

    // Ensure players start in their own half
    if (mp.isHomeTeam && mp.position.x > 0.49f)
    {
      mp.position.x = 0.49f;
    }
    if (!mp.isHomeTeam && mp.position.x < 0.51f)
    {
      mp.position.x = 0.51f;
    }

    mp.velocity = {0.0f, 0.0f};
  }

  bool teamToStart = homeConceded ? true : false;
  MatchPlayer* closest = nullptr;
  float minDist = 1000.0f;

  for (auto& mp : players)
  {
    if (mp.isHomeTeam == teamToStart && mp.player->getRole() != PlayerRole::GK)
    {
      float d = distance(mp.position, ball.position);
      if (d < minDist)
      {
        minDist = d;
        closest = &mp;
      }
    }
  }

  if (closest)
  {
    ball.possessedBy = closest->player;
    closest->position = ball.position;  // Move them to the center to kick off
  }
}

void MatchEngine::logEvent(const std::string& msg)
{
  events.emplace_back(matchTimeMinutes, msg);
  // Keep only last 10 events
  if (events.size() > 10)
  {
    events.erase(events.begin());
  }
}
