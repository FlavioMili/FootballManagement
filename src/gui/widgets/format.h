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

/** @brief Compact currency, e.g. "€12.4M", "€850K", "-€3.1M". */
std::string money(int64_t amount);

/** @brief Full currency with thousands separators, e.g. "€1,250,000". */
std::string moneyFull(int64_t amount);

/** @brief Integer with thousands separators, e.g. "38,500". */
std::string thousands(int64_t value);

/** @brief Localised short date, e.g. "12 Jul 2025". */
std::string date(const GameDateValue& value);

/** @brief Localised short date without the year, e.g. "12 Jul". */
std::string dayMonth(const GameDateValue& value);

/** @brief Signed integer, e.g. "+4", "-2", "0". */
std::string signedInt(int value);

}  // namespace Format
