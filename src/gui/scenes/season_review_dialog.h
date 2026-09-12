// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <imgui.h>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

class GameController;
struct SeasonReview;

/**
 * @brief The end-of-season summary, shown once when a season ends: the
 * final position and its consequence (title, promotion, relegation), the
 * key numbers, the top scorer, the board's verdict on each objective and
 * next season's objectives. Celebratory or sombre with the outcome.
 */
class SeasonReviewDialog
{
 public:
  /** Formats @p review once and opens the dialog on the next render(). */
  void open(const GameController& controller, const SeasonReview& review);

  /** Draws the dialog; returns the season on the frame it is closed. */
  std::optional<std::uint16_t> render();

  /** True from open() until the dialog is closed. */
  [[nodiscard]] bool isOpen() const { return open_requested || visible; }

 private:
  struct Line
  {
    std::string label;
    std::string value;
    ImVec4 color;
  };

  bool open_requested = false;
  bool visible = false;
  std::uint16_t season = 0;

  // Formatted once in open().
  std::string headline;
  ImVec4 headline_color{};
  std::string subtitle;
  std::vector<std::pair<std::string, std::string>> tiles;
  std::vector<std::string> highlights;
  std::string verdict;
  ImVec4 verdict_color{};
  std::string confidence;
  std::vector<Line> grades;
  std::vector<std::pair<std::string, std::string>> money;
  std::vector<std::pair<std::string, std::string>> next_objectives;
  std::string farewell;
};
