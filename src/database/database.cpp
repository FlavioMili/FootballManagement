// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "database.h"
#include "SQLLoader.h"
#include "gamedata.h"
#include "global/logger.h"
#include "global/paths.h"
#include "global/queries.h"
#include "json.hpp"
#include <cstdint>
#include <iostream>
#include <sqlite3.h>
#include <stdexcept>
#include <string>
#include <vector>

Database::Database() : db(nullptr, &sqlite3_close) {
  sqlite3 *raw_db = nullptr;
  if (sqlite3_open(DATABASE_PATH, &raw_db) != SQLITE_OK) {
    std::cerr << "Can't open database: " << sqlite3_errmsg(raw_db) << "\n";
    throw std::runtime_error("Failed to open database: " +
                             std::string(DATABASE_PATH));
  }

  db.reset(raw_db);
  sqlite3_exec(db.get(), "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);

  loadSQLFiles();
}

void Database::loadSQLFiles() {
  try {
    SQLLoader::loadQueriesFromFile();
  } catch (const std::exception &e) {
    Logger::error("Failed to load SQL queries: " + std::string(e.what()) +
                  '\n');
    throw;
  }
}

void Database::initialize() {
  Logger::debug("Database::initialize called.");
  try {
    std::string schema_sql = SQLLoader::loadSchemaFromFile();
    char *err_msg = nullptr;

    if (sqlite3_exec(db.get(), schema_sql.c_str(), 0, 0, &err_msg) !=
        SQLITE_OK) {
      std::string error_str =
          "SQL error during schema initialization: " + std::string(err_msg);
      sqlite3_free(err_msg);
      Logger::error(error_str);
    } else {
      Logger::debug("Database schema initialized successfully.\n");
    }
  } catch (const std::exception &e) {
    Logger::error("Failed to initialize database: " + std::string(e.what()) +
                  "\n");
    throw;
  }
}

bool Database::isFirstRun() {
  sqlite3_stmt *stmt;
  const char *sql = "SELECT COUNT(*) FROM GameState WHERE id = 1;";

  if (sqlite3_prepare_v2(db.get(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return true;
  }

  bool first_run = true;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    int count = sqlite3_column_int(stmt, 0);
    first_run = (count == 0);
  }

  sqlite3_finalize(stmt);
  return first_run;
}

std::vector<League> Database::loadAllLeagues() {
  sqlite3_stmt *stmt;
  const std::string &sql = SQLLoader::getQuery(Query::SELECT_LEAGUES);
  std::vector<League> leagues;

  if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    int id = sqlite3_column_int(stmt, 0);
    const unsigned char *name_text = sqlite3_column_text(stmt, 1);
    std::string name =
        name_text ? reinterpret_cast<const char *>(name_text) : "";
    leagues.emplace_back(id, name);
  }

  for (auto &league : leagues) {
    loadTeamsForLeague(league);
    loadLeaguePoints(league);
  }

  sqlite3_finalize(stmt);
  return leagues;
}

void Database::loadTeamsForLeague(League &league) const {
  sqlite3_stmt *stmt;
  const std::string &sql = "SELECT id FROM Teams WHERE league_id = ?";

  if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  sqlite3_bind_int(stmt, 1, league.getId());

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    int team_id = sqlite3_column_int(stmt, 0);
    league.addTeamID(team_id);
  }

  sqlite3_finalize(stmt);
}

std::vector<Team> Database::loadAllTeams() {
  std::vector<Team> teams;
  sqlite3_stmt *stmt;
  const std::string &sql = "SELECT id, league_id, name, balance FROM Teams";

  if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    int id = sqlite3_column_int(stmt, 0);
    int league_id = sqlite3_column_int(stmt, 1);
    const unsigned char *name_text = sqlite3_column_text(stmt, 2);
    std::string name =
        name_text ? reinterpret_cast<const char *>(name_text) : "";
    std::int64_t balance = sqlite3_column_int64(stmt, 3);
    teams.emplace_back(id, league_id, name, balance);
  }

  sqlite3_finalize(stmt);
  return teams;
}

