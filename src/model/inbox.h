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
#include <optional>
#include <string>
#include <vector>

#include "global/types.h"
#include "model/gamedate.h"

/**
 * @enum InboxCategory
 * @brief Message categories (values are persisted).
 */
enum class InboxCategory : std::uint8_t
{
  General = 0,
  Match,
  Injury,
  Transfer,
  Contract,
  Youth,
  Board,
  Finance,
  COUNT
};

/** Language key naming @p category (e.g. "INBOX_CAT_MATCH"). */
const char* inboxCategoryKey(InboxCategory category);

/**
 * @struct InboxMessage
 * @brief A news item for the managed club.
 *
 * Title and body are stored as language keys plus arguments so that messages
 * follow the current UI language. Arguments starting with '@' are themselves
 * language keys (e.g. "@INJURY_HAMSTRING_STRAIN"). Use formatTitle() and
 * formatBody() to obtain display text.
 */
struct InboxMessage
{
  std::uint32_t id = 0;
  GameDateValue date;
  InboxCategory category = InboxCategory::General;
  std::string title_key;
  std::string body_key;
  std::vector<std::string> args;
  bool read = false;
  std::optional<PlayerID> player_id; /*!< Linked player, if any. */
  std::optional<TeamID> team_id;     /*!< Linked club, if any. */

  /** Localised title with arguments substituted ({0}, {1}, ...). */
  std::string formatTitle() const;

  /** Localised body with arguments substituted ({0}, {1}, ...). */
  std::string formatBody() const;
};

/**
 * @class Inbox
 * @brief Chronological list of messages with read tracking.
 *
 * The inbox keeps the newest MAX_MESSAGES messages; older ones are dropped.
 * Only news that needs the manager's attention arrives unread: routine
 * round-ups (transfer news, instalments) are filed as read, and repeated
 * offers of the same kind (bids and loan offers for the club's players) are
 * grouped into a weekly digest with a single unread message.
 */
class Inbox
{
 public:
  static constexpr std::size_t MAX_MESSAGES = 400;
  /** Window of the weekly digest of repeated offers. */
  static constexpr std::int32_t DIGEST_DAYS = 7;

  /** Appends a message, assigns its id and returns it. */
  std::uint32_t add(InboxMessage message);

  /** Routine news that is filed as read on arrival. */
  static bool isRoutine(const std::string& title_key);

  /** Messages grouped into a weekly digest (one unread per week). */
  static bool isDigested(const std::string& title_key);

  /** Marks a message as read; false if the id is unknown. */
  bool markRead(std::uint32_t id);

  /** Marks every message as read. */
  void markAllRead();

  /** Number of unread messages. */
  std::size_t unreadCount() const;

  /** Messages, oldest first. */
  const std::vector<InboxMessage>& getMessages() const;

  /** Replaces the content with persisted messages. */
  void restore(std::vector<InboxMessage> messages);

 private:
  std::vector<InboxMessage> messages;
  std::uint32_t next_id = 1;
};

/** Formats a localised template, substituting {n} with @p args. */
std::string formatLocalized(const std::string& key,
                            const std::vector<std::string>& args);

/** Compact money text for messages, e.g. "€1.25M", "€350K", "-€2.0M". */
std::string formatMoney(std::int64_t amount);
