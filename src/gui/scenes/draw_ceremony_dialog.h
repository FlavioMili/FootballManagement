// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "global/types.h"
#include "model/draw_ceremony.h"

class GameController;
class GUIView;

/**
 * @brief Modal that reveals a draw already made, one tie at a time.
 *
 * Self-contained: keep one instance in the hosting screen, call open() with
 * a ceremony and render() every frame. The ties appear one by one at the
 * chosen speed; Reveal all shows the rest at once and Reduced motion shows
 * everything straight away. Clubs open their squad list; nothing in the
 * game changes.
 */
class DrawCeremonyDialog
{
 public:
  void open(const DrawCeremony& ceremony, const GameController& controller);
  void render(GUIView* view);

  [[nodiscard]] bool isOpen() const { return visible || open_requested; }
  /** Ties shown so far (all of them once revealed or skipped). */
  [[nodiscard]] std::size_t shownCount() const;
  [[nodiscard]] std::size_t revealCount() const { return rows.size(); }
  /** Shows every remaining tie at once. */
  void revealAll() { skipped = true; }

 private:
  struct Row
  {
    TeamID home_id = 0;
    TeamID away_id = 0;
    std::string home;
    std::string away;
    std::string when;
    std::string pot;
    bool focus = false;
  };

  void renderRow(GUIView* view, const Row& row, std::size_t index, float alpha);

  bool open_requested = false;
  bool visible = false;
  bool skipped = false;
  float elapsed = 0.0f;
  int speed_index = 0;
  std::size_t last_shown = 0;
  std::string title;
  std::string subtitle;
  bool league_phase = false;
  std::vector<Row> rows;
};
