// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <vector>

#include "global/types.h"
#include "model/gamedate.h"

class Match;

/** @brief Kinds of dated events in a club's season (display order per day). */
enum class AgendaKind : std::uint8_t
{
  SeasonStart,
  TransferWindowOpens,
  TransferDeadline,
  InternationalBreak,
  WinterBreak,
  BoardReview,
  ContractReminder,
  YouthPreview,
  YouthIntake,
  YouthDecisionDeadline,
  Fixture
};

/** @brief One entry of the season agenda. */
struct AgendaEvent
{
  GameDateValue date;
  AgendaKind kind = AgendaKind::Fixture;
  int span_days = 1; /*!< Breaks: their length in days. */
  // Fixtures only.
  TeamID home_id = 0;
  TeamID away_id = 0;
  MatchType match_type = MatchType::LEAGUE;
  LeagueID competition_id = 0;
  bool played = false;
  std::uint8_t home_score = 0;
  std::uint8_t away_score = 0;
};

/**
 * @brief Everything dated in a club's season, from 1 July to 30 June:
 * the season start (last season's review, new objectives and budgets),
 * fixtures (league, cup, friendlies), transfer windows opening and
 * deadline days, international and winter breaks, the youth intake cycle,
 * monthly board reviews and contract reminders.
 */
namespace SeasonAgenda
{
/** Language key naming @p kind (e.g. "AGENDA_TRANSFER_DEADLINE"). */
const char* kindKey(AgendaKind kind);

/** First day of the season containing @p date (1 July). */
GameDateValue seasonStart(const GameDateValue& date);

/**
 * The agenda of the season starting on 1 July of @p season_start_year for
 * a club with @p fixtures (any order; other seasons' fixtures ignored).
 * Sorted by date, then by kind. Board reviews are only listed when the
 * club has a board to answer to (@p board_reviews).
 */
std::vector<AgendaEvent> build(std::uint16_t season_start_year,
                               const std::vector<Match>& fixtures,
                               bool board_reviews);
}  // namespace SeasonAgenda
