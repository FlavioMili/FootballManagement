// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "gui/view_models/match_clock.h"

#include <algorithm>
#include <cmath>
#include <format>

#include "model/match_report.h"

namespace
{
constexpr int HALF_TIME_MINUTE = 45;
constexpr int FULL_TIME_MINUTE = 90;
}  // namespace

std::string MatchClock::minuteLabel(float matchMinutes, int period,
                                    bool addedTime)
{
  const int regulationEnd = period >= 2 ? FULL_TIME_MINUTE : HALF_TIME_MINUTE;
  if (addedTime)
  {
    const int added = std::max(
        1, static_cast<int>(
               std::floor(matchMinutes - static_cast<float>(regulationEnd))) +
               1);
    return std::format("{}+{}'", regulationEnd, added);
  }
  const int minute =
      matchMinutes <= 0.0f
          ? 0
          : std::min(regulationEnd,
                     static_cast<int>(std::floor(matchMinutes)) + 1);
  return std::format("{}'", minute);
}

std::string MatchClock::clockLabel(float matchMinutes, int period,
                                   bool addedTime)
{
  const int regulationEnd = period >= 2 ? FULL_TIME_MINUTE : HALF_TIME_MINUTE;
  const float shown =
      std::max(0.0f, addedTime ? matchMinutes - static_cast<float>(regulationEnd)
                               : matchMinutes);
  const int totalSeconds = static_cast<int>(std::floor(shown * 60.0f));
  if (addedTime)
    return std::format("{}+{}:{:02}", regulationEnd, totalSeconds / 60,
                       totalSeconds % 60);
  return std::format("{:02}:{:02}", totalSeconds / 60, totalSeconds % 60);
}

std::string MatchClock::minuteLabel(const MatchReportEvent& event)
{
  // The event stores the whole minutes elapsed on the clock and, in added
  // time, the whole minutes beyond the regulation end.
  if (event.added_minute > 0)
    return std::format("{}+{}'", event.minute - event.added_minute,
                       event.added_minute + 1);
  return std::format("{}'", event.minute + 1);
}
