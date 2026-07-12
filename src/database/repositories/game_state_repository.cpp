// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "game_state_repository.h"

#include <sqlite3.h>

#include <stdexcept>

#include "database/SQLLoader.h"

GameStateRepository::GameStateRepository(
    std::shared_ptr<DatabaseConnection> conn)
    : db_conn(conn)
{
}

bool GameStateRepository::isFirstRun() const
{
  sqlite3_stmt* stmt;
  const char* sql = "SELECT COUNT(*) FROM GameState WHERE id = 1;";

  if (sqlite3_prepare_v2(db_conn->getRaw(), sql, -1, &stmt, nullptr) !=
      SQLITE_OK)
  {
    return true;  // Assume first run if table doesn't exist
  }

  bool first_run = true;
  if (sqlite3_step(stmt) == SQLITE_ROW)
  {
    int count = sqlite3_column_int(stmt, 0);
    first_run = (count == 0);
  }

  sqlite3_finalize(stmt);
  return first_run;
}

void GameStateRepository::updateGameState(uint8_t current_season,
                                          uint16_t managed_team_id,
                                          const std::string& game_date) const
{
  sqlite3_stmt* stmt = db_conn->prepareStatement(SQLLoader::getQuery(Query::UPSERT_GAME_STATE));

  sqlite3_bind_int(stmt, 1, managed_team_id);
  sqlite3_bind_text(stmt, 2, game_date.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 3, current_season);

  db_conn->executeStep(stmt);
  sqlite3_finalize(stmt);
}

bool GameStateRepository::loadGameState(uint8_t& current_season,
                                        uint16_t& managed_team_id,
                                        std::string& game_date) const
{
  sqlite3_stmt* stmt;
  const std::string& sql = SQLLoader::getQuery(Query::SELECT_GAME_STATE);
  bool has_data = false;

  if (sqlite3_prepare_v2(db_conn->getRaw(), sql.c_str(), -1, &stmt, 0) !=
      SQLITE_OK)
  {
    return false;
  }

  if (sqlite3_step(stmt) == SQLITE_ROW)
  {
    has_data = true;
    managed_team_id = static_cast<uint16_t>(sqlite3_column_int(stmt, 0));
    const unsigned char* game_date_text = sqlite3_column_text(stmt, 1);
    game_date =
        game_date_text ? reinterpret_cast<const char*>(game_date_text) : "";
    current_season = static_cast<uint8_t>(sqlite3_column_int(stmt, 2));
  }

  sqlite3_finalize(stmt);
  return has_data;
}
