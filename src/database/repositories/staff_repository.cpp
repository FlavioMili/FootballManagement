// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "database/repositories/staff_repository.h"

#include <sqlite3.h>

#include <algorithm>
#include <charconv>
#include <string>
#include <utility>

namespace
{
std::string columnText(sqlite3_stmt* stmt, int column)
{
  const unsigned char* text = sqlite3_column_text(stmt, column);
  return text ? reinterpret_cast<const char*>(text) : "";
}

std::string encodeAttributes(const StaffMember& member)
{
  std::string text;
  for (std::size_t i = 0; i < member.attributes.size(); ++i)
  {
    if (i > 0) text += ',';
    text += std::to_string(member.attributes[i]);
  }
  return text;
}

void decodeAttributes(const std::string& text, StaffMember& member)
{
  std::size_t index = 0;
  const char* cursor = text.data();
  const char* end = text.data() + text.size();
  while (cursor < end && index < member.attributes.size())
  {
    int value = 0;
    const auto [next, error] = std::from_chars(cursor, end, value);
    if (error != std::errc()) break;
    member.attributes[index++] =
        static_cast<std::uint8_t>(std::clamp(value, 1, 100));
    cursor = next < end && *next == ',' ? next + 1 : next;
  }
}
}  // namespace

StaffRepository::StaffRepository(std::shared_ptr<DatabaseConnection> conn)
    : db_conn(std::move(conn))
{
}

std::vector<StaffMember> StaffRepository::loadAll() const
{
  std::vector<StaffMember> staff;
  sqlite3_stmt* stmt = db_conn->prepareStatement(
      "SELECT id, team_id, first_name, last_name, nationality, age, role, "
      "attributes, wage, contract_years FROM Staff ORDER BY id;");
  while (sqlite3_step(stmt) == SQLITE_ROW)
  {
    StaffMember member;
    member.id = static_cast<StaffID>(sqlite3_column_int64(stmt, 0));
    member.team_id = static_cast<TeamID>(sqlite3_column_int(stmt, 1));
    member.first_name = columnText(stmt, 2);
    member.last_name = columnText(stmt, 3);
    member.nationality = static_cast<Language>(sqlite3_column_int(stmt, 4));
    member.age = static_cast<std::uint8_t>(sqlite3_column_int(stmt, 5));
    const int role = sqlite3_column_int(stmt, 6);
    if (role < 0 || role >= static_cast<int>(STAFF_ROLE_COUNT)) continue;
    member.role = static_cast<StaffRole>(role);
    decodeAttributes(columnText(stmt, 7), member);
    member.wage = static_cast<std::uint32_t>(sqlite3_column_int64(stmt, 8));
    member.contract_years =
        static_cast<std::uint8_t>(sqlite3_column_int(stmt, 9));
    staff.push_back(std::move(member));
  }
  sqlite3_finalize(stmt);
  return staff;
}

void StaffRepository::replaceAll(const StaffRoster& roster) const
{
  sqlite3_exec(db_conn->getRaw(), "DELETE FROM Staff;", nullptr, nullptr,
               nullptr);
  sqlite3_stmt* stmt = db_conn->prepareStatement(
      "INSERT INTO Staff (id, team_id, first_name, last_name, nationality, "
      "age, role, attributes, wage, contract_years) VALUES "
      "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?);");
  for (const auto& [id, member] : roster.all())
  {
    const std::string attributes = encodeAttributes(member);
    sqlite3_bind_int64(stmt, 1, id);
    sqlite3_bind_int(stmt, 2, member.team_id);
    sqlite3_bind_text(stmt, 3, member.first_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, member.last_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, static_cast<int>(member.nationality));
    sqlite3_bind_int(stmt, 6, member.age);
    sqlite3_bind_int(stmt, 7, static_cast<int>(member.role));
    sqlite3_bind_text(stmt, 8, attributes.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 9, member.wage);
    sqlite3_bind_int(stmt, 10, member.contract_years);
    db_conn->executeStep(stmt);
    sqlite3_reset(stmt);
  }
  sqlite3_finalize(stmt);
}