std::vector<Player> Database::loadAllPlayers() {
  sqlite3_stmt *stmt;
  const std::string &sql = SQLLoader::getQuery(Query::SELECT_ALL_PLAYERS);
  std::vector<Player> players;

  if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, nullptr) !=
      SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    uint32_t id = sqlite3_column_int(stmt, 0);
    uint32_t team_id = sqlite3_column_int(stmt, 1);
    const char *first_name =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
    const char *last_name =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
    int age = sqlite3_column_int(stmt, 4);
    const char *role =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 5));
    const char *nationality_str =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6));
    uint32_t wage = sqlite3_column_int(stmt, 7);
    int contract_years = sqlite3_column_int(stmt, 8);
    int height = sqlite3_column_int(stmt, 9);
    const char *foot_str =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 10));
    const char *stats_str =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 11));
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

void Database::insertLeague(const League &league) {
  sqlite3_stmt *stmt;
  const std::string &sql = SQLLoader::getQuery(Query::INSERT_LEAGUE);

  if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  sqlite3_bind_text(stmt, 1, league.getName().data(), -1, SQLITE_TRANSIENT);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  sqlite3_finalize(stmt);
}

void Database::insertLeagueWithId(const League &league) {
  sqlite3_stmt *stmt;
  const std::string &sql = SQLLoader::getQuery(Query::INSERT_LEAGUE_WITH_ID);

  if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  sqlite3_bind_int(stmt, 1, league.getId()); // Explicit ID
  sqlite3_bind_text(stmt, 2, league.getName().c_str(), -1, SQLITE_TRANSIENT);

  if (league.getParentLeagueID()) {
    uint8_t parentID = *league.getParentLeagueID();
    sqlite3_bind_int(stmt, 3, parentID); // NULL if no parent
  }

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  sqlite3_finalize(stmt);
}

void Database::insertTeam(const Team &team) {
  sqlite3_stmt *stmt;
  const std::string &sql = SQLLoader::getQuery(Query::INSERT_TEAM);

  if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  sqlite3_bind_int(stmt, 1, team.getLeagueId());
  sqlite3_bind_text(stmt, 2, team.getName().c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 3, team.getFinances().getBalance());

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  sqlite3_finalize(stmt);
}

void Database::insertTeamWithId(const Team &team) {
  sqlite3_stmt *stmt;
  const std::string &sql = SQLLoader::getQuery(Query::INSERT_TEAM_WITH_ID);

  if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  sqlite3_bind_int(stmt, 1, team.getId()); // Explicit ID
  sqlite3_bind_int(stmt, 2, team.getLeagueId());
  sqlite3_bind_text(stmt, 3, team.getName().c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 4, team.getFinances().getBalance());

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  sqlite3_finalize(stmt);
}

void Database::insertPlayer(const Player &player) {
  sqlite3_stmt *stmt;
  const std::string &sql = SQLLoader::getQuery(Query::INSERT_PLAYER);

  if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  nlohmann::json stats_json = player.getStats();
  std::string stats_str = stats_json.dump();

  // Assuming Player has getters for first/last name instead of getName()
  std::string first_name = player.getFirstName();
  std::string last_name = player.getLastName();

  sqlite3_bind_int(stmt, 1, player.getTeamId());
  sqlite3_bind_text(stmt, 2, first_name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, last_name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 4, player.getAge());
  sqlite3_bind_text(stmt, 5, player.getRole().c_str(), -1, SQLITE_TRANSIENT);

  auto it = languageToString.find(player.getNationality());
  std::string nationality_str =
      (it != languageToString.end()) ? std::string(it->second) : "English";
  sqlite3_bind_text(stmt, 6, nationality_str.c_str(), -1, SQLITE_TRANSIENT);

  sqlite3_bind_int(stmt, 7, player.getWage());
  sqlite3_bind_int(stmt, 8, player.getContractYears());
  sqlite3_bind_int(stmt, 9, player.getHeight());

  std::string foot_str = (player.getFoot() == Foot::Left) ? "Left" : "Right";
  sqlite3_bind_text(stmt, 10, foot_str.c_str(), -1, SQLITE_TRANSIENT);

  sqlite3_bind_text(stmt, 11, stats_str.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 12, player.getStatus());

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  sqlite3_finalize(stmt);
}

