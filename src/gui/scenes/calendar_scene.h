// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "gui/scenes/management_scene.h"
#include "model/season_agenda.h"

/**
 * @brief Season calendar: a month grid with the club's fixtures, breaks and
 * club events, the month's agenda and the selected day's details (match
 * report of a played fixture, the opponent's squad before it).
 */
class CalendarScene : public ManagementScene
{
 public:
  explicit CalendarScene(GUIView* parent);

  void update(float deltaTime) override;
  [[nodiscard]] SceneID getID() const override { return SceneID::CALENDAR; }

 protected:
  void renderContent() override;
  [[nodiscard]] NavSection navSection() const override
  {
    return NavSection::CALENDAR;
  }
  void refresh() override;

 private:
  friend class GameFlowTest_GUIFlowLifecycle_Test;

  /** @brief One agenda entry, pre-formatted. */
  struct Entry
  {
    AgendaEvent event;
    std::string title;  /*!< "vs Opponent (H)" or the event name. */
    std::string detail; /*!< Competition, result or span. */
    std::string score;  /*!< Played fixtures: "2-1". */
    std::string line;   /*!< Agenda text: title and score. */
    std::string date;   /*!< Short date. */
    int outcome = 0;    /*!< Played fixtures: 1 win, 0 draw, -1 loss. */
    TeamID opponent = 0;
  };

  /** @brief What happens on one day of the shown month. */
  struct Day
  {
    int fixture = -1;  /*!< Index into entries, -1 without a match. */
    std::uint16_t kinds = 0; /*!< Bit per AgendaKind (non-fixtures). */
    bool international = false;
    bool winter = false;
  };

  void showMonth(int year, int month);
  void renderToolbar();
  void renderGrid(float width);
  void renderAgenda(float width);
  void renderDay(float width);
  [[nodiscard]] bool isToday(int day) const;

  std::vector<Entry> entries;       /*!< Whole season, by date. */
  GameDateValue season_first;       /*!< 1 July of the season. */
  GameDateValue today;
  int shown_year = 0;
  int shown_month = 0;
  int days_in_month = 0;
  int first_weekday = 0;            /*!< 0 = Monday. */
  std::array<Day, 32> days{};       /*!< Index 1..31. */
  std::vector<std::size_t> month_entries; /*!< Indices into entries. */
  int selected_day = 0;
  std::string month_title;
};
