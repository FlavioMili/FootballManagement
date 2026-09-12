// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "global/number_format.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <format>
#include <vector>

#include "global/language_manager.h"

namespace
{
constexpr std::string_view EURO = "\xE2\x82\xAC";
constexpr double MAX_AMOUNT = 9.0e15;

/** A blank at @p index: its length in bytes (UTF-8 no-break spaces too). */
size_t blankAt(std::string_view text, size_t index)
{
  const std::string_view rest = text.substr(index);
  if (rest.empty()) return 0;
  if (rest.front() == ' ' || rest.front() == '\t') return 1;
  if (rest.starts_with("\xC2\xA0")) return 2;  // no-break space
  if (rest.starts_with("\xE2\x80\xAF") || rest.starts_with("\xE2\x80\x89"))
    return 3;  // narrow no-break and thin space
  return 0;
}

std::string_view trimBlanks(std::string_view text)
{
  while (!text.empty())
  {
    const size_t blank = blankAt(text, 0);
    if (blank == 0) break;
    text.remove_prefix(blank);
  }
  while (!text.empty())
  {
    size_t blank = 0;
    for (const size_t length : {1U, 2U, 3U})
    {
      if (length <= text.size() &&
          blankAt(text, text.size() - length) == length)
        blank = length;
    }
    if (blank == 0) break;
    text.remove_suffix(blank);
  }
  return text;
}

bool isDigit(char character) { return character >= '0' && character <= '9'; }

bool isLetter(char character)
{
  return (character >= 'a' && character <= 'z') ||
         (character >= 'A' && character <= 'Z');
}

char lower(char character)
{
  return character >= 'A' && character <= 'Z'
             ? static_cast<char>(character - 'A' + 'a')
             : character;
}

bool equalsIgnoreCase(std::string_view first, std::string_view second)
{
  if (first.size() != second.size()) return false;
  for (size_t index = 0; index < first.size(); ++index)
    if (lower(first[index]) != lower(second[index])) return false;
  return true;
}

/** Removes "€", "eur" and "euro" before or after the amount. */
std::string_view stripCurrency(std::string_view text)
{
  text = trimBlanks(text);
  for (bool changed = true; changed && !text.empty();)
  {
    changed = false;
    if (text.starts_with(EURO))
    {
      text.remove_prefix(EURO.size());
      changed = true;
    }
    else if (text.ends_with(EURO))
    {
      text.remove_suffix(EURO.size());
      changed = true;
    }
    else
    {
      for (const std::string_view word : {"euro", "eur"})
      {
        if (text.size() < word.size()) continue;
        if (equalsIgnoreCase(text.substr(0, word.size()), word) &&
            (text.size() == word.size() || !isLetter(text[word.size()])))
        {
          text.remove_prefix(word.size());
          changed = true;
          break;
        }
        const size_t start = text.size() - word.size();
        if (equalsIgnoreCase(text.substr(start), word) &&
            (start == 0 || !isLetter(text[start - 1])))
        {
          text.remove_suffix(word.size());
          changed = true;
          break;
        }
      }
    }
    text = trimBlanks(text);
  }
  return text;
}

/** Multiplier of a unit word ("k", "mln", "mila"...), 0 when unknown. */
double unitMultiplier(std::string_view word,
                      const NumberFormat::Locale& locale)
{
  struct Unit
  {
    std::string_view word;
    double multiplier;
  };
  static constexpr std::array<Unit, 17> UNITS = {{
      {"k", 1e3},          {"mila", 1e3},     {"thousand", 1e3},
      {"m", 1e6},          {"mln", 1e6},      {"mio", 1e6},
      {"milione", 1e6},    {"milioni", 1e6},  {"million", 1e6},
      {"millions", 1e6},   {"b", 1e9},        {"bn", 1e9},
      {"mld", 1e9},        {"miliardo", 1e9}, {"miliardi", 1e9},
      {"billion", 1e9},    {"billions", 1e9},
  }};
  for (const Unit& unit : UNITS)
    if (equalsIgnoreCase(word, unit.word)) return unit.multiplier;
  // The language's own units, as shown by money().
  if (equalsIgnoreCase(word, trimBlanks(locale.thousand_unit))) return 1e3;
  if (equalsIgnoreCase(word, trimBlanks(locale.million_unit))) return 1e6;
  if (equalsIgnoreCase(word, trimBlanks(locale.billion_unit))) return 1e9;
  return 0.0;
}

std::string applyPattern(const std::string& pattern, const std::string& body)
{
  const size_t slot = pattern.find("{0}");
  if (slot == std::string::npos) return pattern + body;
  return pattern.substr(0, slot) + body + pattern.substr(slot + 3);
}

uint64_t magnitude(int64_t value)
{
  return value < 0 ? static_cast<uint64_t>(-(value + 1)) + 1
                   : static_cast<uint64_t>(value);
}

std::string groupedMagnitude(uint64_t value, const std::string& mark)
{
  std::string digits = std::to_string(value);
  for (auto position = static_cast<int>(digits.size()) - 3; position > 0;
       position -= 3)
    digits.insert(static_cast<size_t>(position), mark);
  return digits;
}

using NumberFormat::MoneyError;
using NumberFormat::MoneyParse;

MoneyParse failure(MoneyError error) { return {std::nullopt, error}; }
}  // namespace

