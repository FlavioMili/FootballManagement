// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "gui/widgets/format.h"

#include <array>
#include <format>

#include "global/language_manager.h"
#include "global/number_format.h"
#include "model/calendar.h"

namespace
{
constexpr std::array<const char*, 12> MONTH_KEYS = {
    "MONTH_JAN", "MONTH_FEB", "MONTH_MAR", "MONTH_APR",
    "MONTH_MAY", "MONTH_JUN", "MONTH_JUL", "MONTH_AUG",
    "MONTH_SEP", "MONTH_OCT", "MONTH_NOV", "MONTH_DEC"};

const char* monthName(uint8_t month)
{
  if (month < 1 || month > MONTH_KEYS.size()) return "";
  return LOC(MONTH_KEYS[month - 1U]);
}
}  // namespace

namespace Format
{

std::string money(int64_t amount) { return NumberFormat::money(amount); }

std::string thousands(int64_t value) { return NumberFormat::grouped(value); }

std::string moneyFull(int64_t amount) { return NumberFormat::moneyFull(amount); }

std::string decimal(double value, int digits)
{
  return NumberFormat::decimal(value, digits);
}

std::string date(const GameDateValue& value)
{
  return std::format("{} {} {}", value.day, monthName(value.month), value.year);
}

std::string dayMonth(const GameDateValue& value)
{
  return std::format("{} {}", value.day, monthName(value.month));
}

const char* weekday(const GameDateValue& value)
{
  static constexpr std::array<const char*, 7> KEYS = {
      "CALENDAR_WEEKDAY_MON", "CALENDAR_WEEKDAY_TUE", "CALENDAR_WEEKDAY_WED",
      "CALENDAR_WEEKDAY_THU", "CALENDAR_WEEKDAY_FRI", "CALENDAR_WEEKDAY_SAT",
      "CALENDAR_WEEKDAY_SUN"};
  return LOC(KEYS[SeasonCalendar::dayOfWeek(value) % KEYS.size()]);
}

std::string kickoff(uint16_t minutes)
{
  return std::format("{:02}:{:02}", minutes / 60, minutes % 60);
}

std::string matchDay(const GameDateValue& value, uint16_t kickoff_minutes)
{
  return std::format("{} {} {}", weekday(value), dayMonth(value),
                     kickoff(kickoff_minutes));
}

const char* plural(const char* key, int64_t count)
{
  if (count != 1) return LOC(key);
  const std::string singular = std::string(key) + "_ONE";
  const char* localized = LOC(singular.c_str());
  // LOC hands back its argument when the key is missing, and that pointer
  // belongs to the temporary above, so fall back explicitly.
  return singular == localized ? LOC(key) : localized;
}

std::string signedInt(int value)
{
  return value > 0 ? std::format("+{}", value) : std::to_string(value);
}

}  // namespace Format
