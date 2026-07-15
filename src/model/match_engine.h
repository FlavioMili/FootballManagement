// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <string>
#include <vector>

#include "global/stats_config.h"
#include "global/types.h"
#include "model/lineup.h"
#include "model/strategy.h"

struct MatchPlayer
{
  const Player* player;
  bool isHomeTeam;
  Vector2F position;      // Current actual position on the pitch [0.0, 1.0]
  Vector2F velocity;      // Current movement vector
  Vector2F basePosition;  // Assigned tactical position [0.0, 1.0]

  // Physics properties derived from stats
  float maxSpeed;
  float acceleration;
  float tackleCooldown = 0.0f;
};

struct MatchBall
{
  Vector2F position = {0.5f, 0.5f};
  Vector2F velocity = {0.0f, 0.0f};
  float friction = 0.98f;

  // Possession info
  const Player* possessedBy = nullptr;
  const Player* lastPossessor = nullptr;
  float passCooldown = 0.0f;
};

struct MatchEvent
{
  float timeMinute;
  std::string description;
};

class MatchEngine
{
 public:
  MatchEngine(const Lineup& home_lineup, const Lineup& away_lineup,
              const Strategy& home_strat, const Strategy& away_strat,
              const StatsConfig& config);

  void update(float deltaTime);

  const std::vector<MatchPlayer>& getPlayers() const { return players; }
  const MatchBall& getBall() const { return ball; }
  const std::vector<MatchEvent>& getEvents() const { return events; }

  void substitutePlayer(uint32_t outPlayerId, const Player* inPlayer);

  int getHomeScore() const { return homeScore; }
  int getAwayScore() const { return awayScore; }
  float getMatchTimeMinutes() const { return matchTimeMinutes; }

 private:
  std::vector<MatchPlayer> players;
  MatchBall ball;
  const StatsConfig& statsConfig;
  Strategy homeStrategy;
  Strategy awayStrategy;

  std::vector<MatchEvent> events;

  float matchTimeMinutes = 0.0f;
  int homeScore = 0;
  int awayScore = 0;

  // Constants
  const float PITCH_ASPECT_RATIO = 1.5f;  // Length / Width
  const float TIME_SCALE =
      60.0f;  // 1 real second = 60 simulation seconds (1 minute)

  void initializePlayers(const Lineup& lineup, bool isHomeTeam);

  void calculateForces(float dt);
  void resolvePossessionAndPassing(float dt);
  void checkGoals();
  void resetPositions(bool homeConceded);

  void logEvent(const std::string& msg);
};
