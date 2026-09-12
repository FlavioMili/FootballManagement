// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "global/types.h"

struct MatchReport;

/**
 * @struct SeasonHistoryEntry
 * @brief Final outcome of one competition in one season.
 *
 * League rows carry champion, runner-up, the clubs promoted out of the league
 * and relegated out of it, and the league top scorer. Cup rows
 * (competition_type == CUP, competition_id == country root league) carry the
 * cup winner as champion and the losing finalist as runner-up.
 */
struct SeasonHistoryEntry
{
  uint16_t season = 0;     /*!< Game season number (1 = first season). */
  uint16_t start_year = 0; /*!< Calendar year the season started. */
  MatchType competition_type = MatchType::LEAGUE;
  LeagueID competition_id = 0;
  /** Fallback name; "@KEY" (a language key) for continental competitions.
   * Screens name leagues and cups from competition_id in the current
   * language. */
  std::string competition_name;
  TeamID champion_id = 0;  /*!< 0 when undecided. */
  TeamID runner_up_id = 0; /*!< 0 when undecided. */
  std::vector<TeamID> promoted;
  std::vector<TeamID> relegated;
  PlayerID top_scorer_id = 0; /*!< 0 when no goals were attributed. */
  uint16_t top_scorer_goals = 0;
};

/**
 * @struct PlayerSeasonStats
 * @brief Competitive (league or cup) totals of a player for one team in one
 * season. Friendlies are not counted.
 */
struct PlayerSeasonStats
{
  uint16_t season = 0;
  PlayerID player_id = 0;
  TeamID team_id = 0;
  MatchType competition_type = MatchType::LEAGUE;
  uint16_t appearances = 0;
  uint16_t starts = 0;
  uint16_t minutes = 0;
  uint16_t goals = 0;
  uint16_t assists = 0;
  uint16_t yellow_cards = 0;
  uint16_t red_cards = 0;
  float rating_total = 0.0f;
  uint16_t rated_matches = 0;

  /** Average match rating; 0 when no rated match was played. */
  float averageRating() const
  {
    return rated_matches == 0 ? 0.0f
                              : rating_total / static_cast<float>(rated_matches);
  }
};

/** Key: season, player, team, competition type. */
using PlayerSeasonKey = std::tuple<uint16_t, PlayerID, TeamID, MatchType>;
using PlayerSeasonTable = std::map<PlayerSeasonKey, PlayerSeasonStats>;

namespace SeasonStats
{
/**
 * @brief Adds the player lines of a competitive match to the season table.
 *
 * Goals/assists/cards are taken from the player lines; when the engine does
 * not attribute them yet, goal events with a known scorer are used instead.
 * Friendlies are ignored.
 */
void accumulate(PlayerSeasonTable& table, const MatchReport& report);

/** Top scorers (goals > 0) among @p team_ids for a season and competition. */
std::vector<PlayerSeasonStats> topScorers(const PlayerSeasonTable& table,
                                          uint16_t season,
                                          MatchType competition_type,
                                          const std::vector<TeamID>& team_ids,
                                          size_t limit);
}  // namespace SeasonStats
