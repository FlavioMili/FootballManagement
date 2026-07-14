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

#include "global/types.h"

/**
 * @struct GameDateValue
 * @brief Represents a specific date in the game world.
 */
struct GameDateValue
{
  uint16_t year;
  uint8_t month;
  uint8_t day;

  /**
   * @brief Constructs a GameDateValue.
   * @param y Year (default 2025).
   * @param m Month (default 7).
   * @param d Day (default 1).
   *
   * Let's start the game from July first, so that we have
   * the transfer window and some friendlies before starting
   * the championship.
   */
  explicit GameDateValue(uint16_t y = 2025, uint8_t m = 7, uint8_t d = 1)
      : year(y), month(m), day(d)
  {
  }

  // ----------------------
  // Date arithmetic
  // ----------------------
  /** @brief Advances the date by one day. */
  void nextDay();
  /** @brief Advances the date by one week. */
  void nextWeek();
  /** @brief Advances the date by one month. */
  void nextMonth();
  /**
   * @brief Accesses the date part by index (0=year, 1=month, 2=day).
   * @param i Index.
   * @return The corresponding part of the date.
   */
  GameDateValue operator[](size_t i) const;
  /**
   * @brief Adds a specified number of days to the date.
   * @param days Days to add.
   * @return The new date.
   */
  GameDateValue operator+(size_t days) const;
  /**
   * @brief Subtracts a specified number of days from the date.
   * @param days Days to subtract.
   * @return The new date.
   */
  GameDateValue operator-(size_t days) const;

  // ----------------------
  // Comparisons
  // ----------------------
  /**
   * @brief Checks if this date is earlier than another.
   * @param other The date to compare against.
   * @return True if earlier, false otherwise.
   */
  bool operator<(const GameDateValue& other) const;
  /**
   * @brief Checks if this date is equal to another.
   * @param other The date to compare against.
   * @return True if equal, false otherwise.
   */
  bool operator==(const GameDateValue& other) const;

  // ----------------------
  // String/DB conversion
  // ----------------------
  /**
   * @brief Converts the date to a string representation.
   * @return String representation of the date.
   */
  std::string toString() const;
  /**
   * @brief Parses a date from a string.
   * @param str String representation of the date.
   * @return The parsed GameDateValue.
   */
  static GameDateValue fromString(const std::string& str);

  // ----------------------
  // Game logic
  // ----------------------
  /**
   * @brief Checks the current phase of the season.
   * @return The current SeasonPhase.
   */
  SeasonPhase checkPhase() const;
  /**
   * @brief Checks if the transfer window is currently open.
   * @return True if the transfer window is open, false otherwise.
   */
  bool isTransferWindowOpen() const;

 private:
  static bool isLeapYear(uint16_t y);
  static uint8_t daysInMonth(uint8_t m, uint16_t y);
};

/**
 * @class GameDate
 * @brief Singleton class that manages the current date in the game.
 */
class GameDate
{
 public:
  /**
   * @brief Gets the singleton instance.
   * @return Reference to the GameDate instance.
   */
  static GameDate& instance()
  {
    static GameDate _instance;
    return _instance;
  }

  /**
   * @brief Gets the current date.
   * @return The current date.
   */
  GameDateValue getCurrent() const { return current; }
  /**
   * @brief Sets the current date.
   * @param d The date to set.
   */
  void setCurrent(GameDateValue d) { current = d; }

  /** @brief Advances the current date by one day. */
  void nextDay() { current.nextDay(); }
  /** @brief Advances the current date by one week. */
  void nextWeek() { current.nextWeek(); }
  /** @brief Advances the current date by one month. */
  void nextMonth() { current.nextMonth(); }

 private:
  GameDate() : current(2025, 7, 1) {}
  GameDate(const GameDate&) = delete;
  GameDate& operator=(const GameDate&) = delete;

  GameDateValue current;
};
