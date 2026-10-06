// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "global/types.h"
#include "model/continental.h"
#include "model/gamedate.h"

class Calendar;
class GameData;
struct InboxMessage;

/** @brief One tie (or one league-phase opponent) shown by a draw ceremony. */
struct DrawReveal
{
  TeamID home_id = 0;
  TeamID away_id = 0;
  GameDateValue date; /*!< The match, or the first leg. */
  std::optional<GameDateValue> second_leg;
  std::uint8_t pot = 0; /*!< League phase: the opponent's pot (1-based). */
};

/**
 * @brief A draw the game has already made, laid out for its ceremony.
 *
 * Built on demand from the calendar and the continental seasons; building
 * one never changes the game. Names are in the current language.
 */
struct DrawCeremony
{
  enum class Kind : std::uint8_t
  {
    DomesticCup,
    ContinentalKnockout,
    ContinentalLeaguePhase
  };

  Kind kind = Kind::DomesticCup;
  LeagueID competition_id = 0;
  /** Cup round (1-based), or the continental Round as a number. */
  std::uint8_t stage = 0;
  std::string competition_name;
  std::string round_name;
  GameDateValue drawn_on;
  /** League phase: the club whose opponents are drawn; otherwise the club
   * whose tie is highlighted (0: none). */
  TeamID focus_team = 0;
  std::vector<DrawReveal> reveals; /*!< In the order they are revealed. */
};

/**
 * @brief Builds draw ceremonies from draws already made.
 *
 * Domestic cup ties are revealed in a seeded order that depends only on the
 * season, the cup and the round (the calendar does not keep the order the
 * balls came out); continental knockout ties follow the stored draw order;
 * a league-phase draw reveals one club's opponents pot by pot, home first.
 */
namespace DrawCeremonies
{
/** Seconds between two reveals at normal speed. */
inline constexpr float REVEAL_SECONDS = 0.9f;

/** A drawn round of the cup of @p root (nullopt before its draw). */
std::optional<DrawCeremony> cupRound(const Calendar& calendar,
                                     const GameData& gamedata, LeagueID root,
                                     std::uint8_t stage, TeamID focus_team);

/** The latest round of the cup of @p root drawn on or before @p today. */
std::optional<DrawCeremony> latestCupRound(const Calendar& calendar,
                                           const GameData& gamedata,
                                           LeagueID root,
                                           const GameDateValue& today,
                                           TeamID focus_team);

/**
 * A continental draw of the season of @p season_year. For the league
 * phase, @p focus_team picks whose opponents are revealed (the top seed
 * when it is not an entrant).
 */
std::optional<DrawCeremony> continentalRound(
    const ContinentalCompetitions& continental, const Calendar& calendar,
    LeagueID competition_id, std::uint16_t season_year,
    Continental::Round round, TeamID focus_team);

/** The latest continental draw made on or before @p today, if any. */
std::optional<DrawCeremony> latestContinentalRound(
    const ContinentalCompetitions& continental, const Calendar& calendar,
    LeagueID competition_id, const GameDateValue& today, TeamID focus_team);

/** Whether a message announces a draw a ceremony can show. */
bool announcesDraw(const InboxMessage& message);

/**
 * The draw a continental draw message announces (made on the message's
 * day), or nullopt when it can no longer be found.
 */
std::optional<DrawCeremony> forMessage(
    const InboxMessage& message, const ContinentalCompetitions& continental,
    const Calendar& calendar, TeamID focus_team);

/**
 * Reveals shown @p elapsed seconds into a ceremony of @p total reveals at
 * @p speed (1 = normal). Reduced motion shows everything at once.
 */
std::size_t shownAt(float elapsed, float speed, std::size_t total,
                    bool reduced_motion);
}  // namespace DrawCeremonies
