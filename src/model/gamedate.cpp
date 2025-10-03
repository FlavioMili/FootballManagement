// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "gamedate.h"
#include "global/global.h"
#include <iomanip>
#include <stdexcept>
#include <sstream>

/*
 * I know that this code is full of magic numbers but
 * it is understandable enough so I will not change it
 */

/**
 * Cultural moment, I needed to lookup the formula and
 * this is what I found:
 *
 * The Gregorian calendar year is approximately 365.2425 days.
 * If we only used the "divisible by 4" rule, we'd have too many
 * leap years and the calendar would drift about 3 days every 400 years.
 *
 * Seems like you never stop learning when writing code :P
 */
bool GameDateValue::isLeapYear(uint16_t y) {
  return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

uint8_t GameDateValue::daysInMonth(uint8_t m, uint16_t y) {
  if (m == 2 && isLeapYear(y))
    return 29;
  return DAYS_PER_MONTH[m - 1];
}

void GameDateValue::nextDay() {
  day++;
  if (day > daysInMonth(month, year)) {
    day = 1;
    nextMonth();
  }
}

void GameDateValue::nextWeek() {
  for (int i = 0; i < 7; ++i)
    nextDay();
}

void GameDateValue::nextMonth() {
  month++;
  if (month > 12) {
    month = 1;
    year++;
  }
}

GameDateValue GameDateValue::operator[](size_t i) const {
  GameDateValue tmp = *this;
  for (size_t j = 0; j < i; ++j)
    tmp.nextDay();
  return tmp;
}

GameDateValue GameDateValue::operator+(size_t days) const {
  return (*this)[days];
}

GameDateValue GameDateValue::operator-(size_t days) const {
  GameDateValue tmp = *this;
  for (size_t i = 0; i < days; ++i) {
    if (tmp.day > 1)
      tmp.day--;
    else {
      if (tmp.month == 1) {
        tmp.month = 12;
        tmp.year--;
      } else
        tmp.month--;
      tmp.day = daysInMonth(tmp.month, tmp.year);
    }
  }
  return tmp;
}

bool GameDateValue::operator<(const GameDateValue &other) const {
  if (year != other.year)
    return year < other.year;
  if (month != other.month)
    return month < other.month;
  return day < other.day;
}

bool GameDateValue::operator==(const GameDateValue &other) const {
  return year == other.year && month == other.month && day == other.day;
}

std::string GameDateValue::toString() const {
  std::ostringstream oss;
  oss << std::setw(4) << std::setfill('0') << year << "-" << std::setw(2)
      << std::setfill('0') << unsigned(month) << "-" << std::setw(2)
      << std::setfill('0') << unsigned(day);
  return oss.str();
}

GameDateValue GameDateValue::fromString(const std::string &str) {
  if (str.size() != 10)
    throw std::runtime_error("Invalid date string");
  uint16_t y = std::stoi(str.substr(0, 4));
  uint8_t m = std::stoi(str.substr(5, 2));
  uint8_t d = std::stoi(str.substr(8, 2));
  return GameDateValue(y, m, d);
}

SeasonPhase GameDateValue::checkPhase() const {
  if (month == 6) {
    return SeasonPhase::OFF_SEASON;
  }
  if (month == 7 || month == 8) {
    return SeasonPhase::PRE_SEASON;
  }
  if (month == 5) {
    return SeasonPhase::POST_SEASON;  // End of season
  }
  // Sept through April is regular season
  return SeasonPhase::REGULAR_SEASON;
}

bool GameDateValue::isTransferWindowOpen() const {
  // Summer window: June-August
  if (month >= 6 && month <= 8) {
    return true;
  }
  // Winter window: January
  if (month == 1) {
    return true;
  }
  return false;
}
