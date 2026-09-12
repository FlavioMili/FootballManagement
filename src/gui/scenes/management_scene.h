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

#include "global/types.h"
#include "gui/gui_scene.h"
#include "model/gamedate.h"
#include "model/next_action.h"

/**
 * @brief Top-level destinations of the management shell.
 */
enum class NavSection : uint8_t
{
  HOME,
  INBOX,
  CLUB,
  SQUAD,
  LINEUP,
  TACTICS,
  FIXTURES,
  STANDINGS,
  TRANSFERS,
  FINANCES,
  SCOUTING,
  TRAINING,
  STAFF,
  YOUTH,
  MANAGER,
  MEDICAL,
  CALENDAR,
  SQUAD_PLANNER,
  COMPARE,
  DELEGATION,
  DATA_HUB,
  OPPOSITION,
  NONE
};

/**
 * @brief Navigation between management screens.
 *
 * Sections replace whatever is stacked above the club dashboard (one routine
 * screen at a time, no overlay pile-up). Detail screens such as a player
 * profile stack on top of the current section and close with Back.
 */
namespace Navigation
{
/** @brief Opens a top-level section. */
void open(GUIView* view, NavSection section);

/** @brief Opens a player's profile above the current screen. */
void openPlayer(GUIView* view, PlayerID playerId);

/** @brief Opens the report of a played fixture above the current screen. */
void openMatchReport(GUIView* view, GameDateValue date, TeamID homeId,
                     TeamID awayId);

/** @brief Opens another club's squad list. */
void openClub(GUIView* view, TeamID teamId);

/** @brief Compares players side by side (0 = pick one on the screen). */
void openCompare(GUIView* view, PlayerID first, PlayerID second = 0);

/** @brief Closes the top screen (back to the previous one or Home). */
void back(GUIView* view);
}  // namespace Navigation

/**
 * @brief Base class for every in-career screen.
 *
 * Draws the persistent shell (sidebar navigation, top bar with club, date,
 * balance and the context-aware Continue button, command palette and
 * keyboard shortcuts) and delegates the page body to renderContent().
 */
class ManagementScene : public GUIScene
{
 public:
  explicit ManagementScene(GUIView* guiViewPtr);

  void render() final;
  void onEnter() override;
  void onResume() override;

 protected:
  /** @brief Draws the page body inside the content region. */
  virtual void renderContent() = 0;

  /** @brief Sidebar entry highlighted for this screen. */
  [[nodiscard]] virtual NavSection navSection() const = 0;

  /** @brief Rebuilds cached view models from the game state. */
  virtual void refresh() {}

  /** @brief Shows a short confirmation message in the top bar. */
  void showToast(std::string message, bool isError = false);

 private:
  friend class GameFlowTest_GUIFlowLifecycle_Test;
  friend class GameFlowTest_ManagementScreensMidSeason_Test;

  struct PaletteEntry
  {
    enum class Kind : uint8_t
    {
      SECTION,
      CLUB,
      PLAYER,
      ACTION /*!< A pending next step (id: index into palette_actions). */
    };
    Kind kind;
    uint32_t id;
    std::string label;
    std::string label_lower;
    std::string detail;
  };

  void renderSidebar(bool collapsed);
  void renderTopBar(float height);
  void renderPalette();
  void renderMainMenuConfirm();
  void handleShortcuts();
  void openPalette();
  void buildPaletteIndex();
  void filterPalette();
  void activatePaletteEntry(const PaletteEntry& entry);
  void syncClubAccent();

  std::string toast_message;
  float toast_seconds = 0.0f;
  bool toast_is_error = false;

  bool palette_requested = false;
  bool palette_focus_input = false;
  std::array<char, 64> palette_query{};
  std::string palette_filtered_query;
  std::vector<PaletteEntry> palette_entries;
  std::vector<size_t> palette_matches;
  std::vector<NextAction> palette_actions;
  int palette_selection = 0;

  bool main_menu_confirm_requested = false;
  /** Whether the sidebar navigation needed scrolling last frame. */
  bool sidebar_nav_overflow = false;
};
