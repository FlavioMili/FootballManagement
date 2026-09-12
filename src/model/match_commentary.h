// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <functional>
#include <string>
#include <string_view>

#include "global/types.h"
#include "model/match_events.h"

/** Names the commentary fills into an event's line. */
struct MatchCommentaryNames
{
  /** Team names; empty falls back to the localised "home/away side". */
  std::string homeTeam;
  std::string awayTeam;
  /** Display name of a player (empty when unknown). */
  std::function<std::string(PlayerID)> player;
};

/**
 * Localised one-line commentary for structured match events. The engine only
 * records what happened (type, side, players, score, detail); every word
 * shown to the user comes from the MATCH_COMMENT_* language keys, so the
 * presentation layer can re-describe events in the current language with the
 * real team names.
 */
namespace MatchCommentary
{
/** Language key of the line for `event` (e.g. "MATCH_COMMENT_CORNER"). */
const char* key(const MatchEvent& event);

/**
 * Fills {team}, {player}, {other}, {home}, {away}, {homeScore}, {awayScore},
 * {minutes}, {homePens} and {awayPens} of `pattern` from `event` and `names`.
 */
std::string fill(std::string_view pattern, const MatchEvent& event,
                 const MatchCommentaryNames& names);

/** The localised line for `event`: fill() of its key's translation. */
std::string describe(const MatchEvent& event, const MatchCommentaryNames& names);
}  // namespace MatchCommentary