namespace NumberFormat
{

const Locale& current() { return LanguageManager::instance().numberLocale(); }

std::string grouped(int64_t value, const Locale& locale)
{
  const std::string digits = groupedMagnitude(magnitude(value), locale.group_mark);
  return value < 0 ? "-" + digits : digits;
}

std::string decimal(double value, int digits, const Locale& locale)
{
  std::string text = std::format("{:.{}f}", value, std::max(digits, 0));
  if (locale.decimal_mark != ".")
    if (const size_t point = text.find('.'); point != std::string::npos)
      text.replace(point, 1, locale.decimal_mark);
  return text;
}

std::string money(int64_t amount, const Locale& locale)
{
  const double absolute = static_cast<double>(magnitude(amount));
  std::string body;
  if (absolute >= 1'000'000'000.0)
    body = decimal(absolute / 1'000'000'000.0, 2, locale) + locale.billion_unit;
  else if (absolute >= 10'000'000.0)
    body = decimal(absolute / 1'000'000.0, 1, locale) + locale.million_unit;
  else if (absolute >= 1'000'000.0)
    body = decimal(absolute / 1'000'000.0, 2, locale) + locale.million_unit;
  else if (absolute >= 10'000.0)
    body = decimal(absolute / 1'000.0, 0, locale) + locale.thousand_unit;
  else if (absolute >= 1'000.0)
    body = decimal(absolute / 1'000.0, 1, locale) + locale.thousand_unit;
  else
    body = decimal(absolute, 0, locale);
  return (amount < 0 ? "-" : "") + applyPattern(locale.money_pattern, body);
}

std::string moneyFull(int64_t amount, const Locale& locale)
{
  return (amount < 0 ? "-" : "") +
         applyPattern(locale.money_pattern,
                      groupedMagnitude(magnitude(amount), locale.group_mark));
}

MoneyParse parseMoney(std::string_view text, const Locale& locale)
{
  text = stripCurrency(text);
  if (text.empty()) return failure(MoneyError::EMPTY);

  // The number: digits and marks, with blanks only between digits.
  size_t end = 0;
  while (end < text.size())
  {
    const char character = text[end];
    if (isDigit(character) || character == '.' || character == ',' ||
        character == '\'')
    {
      ++end;
      continue;
    }
    size_t blank = blankAt(text, end);
    if (blank == 0) break;
    size_t next = end + blank;
    while (next < text.size() && (blank = blankAt(text, next)) > 0)
      next += blank;
    if (end == 0 || next >= text.size() || !isDigit(text[next]) ||
        !isDigit(text[end - 1]))
      break;
    end = next;
  }
  const std::string_view number = text.substr(0, end);
  const std::string_view unit = trimBlanks(text.substr(end));

  double multiplier = 1.0;
  if (!unit.empty())
  {
    multiplier = unitMultiplier(unit, locale);
    if (multiplier == 0.0) return failure(MoneyError::INVALID);
  }

  // Digit runs and the single mark between each pair (' ' for blanks).
  std::vector<std::string> runs(1);
  std::vector<char> marks;
  for (size_t index = 0; index < number.size();)
  {
    const char character = number[index];
    if (isDigit(character))
    {
      runs.back() += character;
      ++index;
      continue;
    }
    char mark = character;
    if (const size_t blank = blankAt(number, index); blank > 0)
    {
      mark = ' ';
      while (index < number.size() && blankAt(number, index) > 0)
        index += blankAt(number, index);
    }
    else
    {
      ++index;
    }
    // A leading decimal mark (".5M") is fine; two marks in a row are not.
    const bool leadingDecimal =
        marks.empty() && runs.back().empty() && (mark == '.' || mark == ',');
    if (runs.back().empty() && !leadingDecimal)
      return failure(MoneyError::AMBIGUOUS);
    marks.push_back(mark);
    runs.emplace_back();
  }
  if (number.empty()) return failure(MoneyError::INVALID);
  if (runs.back().empty()) return failure(MoneyError::AMBIGUOUS);

  // Which mark, if any, is the decimal mark.
  std::optional<size_t> decimalMark;
  size_t dots = 0;
  size_t commas = 0;
  std::optional<size_t> lastPoint;
  for (size_t index = 0; index < marks.size(); ++index)
  {
    if (marks[index] == '.') ++dots;
    if (marks[index] == ',') ++commas;
    if (marks[index] == '.' || marks[index] == ',') lastPoint = index;
  }
  const bool hasUnit = multiplier != 1.0;
  if (dots > 0 && commas > 0)
  {
    // "1.234.567,89" or "1,234,567.89": the last kind is the decimal mark.
    const char kind = marks[*lastPoint];
    if (*lastPoint + 1 != marks.size() || (kind == '.' ? dots : commas) > 1)
      return failure(MoneyError::AMBIGUOUS);
    decimalMark = lastPoint;
  }
  else if (dots + commas == 1 && *lastPoint + 1 == marks.size())
  {
    const size_t after = runs[*lastPoint + 1].size();
    const bool couldGroup = after == 3 && !runs[*lastPoint].empty();
    if (!couldGroup)
      decimalMark = lastPoint;
    else if (hasUnit)
    {
      // "1.500k": 1.5K in English, 1.5M in Italian. Only the language's
      // own decimal mark settles it.
      if (locale.decimal_mark.size() != 1 ||
          marks[*lastPoint] != locale.decimal_mark.front())
        return failure(MoneyError::AMBIGUOUS);
      decimalMark = lastPoint;
    }
  }

  // Thousands groups: 1-3 digits, then groups of exactly three.
  const size_t integerRuns = decimalMark ? *decimalMark + 1 : runs.size();
  if (integerRuns > 1)
  {
    if (hasUnit) return failure(MoneyError::AMBIGUOUS);
    if (runs.front().empty() || runs.front().size() > 3)
      return failure(MoneyError::AMBIGUOUS);
    for (size_t index = 1; index < integerRuns; ++index)
      if (runs[index].size() != 3) return failure(MoneyError::AMBIGUOUS);
  }
  std::string digits;
  for (size_t index = 0; index < integerRuns; ++index) digits += runs[index];
  if (decimalMark)
  {
    const std::string& fraction = runs.back();
    // Euros have cents at most; "1,2345" says something else.
    if (!hasUnit && fraction.size() > 2) return failure(MoneyError::AMBIGUOUS);
    digits += '.';
    digits += fraction;
  }
  if (digits.size() > 18) return failure(MoneyError::TOO_LARGE);
  char* parsed = nullptr;
  const double amount = std::strtod(digits.c_str(), &parsed) * multiplier;
  if (parsed != digits.c_str() + digits.size() || !std::isfinite(amount))
    return failure(MoneyError::INVALID);
  if (amount > MAX_AMOUNT) return failure(MoneyError::TOO_LARGE);
  return {std::llround(amount), MoneyError::NONE};
}

const char* errorKey(MoneyError error)
{
  switch (error)
  {
    case MoneyError::NONE:
      return nullptr;
    case MoneyError::EMPTY:
      return "WIDGET_MONEY_ERROR_EMPTY";
    case MoneyError::INVALID:
      return "WIDGET_MONEY_ERROR_INVALID";
    case MoneyError::AMBIGUOUS:
      return "WIDGET_MONEY_ERROR_AMBIGUOUS";
    case MoneyError::TOO_LARGE:
      return "WIDGET_MONEY_ERROR_TOO_LARGE";
  }
  return "WIDGET_MONEY_ERROR_INVALID";
}

}  // namespace NumberFormat
