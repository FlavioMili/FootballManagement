// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once
#include <cstdint>
#include <string>
#include "global/types.h"

struct GameDateValue {
  uint16_t year;
  uint8_t month;
  uint8_t day;

  // Let's start the game from July first, so that we have
  // the transfer window and some friendlies before starting
  // the championship.
  GameDateValue(uint16_t y = 2025, uint8_t m = 7, uint8_t d = 1)
      : year(y), month(m), day(d) {}

  // ----------------------
  // Date arithmetic
  // ----------------------
  void nextDay();
  void nextWeek();
  void nextMonth();
  GameDateValue operator[](size_t i) const;
  GameDateValue operator+(size_t days) const;
  GameDateValue operator-(size_t days) const;

  // ----------------------
  // Comparisons
  // ----------------------
  bool operator<(const GameDateValue &other) const;
  bool operator==(const GameDateValue &other) const;

  // ----------------------
  // String/DB conversion
  // ----------------------
  std::string toString() const;
  static GameDateValue fromString(const std::string &str);

  // ----------------------
  // Game logic
  // ----------------------
  SeasonPhase checkPhase() const;
  bool isTransferWindowOpen() const;

 private:
  static bool isLeapYear(uint16_t y);
  static uint8_t daysInMonth(uint8_t m, uint16_t y);
};

// SINGLETON
class GameDate {
 public:
  static GameDate &instance() {
    static GameDate _instance;
    return _instance;
  }

  GameDateValue getCurrent() const { return current; }
  void setCurrent(GameDateValue d) { current = d; }

  void nextDay() { current.nextDay(); }
  void nextWeek() { current.nextWeek(); }
  void nextMonth() { current.nextMonth(); }

 private:
  GameDate() : current(2025, 7, 1) {}
  GameDate(const GameDate &) = delete;
  GameDate &operator=(const GameDate &) = delete;

  GameDateValue current;
};
