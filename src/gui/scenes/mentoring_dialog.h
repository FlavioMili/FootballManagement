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
#include <utility>
#include <vector>

#include "global/types.h"

class GameController;

/**
 * @brief Modal editor of the managed club's mentoring groups.
 *
 * Self-contained: keep one instance in the hosting screen, call open() from
 * a button and render() every frame. A senior player (25+) leads up to three
 * youngsters (23 or younger); each group shows the personalities involved
 * and how far the youngsters have moved so far.
 */
class MentoringDialog
{
 public:
  void open(GameController& controller);

  /**
   * Draws the dialog while open.
   * @return True on frames where the groups changed.
   */
  bool render(GameController& controller);

  [[nodiscard]] bool isOpen() const { return visible; }

 private:
  struct Member
  {
    PlayerID id = 0;
    std::string name;
    std::string detail; /**< Age and personality. */
    std::string shift;  /**< Mentees: movement so far. */
  };
  struct GroupView
  {
    std::uint32_t id = 0;
    Member mentor;
    std::vector<Member> mentees;
    std::string effect;
  };

  void rebuild(GameController& controller);
  bool renderGroup(GameController& controller, const GroupView& group);
  bool renderNewGroup(GameController& controller);
  void report(const char* key, bool error);

  bool open_requested = false;
  bool visible = false;
  std::vector<GroupView> groups;
  std::vector<std::pair<PlayerID, std::string>> mentor_options;
  std::vector<std::pair<PlayerID, std::string>> mentee_options;
  std::string message;
  bool message_error = false;
};
