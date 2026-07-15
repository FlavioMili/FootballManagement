// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>

#include "gamedate.h"
#include "global/types.h"

class GameData;

/**
 * @class Match
 * @brief Represents a football match between two teams.
 */
class Match
{
 public:
  /**
   * @brief Constructs a Match object.
   * @param home_id The ID of the home team.
   * @param away_id The ID of the away team.
   * @param date The date of the match.
   * @param type The type of the match (e.g., league, friendly).
   */
  Match(TeamID home_id, TeamID away_id, GameDateValue date, MatchType type);

  /**
   * @brief Simulates the match using the provided game data.
   * @param game_data Reference to the GameData used for simulation.
   */
  void simulate(const GameData& game_data);

  /**
   * @brief Gets the ID of the home team.
   * @return The home team's ID.
   */
  TeamID getHomeTeamId() const;

  /**
   * @brief Gets the ID of the away team.
   * @return The away team's ID.
   */
  TeamID getAwayTeamId() const;

  /**
   * @brief Gets the home team's score.
   * @return The score of the home team.
   */
  uint8_t getHomeScore() const;

  /**
   * @brief Gets the away team's score.
   * @return The score of the away team.
   */
  uint8_t getAwayScore() const;

  /**
   * @brief Gets the type of the match.
   * @return The match type.
   */
  MatchType getMatchType() const;

  /**
   * @brief Gets the date of the match.
   * @return The date value.
   */
  const GameDateValue& getDate() const;

  /**
   * @brief Checks if the match has been played.
   * @return True if the match has been played, false otherwise.
   */
  bool isPlayed() const;

  void setPlayedResult(uint8_t h, uint8_t a);

 private:
  TeamID home_team_id;
  TeamID away_team_id;
  GameDateValue match_date;
  MatchType match_type;
  uint8_t home_score;
  uint8_t away_score;
  bool _played = false;
};
