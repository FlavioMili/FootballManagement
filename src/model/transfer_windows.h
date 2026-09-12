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
#include <span>

#include "global/types.h"
#include "model/gamedate.h"

/**
 * Registration periods (transfer windows) of each country.
 *
 * A club can register a player from another club only while the window of
 * its own league is open: the buying club's window decides, so a club whose
 * window has shut can still sell abroad. Players without a club follow the
 * country's free-agent rule. Every country has a mid-year window (the
 * European summer) and a start-of-year window (January); both are fixed
 * day-of-year ranges, the closing day (deadline day) included.
 */
namespace TransferWindows
{
/** A calendar day of any year. */
struct MonthDay
{
  std::uint8_t month = 1;
  std::uint8_t day = 1;
};

/** One window: opens on @c opens and shuts after @c closes (deadline). */
struct Period
{
  MonthDay opens;
  MonthDay closes;
};

/** Free agents can join on any day of the year. */
inline constexpr std::int16_t FREE_AGENTS_ANY_TIME = -1;

/** Registration rules of one country (its first and second division). */
struct CountryRules
{
  std::array<LeagueID, 2> leagues{}; /*!< Ids in leagues.json. */
  Period mid_year;
  Period start_of_year;
  /** Days after a window shuts during which free agents may still join
   * (FREE_AGENTS_ANY_TIME: no limit). Inside a window they always can. */
  std::int16_t free_agent_days = FREE_AGENTS_ANY_TIME;
};

/** Every country with its own rules (leagues.json ids). */
std::span<const CountryRules> countries();

/** Rules of @p league's country; a common European calendar otherwise. */
const CountryRules& rulesFor(LeagueID league);

/** True when clubs of @p league can sign players from other clubs. */
bool isOpen(LeagueID league, const GameDateValue& date);

/** True when the window of at least one country is open. */
bool isOpenAnywhere(const GameDateValue& date);

/** True on a day of the start-of-year (January) window of @p league. */
bool isStartOfYearWindow(LeagueID league, const GameDateValue& date);

/** Deadline day (last open day) of the window open on @p date; nullopt when
 * the window of @p league is shut. */
std::optional<GameDateValue> windowEnd(LeagueID league,
                                       const GameDateValue& date);

/** True on the last day of a window of @p league. */
bool isDeadlineDay(LeagueID league, const GameDateValue& date);

/** First day on or after @p date on which the window of @p league is open. */
GameDateValue nextOpening(LeagueID league, const GameDateValue& date);

/** True when a club of @p league can sign a player without a club. */
bool canSignFreeAgent(LeagueID league, const GameDateValue& date);
}  // namespace TransferWindows