void Database::insertPlayerWithId(const Player &player) {
  sqlite3_stmt *stmt;
  const std::string &sql = SQLLoader::getQuery(Query::INSERT_PLAYER_WITH_ID);

  if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  nlohmann::json stats_json = player.getStats();
  std::string stats_str = stats_json.dump();

  sqlite3_bind_int(stmt, 1, player.getId()); // Explicit ID
  sqlite3_bind_int(stmt, 2, player.getTeamId());
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

  sqlite3_bind_int(stmt, 8, player.getWage());
  sqlite3_bind_int(stmt, 9, player.getContractYears());
  sqlite3_bind_int(stmt, 10, player.getHeight());

  std::string foot_str = (player.getFoot() == Foot::Left) ? "Left" : "Right";
  sqlite3_bind_text(stmt, 11, foot_str.c_str(), -1, SQLITE_TRANSIENT);

  sqlite3_bind_text(stmt, 12, stats_str.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 13, player.getStatus());

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  sqlite3_finalize(stmt);
}

void Database::transferPlayer(uint32_t player_id, uint16_t new_team_id) {
  sqlite3_stmt *stmt;
  const std::string &sql = SQLLoader::getQuery(Query::TRANSFER_PLAYER);

  if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  sqlite3_bind_int(stmt, 1, new_team_id);
  sqlite3_bind_int(stmt, 2, player_id);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  sqlite3_finalize(stmt);
}

void Database::insertFixture(const Match &match, uint8_t league_id) {
  std::string sql =
      "INSERT INTO Fixtures (game_date, home_team_id, away_team_id, league_id, "
      "match_type, home_goals, away_goals, played) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?) "
      "ON CONFLICT(game_date, home_team_id, away_team_id) DO UPDATE SET "
      "league_id = excluded.league_id, "
      "match_type = excluded.match_type, "
      "home_goals = excluded.home_goals, "
      "away_goals = excluded.away_goals, "
      "played = excluded.played;";

  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, nullptr) ==
      SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, match.getDate().toString().c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, match.getHomeTeamId());
    sqlite3_bind_int(stmt, 3, match.getAwayTeamId());
    sqlite3_bind_int(stmt, 4, league_id);
    sqlite3_bind_int(stmt, 5, static_cast<int>(match.getMatchType()));

    if (match.isPlayed()) {
      sqlite3_bind_int(stmt, 6, match.getHomeScore());
      sqlite3_bind_int(stmt, 7, match.getAwayScore());
      sqlite3_bind_int(stmt, 8, 1); // played = 1
    } else {
      sqlite3_bind_null(stmt, 6);
      sqlite3_bind_null(stmt, 7);
      sqlite3_bind_int(stmt, 8, 0); // played = 0
    }

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
  } else {
    Logger::error("Failed to prepare upsertFixture statement: " +
                  std::string(sqlite3_errmsg(db.get())));
  }
}

void Database::saveMatches(const std::vector<Match> &matches) {
  for (const auto &match : matches) {
    // Assuming league_id can be determined or is not needed for all match types
    // For now, passing 0 as a placeholder league_id
    insertFixture(match, 0);
  }
}

std::vector<Match> Database::loadAllMatches() {
  std::vector<Match> matches;
  std::string sql =
      "SELECT home_team_id, away_team_id, game_date, match_type FROM Fixtures;";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, nullptr) !=
      SQLITE_OK) {
    Logger::error("Failed to prepare statement: " +
                  std::string(sqlite3_errmsg(db.get())));
    return matches;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    uint16_t home_id = sqlite3_column_int(stmt, 0);
    uint16_t away_id = sqlite3_column_int(stmt, 1);
    std::string date_str =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
    auto match_type = static_cast<MatchType>(sqlite3_column_int(stmt, 3));

    matches.emplace_back(home_id, away_id, GameDateValue::fromString(date_str),
                         match_type);
  }

  sqlite3_finalize(stmt);
  return matches;
}

