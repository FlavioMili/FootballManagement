// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <array>
#include <cstddef>
#include <string>

class GameController;

/**
 * @brief Short welcome tour of a new career: the club, what the board
 * expects, the next thing to do and how Continue works.
 *
 * It opens once when a career is started from the club choice (and again
 * on request from the Help screen), so nothing about it is stored in a save.
 * Every page can be skipped; the texts come from the career's real state,
 * read once when the tour opens.
 */
class WelcomeTour
{
 public:
  /** Pages, in order. */
  static constexpr std::size_t PAGE_COUNT = 4;

  /** Reads the career state and shows the first page next frame. */
  void open(const GameController& controller);

  /** Draws the tour while it is open (a modal dialog). */
  void render();

  [[nodiscard]] bool isOpen() const { return visible; }
  [[nodiscard]] std::size_t page() const { return current_page; }

  /** Moves to the next page; closes after the last one. */
  void next();
  /** Moves to the previous page. */
  void back();
  /** Closes the tour. */
  void close();

  /** Body text of a page (for tests). */
  [[nodiscard]] const std::string& body(std::size_t index) const
  {
    return bodies[index];
  }

 private:
  bool visible = false;
  bool open_requested = false;
  std::size_t current_page = 0;
  std::string club_name;
  std::array<std::string, PAGE_COUNT> bodies;
  std::string next_action_title;
  std::string next_action_reason;
};
