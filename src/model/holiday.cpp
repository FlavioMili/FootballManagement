// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "model/holiday.h"

#include <algorithm>

#include "database/sqlite_rows.h"
#include "model/calendar.h"
#include "model/transfer_negotiation.h"

namespace
{
/** A window opens at most this far ahead (winter window from summer). */
constexpr int WINDOW_SEARCH_DAYS = 370;
}  // namespace

namespace Holiday
{
GameDateValue windowEndDate(const GameDateValue& today)
{
  for (int offset = 0; offset < WINDOW_SEARCH_DAYS; ++offset)
  {
    const GameDateValue day = SeasonCalendar::addDays(today, offset);
    const TransferNegotiation::WindowInfo window =
        TransferNegotiation::windowInfo(day);
    if (window.open && window.days_to_deadline >= 0)
      return SeasonCalendar::addDays(day, window.days_to_deadline + 1);
  }
  return SeasonCalendar::addDays(today, WINDOW_SEARCH_DAYS);
}

std::optional<GameDateValue> targetDate(
    const HolidayPlan& plan, const GameDateValue& today,
    const std::optional<GameDateValue>& next_match)
{
  switch (plan.mode)
  {
    case HolidayMode::UntilDate:
      if (!(today < plan.until)) return std::nullopt;
      return plan.until;
    case HolidayMode::NextMatch:
      if (!next_match || !(today < *next_match)) return std::nullopt;
      return next_match;
    case HolidayMode::WindowEnd:
      return windowEndDate(today);
    case HolidayMode::NextDecision:
    case HolidayMode::COUNT:
      break;
  }
  return std::nullopt;
}

std::optional<HolidayStop> checkStop(const HolidayPlan& plan,
                                     const HolidayDay& day)
{
  const HolidayPreferences& rules = plan.preferences;
  if (day.dismissed) return HolidayStop::Dismissed;
  if (rules.stop_sacking_warning && day.board_warning)
    return HolidayStop::SackingWarning;
  if (rules.stop_big_bid)
    for (const HolidayOffer& offer : day.new_offers)
    {
      if (offer.loan) continue;
      const bool big =
          rules.big_bid_threshold > 0
              ? static_cast<std::int64_t>(offer.fee) >= rules.big_bid_threshold
              : offer.key_player;
      if (big) return HolidayStop::BigBid;
    }
  if (rules.stop_key_injury)
    for (const HolidayInjury& injury : day.new_injuries)
      if (injury.key_player && injury.days >= KEY_INJURY_DAYS)
        return HolidayStop::KeyPlayerInjured;
  if (rules.stop_injury_crisis && day.injured > day.injured_at_start &&
      day.injured >= static_cast<int>(rules.injury_crisis_count))
    return HolidayStop::InjuryCrisis;
  if (plan.mode == HolidayMode::NextDecision && day.new_decision)
    return HolidayStop::Decision;
  return std::nullopt;
}

const char* stopKey(HolidayStop stop)
{
  switch (stop)
  {
    case HolidayStop::Completed:
      return "HOLIDAY_STOP_COMPLETED";
    case HolidayStop::Dismissed:
      return "HOLIDAY_STOP_DISMISSED";
    case HolidayStop::SackingWarning:
      return "HOLIDAY_STOP_WARNING";
    case HolidayStop::BigBid:
      return "HOLIDAY_STOP_BIG_BID";
    case HolidayStop::KeyPlayerInjured:
      return "HOLIDAY_STOP_KEY_INJURY";
    case HolidayStop::InjuryCrisis:
      return "HOLIDAY_STOP_INJURY_CRISIS";
    case HolidayStop::Decision:
      return "HOLIDAY_STOP_DECISION";
    case HolidayStop::DayLimit:
      break;
  }
  return "HOLIDAY_STOP_DAY_LIMIT";
}

const char* modeKey(HolidayMode mode)
{
  switch (mode)
  {
    case HolidayMode::UntilDate:
      return "HOLIDAY_MODE_UNTIL_DATE";
    case HolidayMode::NextMatch:
      return "HOLIDAY_MODE_NEXT_MATCH";
    case HolidayMode::NextDecision:
      return "HOLIDAY_MODE_NEXT_DECISION";
    case HolidayMode::WindowEnd:
    case HolidayMode::COUNT:
      break;
  }
  return "HOLIDAY_MODE_WINDOW_END";
}

HolidayPreferences loadPreferences(
    const std::shared_ptr<DatabaseConnection>& db_conn)
{
  using namespace SqliteRows;
  HolidayPreferences preferences;
  forEach(*db_conn,
          "SELECT stop_big_bid, big_bid_threshold, stop_sacking_warning, "
          "stop_injury_crisis, injury_crisis_count, stop_key_injury, "
          "assistant_lineup, assistant_training, assistant_inbox FROM "
          "HolidayPreferences WHERE id = 1;",
          [&](sqlite3_stmt* stmt)
          {
            preferences.stop_big_bid = column<int>(stmt, 0) != 0;
            preferences.big_bid_threshold = column<std::int64_t>(stmt, 1);
            preferences.stop_sacking_warning = column<int>(stmt, 2) != 0;
            preferences.stop_injury_crisis = column<int>(stmt, 3) != 0;
            preferences.injury_crisis_count = static_cast<std::uint8_t>(
                std::clamp(column<int>(stmt, 4), 1, 30));
            preferences.stop_key_injury = column<int>(stmt, 5) != 0;
            preferences.assistant_lineup = column<int>(stmt, 6) != 0;
            preferences.assistant_training = column<int>(stmt, 7) != 0;
            preferences.assistant_inbox = column<int>(stmt, 8) != 0;
          });
  return preferences;
}

void savePreferences(const std::shared_ptr<DatabaseConnection>& db_conn,
                     const HolidayPreferences& preferences)
{
  using namespace SqliteRows;
  const DatabaseConnection& db = *db_conn;
  clearTable(db, "HolidayPreferences");
  sqlite3_stmt* stmt = db.prepareStatement(
      "INSERT INTO HolidayPreferences (id, stop_big_bid, big_bid_threshold, "
      "stop_sacking_warning, stop_injury_crisis, injury_crisis_count, "
      "stop_key_injury, assistant_lineup, assistant_training, "
      "assistant_inbox) VALUES (1, ?, ?, ?, ?, ?, ?, ?, ?, ?);");
  sqlite3_bind_int(stmt, 1, preferences.stop_big_bid ? 1 : 0);
  sqlite3_bind_int64(stmt, 2, preferences.big_bid_threshold);
  sqlite3_bind_int(stmt, 3, preferences.stop_sacking_warning ? 1 : 0);
  sqlite3_bind_int(stmt, 4, preferences.stop_injury_crisis ? 1 : 0);
  sqlite3_bind_int(stmt, 5, preferences.injury_crisis_count);
  sqlite3_bind_int(stmt, 6, preferences.stop_key_injury ? 1 : 0);
  sqlite3_bind_int(stmt, 7, preferences.assistant_lineup ? 1 : 0);
  sqlite3_bind_int(stmt, 8, preferences.assistant_training ? 1 : 0);
  sqlite3_bind_int(stmt, 9, preferences.assistant_inbox ? 1 : 0);
  db.executeStep(stmt);
  sqlite3_finalize(stmt);
}
}  // namespace Holiday
