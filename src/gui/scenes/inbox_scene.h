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
#include "model/inbox.h"

/**
 * @brief Club inbox with category filters and digest grouping.
 *
 * Routine messages of the same kind that arrive in the same week are folded
 * into one digest row, so a busy matchday reads as a single entry instead of
 * a wall of near-identical notifications.
 */
class InboxScene : public ManagementScene
{
 public:
  explicit InboxScene(GUIView* parent);

  void update(float deltaTime) override;
  [[nodiscard]] SceneID getID() const override { return SceneID::INBOX; }

 protected:
  void renderContent() override;
  [[nodiscard]] NavSection navSection() const override
  {
    return NavSection::INBOX;
  }
  void refresh() override;

 private:
  friend class GameFlowTest_GUIFlowLifecycle_Test;

  /** @brief One visible row: a single message or a folded digest. */
  struct Thread
  {
    std::vector<size_t> messages; /**< Indices into the inbox, newest first. */
    std::string title;
    std::string detail;
    std::string date_text;
    std::vector<std::string> message_titles; /**< Digest children only. */
    InboxCategory category = InboxCategory::General;
    size_t unread = 0;
  };

  void rebuildThreads();
  void renderFilters(float width, float height);
  void renderThreads(float width, float height);
  void renderReader(float height);
  void openMessage(size_t messageIndex);

  static constexpr size_t CATEGORY_COUNT =
      static_cast<size_t>(InboxCategory::COUNT);

  std::vector<Thread> threads;
  std::array<size_t, CATEGORY_COUNT> unread_by_category{};
  std::array<size_t, CATEGORY_COUNT> total_by_category{};
  int category_filter = -1; /**< -1 = all categories. */
  bool unread_only = false;
  int selected_thread = -1;
  int expanded_thread = -1;
  size_t selected_message = SIZE_MAX;
  std::string selected_title;
  std::string selected_body;
};
