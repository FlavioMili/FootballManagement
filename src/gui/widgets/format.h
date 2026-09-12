// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <string>

#include "model/gamedate.h"

/**
 * @brief Text formatting helpers shared by management screens.
 */
namespace Format
{

/**
 * @brief Compact currency in the current language, e.g. "€12.4M", "€850K",
 * "-€3.1M"; Italian "€ 12,4 mln", "€ 850 mila".
 */
std::string money(int64_t amount);

/**
 * @brief Full currency with thousands separators, e.g. "€1,250,000";
 * Italian "€ 1.250.000".
 */
std::string moneyFull(int64_t amount);

/** @brief Integer with thousands separators, e.g. "38,500" / "38.500". */
std::string thousands(int64_t value);

/** @brief Fixed decimals with the language's mark, e.g. "7.5" / "7,5". */
std::string decimal(double value, int digits);

/** @brief Localised short date, e.g. "12 Jul 2025". */
std::string date(const GameDateValue& value);

/** @brief Localised short date without the year, e.g. "12 Jul". */
std::string dayMonth(const GameDateValue& value);

/** @brief Localised short weekday, e.g. "Sun". */
const char* weekday(const GameDateValue& value);

/** @brief Kick-off time (minutes after midnight), e.g. "20:45". */
std::string kickoff(uint16_t minutes);

/** @brief Weekday, short date and kick-off, e.g. "Sun 17 Aug 20:45". */
std::string matchDay(const GameDateValue& value, uint16_t kickoff_minutes);

/**
 * @brief Localised pattern for a count: KEY_ONE when count is 1 and such a
 * translation exists, otherwise KEY. Avoids "1 days" style texts.
 */
const char* plural(const char* key, int64_t count);

/** @brief Signed integer, e.g. "+4", "-2", "0". */
std::string signedInt(int value);

}  // namespace Format
