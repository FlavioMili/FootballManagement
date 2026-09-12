// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <string>

struct MatchReportEvent;

/**
 * @brief Broadcast minute labels shared by the live match and its report.
 *
 * The label names the minute being played: "1'" during the first minute,
 * "45'" up to the half, then "45+N'" in first-half added time, "46'".."90'"
 * and "90+N'". Live events and stored report events of the same moment get
 * the same label.
 */
namespace MatchClock
{
/** @brief Label of a match clock reading (minutes since kick-off). */
std::string minuteLabel(float matchMinutes, int period, bool addedTime);

/**
 * @brief Running clock with seconds: "67:23", "45+2:10" in added time
 * (minutes beyond the regulation end of the half).
 */
std::string clockLabel(float matchMinutes, int period, bool addedTime);

/** @brief Label of a stored event (whole minutes elapsed, see its doc). */
std::string minuteLabel(const MatchReportEvent& event);
}  // namespace MatchClock
