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
#include <string>
#include <vector>

#include "global/types.h"
#include "model/league.h"

class Calendar;
class ContinentalCompetitions;
class GameData;

/**
 * @brief Run-in maths of a league table: which outcomes are mathematically
 * decided (title, promotion, play-off and continental places, survival) and
 * what a club still needs from its own games.
 *
 * Everything is derived from the fixtures themselves: the played results
 * give the current table and the head-to-head records, the unplayed ones
 * the games still to come. A result is only reported as decided when it
 * holds for every combination of the remaining results and every possible
 * score, under the league's tie-break rule (see Standings::compute).
 */
namespace Standings
{
/** One league fixture as seen by the run-in maths. */
struct RaceFixture
{
  TeamID home_id = 0;
  TeamID away_id = 0;
  bool played = false;
  uint8_t home_goals = 0;
  uint8_t away_goals = 0;
};

/** Clubs (with the names used by the last tie-break), fixtures and rule. */
struct RaceInput
{
  std::vector<TeamID> teams;
  std::vector<std::string> names; /**< Parallel to teams. */
  std::vector<RaceFixture> fixtures;
  TieBreakRule rule = TieBreakRule::GOAL_DIFFERENCE;
};

/**
 * @brief Table places that decide something. Promotion, play-off and
 * continental places count from the top (play-off places follow the
 * promotion places; continental includes continental_top), relegation from
 * the bottom. 0 means the league has none.
 */
struct RacePlaces
{
  uint16_t promotion = 0;
  uint16_t play_off = 0;
  uint16_t continental_top = 0; /**< Places in the best continental cup. */
  uint16_t continental = 0;     /**< Places in any continental cup. */
  uint16_t relegation = 0;
};

/** The races a club can be part of, most ambitious first. */
enum class Race : uint8_t
{
  TITLE,
  PROMOTION,
  PLAY_OFF,
  CONTINENTAL_TOP,
  CONTINENTAL,
  SURVIVAL
};
inline constexpr size_t RACE_COUNT = 6;

enum class RaceState : uint8_t
{
  NONE,    /**< The league has no such places. */
  OPEN,    /**< Still undecided (or too early to prove either way). */
  SECURED, /**< Guaranteed whatever happens. */
  LOST     /**< Out of reach whatever happens. */
};

/** Headline status of a club, the best decided outcome first. */
enum class Clinch : uint8_t
{
  OPEN,
  CHAMPION,
  PROMOTED,
  PLAY_OFF,
  CONTINENTAL_TOP,
  CONTINENTAL,
  SAFE,
  RELEGATED
};

/** Decided and open races of one club. */
struct ClinchReport
{
  TeamID team_id = 0;
  std::array<RaceState, RACE_COUNT> races{};
  Clinch status = Clinch::OPEN;
};

/** What a club needs from its own remaining games in one race. */
struct RaceNeed
{
  enum class Kind : uint8_t
  {
    IN_HANDS,   /**< `points` from its own games guarantee the place. */
    NEEDS_HELP  /**< Still possible, but only with help from elsewhere. */
  };
  Race race = Race::TITLE;
  Kind kind = Kind::IN_HANDS;
  uint16_t points = 0;     /**< Minimum points that guarantee the place. */
  uint16_t games_left = 0; /**< League games the club still plays. */
  TeamID next_opponent = 0;
  bool next_at_home = false;
};

/** Number of places (from the top) that decide a race; 0 = no such race. */
uint16_t raceCutoff(Race race, const RacePlaces& places, size_t clubs);

/** Decided races and headline status of one club. */
ClinchReport clinchReport(const RaceInput& input, const RacePlaces& places,
                          TeamID team);

/** Headline status of one club (see clinchReport). */
Clinch clinchStatus(const RaceInput& input, const RacePlaces& places,
                    TeamID team);

/** clinchReport for every club, in input.teams order. */
std::vector<ClinchReport> clinchReports(const RaceInput& input,
                                        const RacePlaces& places);

/**
 * @brief What @p team needs in its still open races, most ambitious first.
 *
 * Worked out only in the run-in (at most MAX_NEED_GAMES league games left
 * for the club); earlier, or when the search would be too long, the list
 * is empty. Races already decided are left out.
 */
std::vector<RaceNeed> whatYouNeed(const RaceInput& input,
                                  const RacePlaces& places, TeamID team);
inline constexpr uint16_t MAX_NEED_GAMES = 6;

/** Fixtures, clubs and tie-break rule of a league from the calendar. */
RaceInput raceInput(const League& league, const Calendar& calendar,
                    const GameData& gamedata);

/**
 * @brief Deciding places of a league: promotion and relegation as the
 * season-end movements apply them; continental places (top divisions only)
 * from the current association ranking, minus the place the domestic cup
 * winner can take.
 */
RacePlaces racePlaces(const GameData& gamedata,
                      const ContinentalCompetitions* continental,
                      LeagueID league_id);
}  // namespace Standings
