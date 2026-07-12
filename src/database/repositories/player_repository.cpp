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
  sqlite3_stmt* stmt =
      db_conn->prepareStatement(SQLLoader::getQuery(Query::SELECT_ALL_PLAYERS));
  std::vector<Player> players;

  while (sqlite3_step(stmt) == SQLITE_ROW)
  {
    auto id = static_cast<uint32_t>(sqlite3_column_int(stmt, 0));
    auto team_id = static_cast<uint32_t>(sqlite3_column_int(stmt, 1));
    auto first_name =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    auto last_name =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
    int age = sqlite3_column_int(stmt, 4);
    auto role = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
    auto nationality_str =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
    auto wage = static_cast<uint32_t>(sqlite3_column_int(stmt, 7));
    int contract_years = sqlite3_column_int(stmt, 8);
    int height = sqlite3_column_int(stmt, 9);
    auto foot_str =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));
    auto stats_str =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 11));
    int status = sqlite3_column_int(stmt, 12);

    std::string nat_str(nationality_str);
    auto it = stringToLanguage.find(nat_str);
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

void PlayerRepository::bindPlayerParams(sqlite3_stmt* stmt,
                                        const Player& player,
                                        int startIndex) const
{
  nlohmann::json stats_json = player.getStats();
  std::string stats_str = stats_json.dump();

  sqlite3_bind_int(stmt, startIndex++, static_cast<int>(player.getTeamId()));
  sqlite3_bind_text(stmt, startIndex++, player.getFirstName().c_str(), -1,
                    SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, startIndex++, player.getLastName().c_str(), -1,
                    SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, startIndex++, player.getAge());
  sqlite3_bind_text(stmt, startIndex++, player.getRole().c_str(), -1,
                    SQLITE_TRANSIENT);

  auto it = languageToString.find(player.getNationality());
  std::string nationality_str =
      (it != languageToString.end()) ? std::string(it->second) : "English";
  sqlite3_bind_text(stmt, startIndex++, nationality_str.c_str(), -1,
                    SQLITE_TRANSIENT);

  sqlite3_bind_int(stmt, startIndex++, static_cast<int>(player.getWage()));
  sqlite3_bind_int(stmt, startIndex++, player.getContractYears());
  sqlite3_bind_int(stmt, startIndex++, player.getHeight());

  std::string foot_str = (player.getFoot() == Foot::Left) ? "Left" : "Right";
  sqlite3_bind_text(stmt, startIndex++, foot_str.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, startIndex++, stats_str.c_str(), -1,
                    SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, startIndex++, static_cast<int>(player.getStatus()));
}

void PlayerRepository::insertPlayer(const Player& player) const
{
  sqlite3_stmt* stmt =
      db_conn->prepareStatement(SQLLoader::getQuery(Query::INSERT_PLAYER));

  bindPlayerParams(stmt, player, 1);

  db_conn->executeStep(stmt);
  sqlite3_finalize(stmt);
}

void PlayerRepository::insertPlayerWithId(const Player& player) const
{
  sqlite3_stmt* stmt = db_conn->prepareStatement(
      SQLLoader::getQuery(Query::INSERT_PLAYER_WITH_ID));

  sqlite3_bind_int(stmt, 1, static_cast<int>(player.getId()));
  bindPlayerParams(stmt, player, 2);

  db_conn->executeStep(stmt);
  sqlite3_finalize(stmt);
}

void PlayerRepository::updatePlayer(const Player& player) const
{
  sqlite3_stmt* stmt =
      db_conn->prepareStatement(SQLLoader::getQuery(Query::UPDATE_PLAYER));

  bindPlayerParams(stmt, player, 1);
  sqlite3_bind_int(stmt, 13, static_cast<int>(player.getId()));

  db_conn->executeStep(stmt);
  sqlite3_finalize(stmt);
}

void PlayerRepository::deletePlayer(PlayerID player_id) const
{
  sqlite3_stmt* stmt =
      db_conn->prepareStatement(SQLLoader::getQuery(Query::DELETE_PLAYER));
  sqlite3_bind_int(stmt, 1, static_cast<int>(player_id));

  db_conn->executeStep(stmt);
  sqlite3_finalize(stmt);
}

void PlayerRepository::transferPlayer(PlayerID player_id,
                                      uint16_t new_team_id) const
{
  sqlite3_stmt* stmt =
      db_conn->prepareStatement(SQLLoader::getQuery(Query::TRANSFER_PLAYER));

  sqlite3_bind_int(stmt, 1, new_team_id);
  sqlite3_bind_int(stmt, 2, static_cast<int>(player_id));

  db_conn->executeStep(stmt);
  sqlite3_finalize(stmt);
}
