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

std::string money(int64_t amount)
{
  const double absolute = static_cast<double>(amount < 0 ? -amount : amount);
  const char* sign = amount < 0 ? "-" : "";
  if (absolute >= 1'000'000'000.0)
    return std::format("{}€{:.2f}B", sign, absolute / 1'000'000'000.0);
  if (absolute >= 10'000'000.0)
    return std::format("{}€{:.1f}M", sign, absolute / 1'000'000.0);
  if (absolute >= 1'000'000.0)
    return std::format("{}€{:.2f}M", sign, absolute / 1'000'000.0);
  if (absolute >= 10'000.0)
    return std::format("{}€{:.0f}K", sign, absolute / 1'000.0);
  if (absolute >= 1'000.0)
    return std::format("{}€{:.1f}K", sign, absolute / 1'000.0);
  return std::format("{}€{:.0f}", sign, absolute);
}

std::string thousands(int64_t value)
{
  const uint64_t absolute = value < 0 ? static_cast<uint64_t>(-(value + 1)) + 1
                                      : static_cast<uint64_t>(value);
  std::string digits = std::to_string(absolute);
  for (auto position = static_cast<int>(digits.size()) - 3; position > 0;
       position -= 3)
    digits.insert(static_cast<size_t>(position), ",");
  return value < 0 ? "-" + digits : digits;
}

std::string moneyFull(int64_t amount)
{
  const std::string digits = thousands(amount);
  return amount < 0 ? "-€" + digits.substr(1) : "€" + digits;
}

std::string date(const GameDateValue& value)
{
  return std::format("{} {} {}", value.day, monthName(value.month), value.year);
}

std::string dayMonth(const GameDateValue& value)
{
  return std::format("{} {}", value.day, monthName(value.month));
}

std::string signedInt(int value)
{
  return value > 0 ? std::format("+{}", value) : std::to_string(value);
}

}  // namespace Format
