// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "player_repository.h"

#include <sqlite3.h>

#include <nlohmann/json.hpp>
#include <stdexcept>

#include "database/SQLLoader.h"

PlayerRepository::PlayerRepository(std::shared_ptr<DatabaseConnection> conn)
    : db_conn(conn)
{
}

std::vector<Player> PlayerRepository::loadAllPlayers() const
{
  sqlite3_stmt* stmt;
  const std::string& sql = SQLLoader::getQuery(Query::SELECT_ALL_PLAYERS);
  std::vector<Player> players;

  if (sqlite3_prepare_v2(db_conn->getRaw(), sql.c_str(), -1, &stmt, nullptr) !=
      SQLITE_OK)
  {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db_conn->getRaw())));
  }

  while (sqlite3_step(stmt) == SQLITE_ROW)
  {
    uint32_t id = static_cast<uint32_t>(sqlite3_column_int(stmt, 0));
    uint32_t team_id = static_cast<uint32_t>(sqlite3_column_int(stmt, 1));
    const char* first_name =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    const char* last_name =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
    int age = sqlite3_column_int(stmt, 4);
    const char* role =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
    const char* nationality_str =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
    uint32_t wage = static_cast<uint32_t>(sqlite3_column_int(stmt, 7));
    int contract_years = sqlite3_column_int(stmt, 8);
    int height = sqlite3_column_int(stmt, 9);
    const char* foot_str =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));
    const char* stats_str =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 11));
    int status = sqlite3_column_int(stmt, 12);

    auto it = stringToLanguage.find(nationality_str);
    Language nationality =
        (it != stringToLanguage.end()) ? it->second : Language::EN;
    Foot foot = (std::string(foot_str) == "Left") ? Foot::Left : Foot::Right;

    nlohmann::json stats_json = nlohmann::json::parse(stats_str);
    std::map<std::string, float> stats =
        stats_json.get<std::map<std::string, float>>();

    players.emplace_back(id, team_id, first_name, last_name, role, nationality,
                         wage, status, age, contract_years, height, foot,
                         stats);
  }

  sqlite3_finalize(stmt);
  return players;
}

void PlayerRepository::insertPlayer(const Player& player) const
{
  sqlite3_stmt* stmt;
  const std::string& sql = SQLLoader::getQuery(Query::INSERT_PLAYER);

  if (sqlite3_prepare_v2(db_conn->getRaw(), sql.c_str(), -1, &stmt, 0) !=
      SQLITE_OK)
  {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db_conn->getRaw())));
  }

  nlohmann::json stats_json = player.getStats();
  std::string stats_str = stats_json.dump();

  sqlite3_bind_int(stmt, 1, static_cast<int>(player.getTeamId()));
  sqlite3_bind_text(stmt, 2, player.getFirstName().c_str(), -1,
                    SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, player.getLastName().c_str(), -1,
                    SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 4, player.getAge());
  sqlite3_bind_text(stmt, 5, player.getRole().c_str(), -1, SQLITE_TRANSIENT);

  auto it = languageToString.find(player.getNationality());
  std::string nationality_str =
      (it != languageToString.end()) ? std::string(it->second) : "English";
  sqlite3_bind_text(stmt, 6, nationality_str.c_str(), -1, SQLITE_TRANSIENT);

  sqlite3_bind_int(stmt, 7, static_cast<int>(player.getWage()));
  sqlite3_bind_int(stmt, 8, player.getContractYears());
  sqlite3_bind_int(stmt, 9, player.getHeight());

  std::string foot_str = (player.getFoot() == Foot::Left) ? "Left" : "Right";
  sqlite3_bind_text(stmt, 10, foot_str.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 11, stats_str.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 12, static_cast<int>(player.getStatus()));

  if (sqlite3_step(stmt) != SQLITE_DONE)
  {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute statement: " +
                             std::string(sqlite3_errmsg(db_conn->getRaw())));
  }

  sqlite3_finalize(stmt);
}

