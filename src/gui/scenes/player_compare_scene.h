// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <imgui.h>

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "gui/scenes/management_scene.h"
#include "model/scouting.h"

/**
 * @brief Two or three players side by side: attributes on a radar and as
 * bars, ability and potential, this season's numbers and value, wage and
 * contract. Players of other clubs are shown with the club's scouting
 * estimates and their ranges, never their true attributes.
 */
class PlayerCompareScene : public ManagementScene
{
 public:
  /** @param first,second Players to start with (0 = empty slot). */
  PlayerCompareScene(GUIView* parent, PlayerID first, PlayerID second = 0);

  void update(float deltaTime) override;
  [[nodiscard]] SceneID getID() const override
  {
    return SceneID::PLAYER_COMPARE;
  }
  /** The players it was opened with, whatever was picked since. */
  [[nodiscard]] std::optional<NavEntry> historyEntry() const override
  {
    return NavEntry::ofCompare(opened_with[0], opened_with[1]);
  }

  static constexpr std::size_t MAX_PLAYERS = 3;

 protected:
  void renderContent() override;
  [[nodiscard]] NavSection navSection() const override
  {
    return NavSection::COMPARE;
  }
  void refresh() override;

 private:
  friend class GameFlowTest_GUIFlowLifecycle_Test;

  /** @brief Everything shown for one compared player. */
  struct Slot
  {
    PlayerID id = 0;
    bool own = false;
    std::string name;
    std::string subtitle;  /*!< "ST · 24 · Club". */
    std::string knowledge; /*!< "Knowledge 62%" or "Your player". */
    std::vector<ScoutedAttribute> attributes;
    std::vector<std::string> attribute_texts; /*!< "72" or "65-78". */
    std::vector<std::string> facts;           /*!< One per FACT_KEYS row. */
  };

  /** @brief A player the picker offers. */
  struct Candidate
  {
    PlayerID id = 0;
    std::string label;
    std::string lower;
    std::string detail;
  };

  void setSlot(std::size_t index, PlayerID id);
  void buildCandidates();
  void rebuildAxes();
  void renderSlots();
  void renderPicker(std::size_t index, float width);
  void renderRadar(float width);
  void renderBars(float width);
  void renderFacts();
  [[nodiscard]] std::size_t filledSlots() const;
  [[nodiscard]] ImVec4 seriesColor(std::size_t index) const;

  std::array<Slot, MAX_PLAYERS> slots;
  std::array<PlayerID, MAX_PLAYERS> requested{};
  std::array<PlayerID, 2> opened_with{};
  /** Attribute names on the radar (union of the compared players'). */
  std::vector<std::string> axes;
  std::vector<std::string> axis_labels; /*!< Localised, same order. */
  std::vector<Candidate> candidates;
  std::array<char, 48> filter{};
  std::vector<std::size_t> filtered;
  std::string filtered_query;
};
