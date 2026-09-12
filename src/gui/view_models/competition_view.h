// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "global/types.h"
#include "gui/widgets/widgets.h"
#include "model/gamedate.h"

class GameController;

/**
 * @brief Read-only view models for league tables and fixture lists.
 *
 * Built from the competitions model and the in-memory calendar (no SQL) and
 * cached by the screens until the game state changes.
 */
namespace CompetitionView
{

constexpr size_t FORM_LENGTH = 5;

/** @brief One row of a league table. */
struct StandingRow
{
  TeamID team_id = 0;
  std::string name;
  int played = 0;
  int won = 0;
  int drawn = 0;
  int lost = 0;
  int goals_for = 0;
  int goals_against = 0;
  int points = 0;
  std::array<UI::Outcome, FORM_LENGTH> form{};
  uint8_t form_count = 0; /**< Valid entries in form, oldest first. */

  [[nodiscard]] int goalDifference() const { return goals_for - goals_against; }
};

/** @brief One scheduled or played match. */
struct FixtureRow
{
  GameDateValue date;
  TeamID home_id = 0;
  TeamID away_id = 0;
  std::string home_name;
  std::string away_name;
  MatchType type = MatchType::LEAGUE;
  bool played = false;
  uint8_t home_score = 0;
  uint8_t away_score = 0;
  int round = 0; /**< League round (1-based); 0 for non-league matches. */
  uint16_t kickoff = 0; /**< Minutes after midnight. */
};

/** @brief Number of promotion and relegation places in a league table. */
struct Zones
{
  size_t promotion = 0;
  size_t relegation = 0;
};

/**
 * @brief League table for one league in the competitions model's order
 * (points, goal difference, goals scored, head-to-head, name).
 */
std::vector<StandingRow> buildStandings(const GameController& controller,
                                        LeagueID leagueId);

/** @brief Promotion (top) and relegation (bottom) places of a league. */
Zones zonesFor(const GameController& controller, LeagueID leagueId);

/** @brief All league fixtures of a league in date order, with rounds. */
std::vector<FixtureRow> buildLeagueFixtures(const GameController& controller,
                                            LeagueID leagueId);

/** @brief Every fixture (all competitions) of one club in date order. */
std::vector<FixtureRow> buildClubFixtures(const GameController& controller,
                                          TeamID teamId);

/** @brief The club's next unplayed fixture on or after the current date. */
std::optional<FixtureRow> nextFixture(const GameController& controller,
                                      TeamID teamId);

/** @brief Localisation key naming a competition type. */
const char* matchTypeKey(MatchType type);

/** @brief Outcome of a played fixture from one club's perspective. */
UI::Outcome outcomeFor(const FixtureRow& fixture, TeamID teamId);

}  // namespace CompetitionView
