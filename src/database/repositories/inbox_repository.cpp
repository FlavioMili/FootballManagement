// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "inbox_repository.h"

#include <sqlite3.h>

#include <nlohmann/json.hpp>
#include <string>
#include <utility>

#include "model/world_rng.h"

namespace
{
std::string columnText(sqlite3_stmt* stmt, int column)
{
  const unsigned char* text = sqlite3_column_text(stmt, column);
  return text ? reinterpret_cast<const char*>(text) : "";
}
}  // namespace

InboxRepository::InboxRepository(std::shared_ptr<DatabaseConnection> conn)
    : db_conn(std::move(conn))
{
}

std::vector<InboxMessage> InboxRepository::loadAll() const
{
  std::vector<InboxMessage> messages;
  sqlite3_stmt* stmt = db_conn->prepareStatement(
      "SELECT id, game_date, category, title_key, body_key, args, is_read, "
      "player_id, team_id FROM InboxMessages ORDER BY id;");
  while (sqlite3_step(stmt) == SQLITE_ROW)
  {
    InboxMessage message;
    message.id = static_cast<std::uint32_t>(sqlite3_column_int64(stmt, 0));
    message.date = dateFromInt(sqlite3_column_int(stmt, 1));
    message.category = static_cast<InboxCategory>(sqlite3_column_int(stmt, 2));
    message.title_key = columnText(stmt, 3);
    message.body_key = columnText(stmt, 4);
    const auto args =
        nlohmann::json::parse(columnText(stmt, 5), nullptr, false);
    if (args.is_array())
    {
      for (const auto& arg : args)
      {
        if (arg.is_string()) message.args.push_back(arg.get<std::string>());
      }
    }
    message.read = sqlite3_column_int(stmt, 6) != 0;
    if (sqlite3_column_type(stmt, 7) != SQLITE_NULL)
      message.player_id = static_cast<PlayerID>(sqlite3_column_int64(stmt, 7));
    if (sqlite3_column_type(stmt, 8) != SQLITE_NULL)
      message.team_id = static_cast<TeamID>(sqlite3_column_int(stmt, 8));
    messages.push_back(std::move(message));
  }
  sqlite3_finalize(stmt);
  return messages;
}

void InboxRepository::replaceAll(
    const std::vector<InboxMessage>& messages) const
{
  sqlite3_exec(db_conn->getRaw(), "DELETE FROM InboxMessages;", nullptr,
               nullptr, nullptr);
  sqlite3_stmt* stmt = db_conn->prepareStatement(
      "INSERT INTO InboxMessages (id, game_date, category, title_key, "
      "body_key, args, is_read, player_id, team_id) VALUES "
      "(?, ?, ?, ?, ?, ?, ?, ?, ?);");
  for (const InboxMessage& message : messages)
  {
    const std::string args = nlohmann::json(message.args).dump();
    sqlite3_bind_int64(stmt, 1, message.id);
    sqlite3_bind_int(stmt, 2, dateToInt(message.date));
    sqlite3_bind_int(stmt, 3, static_cast<int>(message.category));
    sqlite3_bind_text(stmt, 4, message.title_key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, message.body_key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, args.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 7, message.read ? 1 : 0);
    if (message.player_id)
      sqlite3_bind_int64(stmt, 8, *message.player_id);
    else
      sqlite3_bind_null(stmt, 8);
    if (message.team_id)
      sqlite3_bind_int(stmt, 9, *message.team_id);
    else
      sqlite3_bind_null(stmt, 9);
    db_conn->executeStep(stmt);
    sqlite3_reset(stmt);
    sqlite3_clear_bindings(stmt);
  }
  sqlite3_finalize(stmt);
}
