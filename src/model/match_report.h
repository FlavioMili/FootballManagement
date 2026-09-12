// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "gamedate.h"
#include "global/types.h"

class Lineup;
class MatchEngine;

/** @brief Kind of a structured, persisted match event. */
enum class MatchEventKind : uint8_t
{
  GOAL = 0,
  OWN_GOAL = 1,
  YELLOW_CARD = 3,
  RED_CARD = 4,       /*!< Straight red card. */
  SECOND_YELLOW = 5   /*!< Second caution, player sent off. */
};

/**
 * @brief One structured event of a finished match (scorer, card...).
 *
 * `home` is the side of `player`; an OWN_GOAL therefore counts for the other
 * side. `minute` is the match clock; when `added_minute` > 0 the event
 * happened in added time ("45+2'" is minute 47 with added_minute 2).
 */
struct MatchReportEvent
{
  uint8_t minute = 0;
  uint8_t added_minute = 0;
  MatchEventKind kind = MatchEventKind::GOAL;
  bool home = true;
  PlayerID player = 0;  /*!< 0 when the engine did not attribute it. */
  PlayerID assist = 0;  /*!< 0 when there was no (known) assist. */
};

/** @brief Team totals of a finished match. */
struct TeamMatchStats
{
  uint16_t shots = 0;
  uint16_t shots_on_target = 0;
  uint16_t corners = 0;
  uint16_t fouls = 0;
  uint16_t yellow_cards = 0;
  uint16_t red_cards = 0;
  uint16_t offsides = 0;
  uint16_t saves = 0;
  uint16_t passes_attempted = 0;
  uint16_t passes_completed = 0;
  float possession = 50.0f;
  float expected_goals = 0.0f;
  /** Shots and goals from set pieces (penalties included); reports saved
   * before they were recorded leave set_pieces_known false. */
  bool set_pieces_known = false;
  uint16_t set_piece_shots = 0;
  uint16_t set_piece_goals = 0;
};

/** @brief Per-player line of a finished match. */
struct PlayerMatchLine
{
  PlayerID player_id = 0;
  TeamID team_id = 0;
  bool started = true;
  uint8_t minutes = 0;
  uint8_t goals = 0;
  uint8_t assists = 0;
  uint8_t yellow_cards = 0;
  uint8_t red_cards = 0;
  float rating = 0.0f; /*!< 0 when the engine produced no rating. */
};

/**
 * @struct MatchReport
 * @brief Structured summary of a played fixture, persisted per fixture.
 *
 * Fixtures are identified by (date, home team, away team). Scores include
 * extra-time goals; penalties are reported separately.
 */
struct MatchReport
{
  GameDateValue date;
  TeamID home_team_id = 0;
  TeamID away_team_id = 0;
  MatchType match_type = MatchType::LEAGUE;
  LeagueID competition_id = 0;
  uint8_t stage = 0;
  uint16_t season = 0;
  uint8_t home_goals = 0;
  uint8_t away_goals = 0;
  bool extra_time = false;
  bool penalties = false;
  uint8_t home_penalties = 0;
  uint8_t away_penalties = 0;
  uint32_t attendance = 0; /*!< 0 until stadiums/attendance are modelled. */
  TeamMatchStats home_stats;
  TeamMatchStats away_stats;
  std::vector<MatchReportEvent> events;
  std::vector<PlayerMatchLine> players;
  /**
   * Play mode: the side the manager played himself on the pitch (empty when
   * the match was watched or simulated) and his share of that side's
   * passes, shots and tackles in [0, 1]. Stored with the team statistics.
   */
  std::optional<bool> played_home;
  float played_share = 0.0f;

  /**
   * Copies the engine's final score, team totals, per-player lines and the
   * scorer/card events.
   */
  void fillFromEngine(const MatchEngine& engine, TeamID home_id,
                      TeamID away_id);

  /**
   * Records the starters of a lineup as having played the full match (used
   * when no per-player engine data exists, e.g. score-only results).
   */
  void addLineupAppearances(const Lineup& lineup, TeamID team_id);

  /**
   * Credits @p goals extra-time goals of one side when extra time was
   * resolved statistically (Competitions::resolveDrawnKnockout, the fallback
   * for a tie recorded level without the engine playing it): each goal is
   * given to a player of @p lineup who finished the match, drawn from @p seed with forwards and good finishers favoured: it
   * counts on his line and becomes a GOAL event between minutes 91 and 120.
   * When the side has nobody left to credit, the event has no player (a team
   * goal).
   */
  void creditExtraTimeGoals(const Lineup& lineup, TeamID team_id, bool home,
                            uint8_t goals, uint32_t seed);

  std::string eventsToJson() const;
  std::string playersToJson() const;
  std::string statsToJson() const;
  void eventsFromJson(const std::string& json);
  void playersFromJson(const std::string& json);
  void statsFromJson(const std::string& json);
};
