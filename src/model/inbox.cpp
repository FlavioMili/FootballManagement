// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "model/inbox.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <format>
#include <string_view>
#include <utility>

#include "global/language_manager.h"
#include "model/world_rng.h"

namespace
{
constexpr std::array<const char*,
                     static_cast<std::size_t>(InboxCategory::COUNT)>
    CATEGORY_KEYS = {"INBOX_CAT_GENERAL",  "INBOX_CAT_MATCH",
                     "INBOX_CAT_INJURY",   "INBOX_CAT_TRANSFER",
                     "INBOX_CAT_CONTRACT", "INBOX_CAT_YOUTH",
                     "INBOX_CAT_BOARD",    "INBOX_CAT_FINANCE"};

constexpr std::array<std::string_view, 4> ROUTINE_TITLES = {
    "INBOX_TRANSFER_NEWS_TITLE", "INBOX_INSTALMENT_PAID_TITLE",
    "INBOX_INSTALMENT_RECEIVED_TITLE", "SCOUT_ALERT_MOVED_TITLE"};

constexpr std::array<std::string_view, 2> DIGEST_TITLES = {
    "INBOX_BID_TITLE", "INBOX_LOAN_OFFER_TITLE"};

std::string resolveArgument(const std::string& argument)
{
  if (argument.size() > 1 && argument.front() == '@')
    return LOC(argument.c_str() + 1);
  return argument;
}
}  // namespace

const char* inboxCategoryKey(InboxCategory category)
{
  const auto index = static_cast<std::size_t>(category);
  return index < CATEGORY_KEYS.size() ? CATEGORY_KEYS[index]
                                      : "INBOX_CAT_GENERAL";
}

std::string formatLocalized(const std::string& key,
                            const std::vector<std::string>& args)
{
  const std::string pattern = LOC(key.c_str());
  std::string result;
  result.reserve(pattern.size() + 32);
  for (std::size_t i = 0; i < pattern.size(); ++i)
  {
    if (pattern[i] == '{')
    {
      const std::size_t close = pattern.find('}', i);
      if (close != std::string::npos && close > i + 1)
      {
        char* end = nullptr;
        const unsigned long index =
            std::strtoul(pattern.c_str() + i + 1, &end, 10);
        if (end == pattern.c_str() + close)
        {
          if (index < args.size()) result += resolveArgument(args[index]);
          i = close;
          continue;
        }
      }
    }
    result += pattern[i];
  }
  return result;
}

std::string formatMoney(std::int64_t amount)
{
  const char* sign = amount < 0 ? "-" : "";
  const double magnitude = std::abs(static_cast<double>(amount));
  if (magnitude >= 1'000'000.0)
    return std::format("{}€{:.2f}M", sign, magnitude / 1'000'000.0);
  if (magnitude >= 1'000.0)
    return std::format("{}€{:.0f}K", sign, magnitude / 1'000.0);
  return std::format("{}€{:.0f}", sign, magnitude);
}

std::string InboxMessage::formatTitle() const
{
  return formatLocalized(title_key, args);
}

std::string InboxMessage::formatBody() const
{
  return formatLocalized(body_key, args);
}

bool Inbox::isRoutine(const std::string& title_key)
{
  return std::ranges::contains(ROUTINE_TITLES, title_key);
}

bool Inbox::isDigested(const std::string& title_key)
{
  return std::ranges::contains(DIGEST_TITLES, title_key);
}

std::uint32_t Inbox::add(InboxMessage message)
{
  if (isRoutine(message.title_key)) message.read = true;
  if (!message.read && isDigested(message.title_key))
  {
    // One unread message of the kind per week; the rest join its thread.
    const std::int32_t day = dayOrdinal(message.date);
    for (auto it = messages.rbegin(); it != messages.rend(); ++it)
    {
      if (day - dayOrdinal(it->date) >= DIGEST_DAYS) break;
      if (!it->read && it->title_key == message.title_key)
      {
        message.read = true;
        break;
      }
    }
  }
  message.id = next_id++;
  const std::uint32_t id = message.id;
  messages.push_back(std::move(message));
  if (messages.size() > MAX_MESSAGES)
  {
    messages.erase(messages.begin(),
                   messages.begin() + static_cast<std::ptrdiff_t>(
                                          messages.size() - MAX_MESSAGES));
  }
  return id;
}

bool Inbox::markRead(std::uint32_t id)
{
  const auto found = std::ranges::find(messages, id, &InboxMessage::id);
  if (found == messages.end()) return false;
  found->read = true;
  return true;
}

void Inbox::markAllRead()
{
  for (InboxMessage& message : messages) message.read = true;
}

std::size_t Inbox::unreadCount() const
{
  return static_cast<std::size_t>(
      std::ranges::count(messages, false, &InboxMessage::read));
}

const std::vector<InboxMessage>& Inbox::getMessages() const { return messages; }

void Inbox::restore(std::vector<InboxMessage> restored)
{
  messages = std::move(restored);
  std::ranges::sort(messages, {}, &InboxMessage::id);
  next_id = messages.empty() ? 1 : messages.back().id + 1;
}
