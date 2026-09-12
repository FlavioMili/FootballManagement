// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

/**
 * @brief Numbers and money in the conventions of the current language.
 *
 * The marks and money units come from the language file (NUMBER_* and
 * MONEY_* keys), so English shows "€1.5M" and "1,250,000" while Italian
 * shows "€ 1,5 mln" and "1.250.000". Parsing accepts both conventions and
 * refuses input whose separators could mean two different amounts.
 */
namespace NumberFormat
{

/** @brief Separators and money units of one language. */
struct Locale
{
  std::string decimal_mark = ".";
  std::string group_mark = ",";
  /** "{0}" is the number with its unit, e.g. "€{0}" or "€ {0}". */
  std::string money_pattern = "€{0}";
  std::string thousand_unit = "K";
  std::string million_unit = "M";
  std::string billion_unit = "B";

  /** @brief The English conventions (also the defaults above). */
  static Locale english() { return {}; }
};

/** @brief Conventions of the language loaded now. */
const Locale& current();

/** @brief Integer with grouping marks, e.g. "38,500" / "38.500". */
std::string grouped(int64_t value, const Locale& locale = current());

/** @brief Fixed decimals with the decimal mark, e.g. "7.5" / "7,5". */
std::string decimal(double value, int digits, const Locale& locale = current());

/** @brief Compact money, e.g. "€12.4M", "€850K", "-€3.1M", "€ 1,5 mln". */
std::string money(int64_t amount, const Locale& locale = current());

/** @brief Full money, e.g. "€1,250,000" / "€ 1.250.000". */
std::string moneyFull(int64_t amount, const Locale& locale = current());

/** @brief Why a money text was refused. */
enum class MoneyError : uint8_t
{
  NONE,
  EMPTY,     /**< Nothing but blanks or a currency sign. */
  INVALID,   /**< Letters, signs or symbols that are not part of an amount. */
  AMBIGUOUS, /**< Separators that could mean two different amounts. */
  TOO_LARGE, /**< Beyond any amount the game can hold. */
};

/** @brief Result of parseMoney(): a value, or the reason it has none. */
struct MoneyParse
{
  std::optional<int64_t> value;
  MoneyError error = MoneyError::NONE;
};

/**
 * @brief Reads a typed amount: "1,5M", "1.5M", "1.500.000", "1,500,000",
 * "1500k", "€ 2,25 mln", "850 mila", "3 mld".
 *
 * A mark followed by one or two digits, or the only mark before a unit, is
 * a decimal mark; marks between groups of three digits group thousands.
 * "14.500" and "14,500" are 14,500 in both languages (money has no third
 * decimal). A single mark before three digits and a unit ("1.500k") is a
 * decimal mark only when it is the language's own; otherwise the text is
 * refused as ambiguous. Grouped numbers with a unit are refused as well.
 */
MoneyParse parseMoney(std::string_view text, const Locale& locale = current());

/** @brief Language key explaining a parse error (nullptr for NONE). */
const char* errorKey(MoneyError error);

}  // namespace NumberFormat