void Database::updateGameState(uint8_t current_season, uint16_t managed_team_id,
                               const std::string &game_date) {
  sqlite3_stmt *stmt;
  const std::string &sql = SQLLoader::getQuery(Query::UPSERT_GAME_STATE);

  if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  sqlite3_bind_int(stmt, 1, managed_team_id);
  sqlite3_bind_text(stmt, 2, game_date.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 3, current_season);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  sqlite3_finalize(stmt);
}

bool Database::loadGameState(uint8_t &current_season, uint16_t &managed_team_id,
                             std::string &game_date) const {
  sqlite3_stmt *stmt;
  const std::string &sql = SQLLoader::getQuery(Query::SELECT_GAME_STATE);
  bool has_data = false;

  if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    return false;
  }

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    has_data = true;
    managed_team_id = sqlite3_column_int(stmt, 0);
    const unsigned char *game_date_text = sqlite3_column_text(stmt, 1);
    game_date =
        game_date_text ? reinterpret_cast<const char *>(game_date_text) : "";
    current_season = sqlite3_column_int(stmt, 2);
  }

  sqlite3_finalize(stmt);
  return has_data;
}

void Database::saveLeaguePoints(const League &league) {
  const std::string &sql = SQLLoader::getQuery(Query::UPSERT_LEAGUE_POINTS);
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  for (const auto &pair : league.getLeaderboard()) {
    sqlite3_bind_int(stmt, 1, league.getId());
    sqlite3_bind_int(stmt, 2, pair.first);
    sqlite3_bind_int(stmt, 3, pair.second);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
      sqlite3_finalize(stmt);
      throw std::runtime_error("Failed to execute statement: " +
                               std::string(sqlite3_errmsg(db.get())));
    }

   sqlite3_clear_bindings(stmt);
    sqlite3_reset(stmt);
  }

  sqlite3_finalize(stmt);
}

void Database::loadLeaguePoints(League &league) const {
  const std::string &sql = SQLLoader::getQuery(Query::SELECT_LEAGUE_POINTS);
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  sqlite3_bind_int(stmt, 1, league.getId());

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    uint16_t team_id = static_cast<uint16_t>(sqlite3_column_int(stmt, 0));
    uint8_t points = static_cast<uint8_t>(sqlite3_column_int(stmt, 1));
    league.setPoints(team_id, points);
  }

  sqlite3_finalize(stmt);
}

// TODO remove
void Database::resetAllLeaguePoints() {
  const std::string &sql = SQLLoader::getQuery(Query::RESET_ALL_LEAGUE_POINTS);
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  sqlite3_finalize(stmt);
}

void Database::updatePlayer(const Player &player) {
  sqlite3_stmt *stmt;
  const std::string &sql = SQLLoader::getQuery(Query::UPDATE_PLAYER);

  if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  nlohmann::json stats_json = player.getStats();
  std::string stats_str = stats_json.dump();

  sqlite3_bind_int(stmt, 1, player.getTeamId());
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

  sqlite3_bind_int(stmt, 7, player.getWage());
  sqlite3_bind_int(stmt, 8, player.getContractYears());
  sqlite3_bind_int(stmt, 9, player.getHeight());

  std::string foot_str = (player.getFoot() == Foot::Left) ? "Left" : "Right";
  sqlite3_bind_text(stmt, 10, foot_str.c_str(), -1, SQLITE_TRANSIENT);

  sqlite3_bind_text(stmt, 11, stats_str.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 12, player.getStatus());
  sqlite3_bind_int(stmt, 13, player.getId());

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  sqlite3_finalize(stmt);
}

void Database::deletePlayer(uint32_t player_id) {
  sqlite3_stmt *stmt;
  const std::string &sql = SQLLoader::getQuery(Query::DELETE_PLAYER);
  if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  sqlite3_bind_int(stmt, 1, player_id);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  sqlite3_finalize(stmt);
}

void Database::ageAllPlayers() {
  auto &players_map = GameData::instance().getPlayers();

  for (auto &pair : players_map) {
    Player &player = pair.second;

    player.agePlayer();

    if (player.checkRetirement()) {
      deletePlayer(player.getId());
      std::cout << player.getName() << " just retired at the age of "
                << player.getAge() << "!\n";
    } else {
      updatePlayer(player);
    }
  }
}