void PlayerRepository::insertPlayerWithId(const Player& player) const
{
  sqlite3_stmt* stmt;
  const std::string& sql = SQLLoader::getQuery(Query::INSERT_PLAYER_WITH_ID);

  if (sqlite3_prepare_v2(db_conn->getRaw(), sql.c_str(), -1, &stmt, 0) !=
      SQLITE_OK)
  {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db_conn->getRaw())));
  }

  nlohmann::json stats_json = player.getStats();
  std::string stats_str = stats_json.dump();

  sqlite3_bind_int(stmt, 1, static_cast<int>(player.getId()));
  sqlite3_bind_int(stmt, 2, static_cast<int>(player.getTeamId()));
  sqlite3_bind_text(stmt, 3, player.getFirstName().c_str(), -1,
                    SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, player.getLastName().c_str(), -1,
                    SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 5, player.getAge());
  sqlite3_bind_text(stmt, 6, player.getRole().c_str(), -1, SQLITE_TRANSIENT);

  auto it = languageToString.find(player.getNationality());
  std::string nationality_str =
      (it != languageToString.end()) ? std::string(it->second) : "English";
  sqlite3_bind_text(stmt, 7, nationality_str.c_str(), -1, SQLITE_TRANSIENT);

  sqlite3_bind_int(stmt, 8, static_cast<int>(player.getWage()));
  sqlite3_bind_int(stmt, 9, player.getContractYears());
  sqlite3_bind_int(stmt, 10, player.getHeight());

  std::string foot_str = (player.getFoot() == Foot::Left) ? "Left" : "Right";
  sqlite3_bind_text(stmt, 11, foot_str.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 12, stats_str.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 13, static_cast<int>(player.getStatus()));

  if (sqlite3_step(stmt) != SQLITE_DONE)
  {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute statement: " +
                             std::string(sqlite3_errmsg(db_conn->getRaw())));
  }

  sqlite3_finalize(stmt);
}

void PlayerRepository::updatePlayer(const Player& player) const
{
  sqlite3_stmt* stmt;
  const std::string& sql = SQLLoader::getQuery(Query::UPDATE_PLAYER);

  if (sqlite3_prepare_v2(db_conn->getRaw(), sql.c_str(), -1, &stmt, 0) !=
      SQLITE_OK)
  {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db_conn->getRaw())));
  }

  nlohmann::json stats_json = player.getStats();
  std::string stats_str = stats_json.dump();

  sqlite3_bind_int(stmt, 1, static_cast<int>(player.getTeamId()));
  sqlite3_bind_text(stmt, 2, player.getFirstName().c_str(), -1,
                    SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, player.getLastName().c_str(), -1,
                    SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 4, player.getAge());
  sqlite3_bind_text(stmt, 5, player.getRole().c_str(), -1, SQLITE_TRANSIENT);

  auto it = languageToString.find(player.getNationality());
  std::string nationality_str =
      (it != languageToString.end()) ? std::string(it->second) : "English";
  sqlite3_bind_text(stmt, 6, nationality_str.c_str(), -1, SQLITE_TRANSIENT);

  sqlite3_bind_int(stmt, 7, static_cast<int>(player.getWage()));
  sqlite3_bind_int(stmt, 8, player.getContractYears());
  sqlite3_bind_int(stmt, 9, player.getHeight());

  std::string foot_str = (player.getFoot() == Foot::Left) ? "Left" : "Right";
  sqlite3_bind_text(stmt, 10, foot_str.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 11, stats_str.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 12, static_cast<int>(player.getStatus()));
  sqlite3_bind_int(stmt, 13, static_cast<int>(player.getId()));

  if (sqlite3_step(stmt) != SQLITE_DONE)
  {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute statement: " +
                             std::string(sqlite3_errmsg(db_conn->getRaw())));
  }

  sqlite3_finalize(stmt);
}

void PlayerRepository::deletePlayer(PlayerID player_id) const
{
  sqlite3_stmt* stmt;
  const std::string& sql = SQLLoader::getQuery(Query::DELETE_PLAYER);
  if (sqlite3_prepare_v2(db_conn->getRaw(), sql.c_str(), -1, &stmt, 0) !=
      SQLITE_OK)
  {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db_conn->getRaw())));
  }

  sqlite3_bind_int(stmt, 1, static_cast<int>(player_id));

  if (sqlite3_step(stmt) != SQLITE_DONE)
  {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute statement: " +
                             std::string(sqlite3_errmsg(db_conn->getRaw())));
  }

  sqlite3_finalize(stmt);
}

void PlayerRepository::transferPlayer(PlayerID player_id,
                                      uint16_t new_team_id) const
{
  sqlite3_stmt* stmt;
  const std::string& sql = SQLLoader::getQuery(Query::TRANSFER_PLAYER);

  if (sqlite3_prepare_v2(db_conn->getRaw(), sql.c_str(), -1, &stmt, 0) !=
      SQLITE_OK)
  {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db_conn->getRaw())));
  }

  sqlite3_bind_int(stmt, 1, new_team_id);
  sqlite3_bind_int(stmt, 2, static_cast<int>(player_id));

  if (sqlite3_step(stmt) != SQLITE_DONE)
  {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute statement: " +
                             std::string(sqlite3_errmsg(db_conn->getRaw())));
  }

  sqlite3_finalize(stmt);
}
