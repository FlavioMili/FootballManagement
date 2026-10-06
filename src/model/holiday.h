// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "global/types.h"
#include "model/gamedate.h"

class DatabaseConnection;

/** @brief How long the manager is away. */
enum class HolidayMode : std::uint8_t
{
  UntilDate = 0, /*!< Up to (not including play on) a chosen day. */
  NextMatch,     /*!< Until the day of the next managed fixture. */
  NextDecision,  /*!< Until something needs the manager's answer. */
  WindowEnd,     /*!< Until the day after the (next) transfer deadline. */
  COUNT
};

/** @brief Why the holiday ended. */
enum class HolidayStop : std::uint8_t
{
  Completed = 0, /*!< Reached the chosen date / match / deadline. */
  Dismissed,
  SackingWarning,
  BigBid,
  KeyPlayerInjured,
  InjuryCrisis,
  Decision,
  DayLimit,
  Interrupted /*!< The manager came back early (Stop, or the game closed). */
};

/**
 * @struct HolidayPreferences
 * @brief What stops a holiday early and what the assistant takes over
 * (persisted with the save).
 */
struct HolidayPreferences
{
  bool stop_big_bid = true;
  /** Offers from this fee stop the holiday; 0 = any offer for a key player. */
  std::int64_t big_bid_threshold = 0;
  bool stop_sacking_warning = true;
  bool stop_injury_crisis = true;
  std::uint8_t injury_crisis_count = 5; /*!< Injured squad members. */
  bool stop_key_injury = true;          /*!< A key player out 14+ days. */
  bool assistant_lineup = true;   /*!< Keeps the matchday squad eligible. */
  bool assistant_training = true; /*!< Runs intensity and congested weeks. */
  bool assistant_inbox = true;    /*!< Files routine news as read. */
};

/** @brief A holiday request. */
struct HolidayPlan
{
  HolidayMode mode = HolidayMode::NextMatch;
  GameDateValue until; /*!< UntilDate only. */
  HolidayPreferences preferences;
  int max_days = 366;
};

/** @brief A new offer for a managed player seen during the holiday. */
struct HolidayOffer
{
  PlayerID player_id = 0;
  std::uint32_t fee = 0;
  bool loan = false;
  bool key_player = false;
};

/** @brief A managed player injured during the holiday. */
struct HolidayInjury
{
  PlayerID player_id = 0;
  std::string name;
  std::string injury_key;
  std::uint16_t days = 0;
  bool key_player = false;
};

/** @brief What changed on one simulated day (input of the stop rules). */
struct HolidayDay
{
  bool dismissed = false;
  bool board_warning = false;
  std::vector<HolidayOffer> new_offers;
  std::vector<HolidayInjury> new_injuries;
  int injured = 0;           /*!< Injured squad members now. */
  int injured_at_start = 0;  /*!< ... when the holiday began. */
  bool new_decision = false; /*!< Offer, player request or board message. */
};

/** @brief A managed result during the holiday. */
struct HolidayResult
{
  GameDateValue date;
  TeamID opponent_id = 0;
  bool home = true;
  MatchType type = MatchType::LEAGUE;
  std::uint8_t goals_for = 0;
  std::uint8_t goals_against = 0;
};

/** @brief A transfer in or out of the managed club during the holiday. */
struct HolidayMove
{
  PlayerID player_id = 0;
  std::string name;
  TeamID other_team_id = 0;
  std::uint32_t fee = 0;
  bool incoming = true;
};

/** @brief The assistant's report when the manager returns. */
struct HolidaySummary
{
  bool valid = false;
  GameDateValue start;
  GameDateValue end;
  int days = 0;
  HolidayStop reason = HolidayStop::Completed;
  std::string stop_detail;      /*!< Player concerned, if any. */
  std::int64_t stop_amount = 0; /*!< Fee of the bid that stopped it. */
  std::vector<HolidayResult> results;
  int position_before = 0;
  int position_after = 0;
  int points_before = 0;
  int played_before = 0; /*!< League matches before the holiday. */
  int points_after = 0;
  std::int64_t balance_before = 0;
  std::int64_t balance_after = 0;
  std::int64_t income = 0;
  std::int64_t expenses = 0;
  std::vector<HolidayMove> moves;
  std::vector<HolidayInjury> injuries;
  int messages_filed = 0;
};

/**
 * @namespace Holiday
 * @brief Targets and stop rules of "continue until".
 */
namespace Holiday
{
/** Minimum layoff for a key player's injury to stop the holiday. */
inline constexpr std::uint16_t KEY_INJURY_DAYS = 14;

/**
 * Day the holiday ends on, or nullopt for open-ended modes (NextDecision)
 * and when there is nothing to wait for. @p next_match is the day of the
 * next unplayed managed fixture; @p league that of the managed club, whose
 * country's transfer window counts.
 */
std::optional<GameDateValue> targetDate(
    const HolidayPlan& plan, const GameDateValue& today,
    const std::optional<GameDateValue>& next_match, LeagueID league);

/** Day after the deadline of @p league's open window, or of its next one. */
GameDateValue windowEndDate(LeagueID league, const GameDateValue& today);

/** The first early-stop rule that fires today, most serious first. */
std::optional<HolidayStop> checkStop(const HolidayPlan& plan,
                                     const HolidayDay& day);

/** Language key of @p stop (e.g. "HOLIDAY_STOP_BIG_BID"). */
const char* stopKey(HolidayStop stop);

HolidayPreferences loadPreferences(
    const std::shared_ptr<DatabaseConnection>& db_conn);
void savePreferences(const std::shared_ptr<DatabaseConnection>& db_conn,
                     const HolidayPreferences& preferences);
}  // namespace Holiday
