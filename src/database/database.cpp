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
    // Debug line
    // std::cout << "SQL queries loaded successfully from: " << queries_path_ <<
    // std::endl;
  } catch (const std::exception &e) {
    std::cerr << "Failed to load SQL queries: " << e.what() << '\n';
    throw;
  }
}

void Database::initialize() {
  try {
    std::string schema_sql = SQLLoader::loadSchemaFromFile();
    char *err_msg = nullptr;

    if (sqlite3_exec(db.get(), schema_sql.c_str(), 0, 0, &err_msg) !=
        SQLITE_OK) {
      std::string error_str = "SQL error: " + std::string(err_msg);
      sqlite3_free(err_msg);
      throw std::runtime_error(error_str);
    }

    // Debug line
    Logger::debug("Database schema initialized successfully.\n");
  } catch (const std::exception &e) {
    Logger::error("Failed to initialize database: " + std::string(e.what()) +
                  "\n");
    throw;
  }
}

bool Database::isFirstRun() const {
  sqlite3_stmt *stmt;
  const std::string &sql = SQLLoader::getQuery(Query::SELECT_GAME_STATE_BY_KEY);

  if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    return true; // Table doesn't exist yet, so it's a first run
  }

  sqlite3_bind_text(stmt, 1, "season", -1, SQLITE_TRANSIENT);
  int rc = sqlite3_step(stmt);

  if (rc == SQLITE_ROW) {
    int season = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return season == 0;
  }

  sqlite3_finalize(stmt);
  return true;
}

void Database::addLeague(const std::string &name) {
  sqlite3_stmt *stmt;
  const std::string &sql = SQLLoader::getQuery(Query::INSERT_LEAGUE);

  if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  sqlite3_finalize(stmt);
}

std::vector<League> Database::getLeagues() const {
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

  sqlite3_finalize(stmt);
  return leagues;
}

std::vector<int> Database::getAllTeamIds() const {
  std::vector<int> team_ids;
  sqlite3_stmt *stmt;
  const std::string &sql = SQLLoader::getQuery(Query::SELECT_ALL_TEAM_IDS);

  if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    team_ids.push_back(sqlite3_column_int(stmt, 0));
  }

  sqlite3_finalize(stmt);
  return team_ids;
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

void Database::addFirstName(const std::string &name) {
  sqlite3_stmt *stmt;
  const std::string &sql = SQLLoader::getQuery(Query::INSERT_FIRST_NAME);

  if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  sqlite3_finalize(stmt);
}

void Database::addLastName(const std::string &name) {
  sqlite3_stmt *stmt;
  const std::string &sql = SQLLoader::getQuery(Query::INSERT_LAST_NAME);

  if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  sqlite3_finalize(stmt);
}

std::vector<std::string> Database::getFirstNames() const {
  sqlite3_stmt *stmt;
  const std::string &sql = SQLLoader::getQuery(Query::SELECT_FIRST_NAMES);
  std::vector<std::string> names;

  if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const unsigned char *name_text = sqlite3_column_text(stmt, 0);
    names.emplace_back(reinterpret_cast<const char *>(name_text));
  }

  sqlite3_finalize(stmt);
  return names;
}

std::vector<std::string> Database::getLastNames() const {
  sqlite3_stmt *stmt;
  const std::string &sql = SQLLoader::getQuery(Query::SELECT_LAST_NAMES);
  std::vector<std::string> names;

  if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const unsigned char *name_text = sqlite3_column_text(stmt, 0);
    names.emplace_back(reinterpret_cast<const char *>(name_text));
  }

  sqlite3_finalize(stmt);
  return names;
}

void Database::addTeam(uint8_t league_id, const std::string &name,
                       int64_t balance) {
  sqlite3_stmt *stmt;
  const std::string &sql = SQLLoader::getQuery(Query::INSERT_TEAM);

  if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  sqlite3_bind_int(stmt, 1, league_id);
  sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 3, static_cast<long long>(balance));

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  sqlite3_finalize(stmt);
}

std::vector<Team> Database::getTeams(const uint8_t league_id) const {
  std::vector<Team> teams;
  sqlite3_stmt *stmt;
  const std::string &sql = SQLLoader::getQuery(Query::SELECT_TEAMS_BY_LEAGUE);

  if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  sqlite3_bind_int(stmt, 1, league_id);

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    int id = sqlite3_column_int(stmt, 0);
    const unsigned char *name_text = sqlite3_column_text(stmt, 1);
    std::string name =
        name_text ? reinterpret_cast<const char *>(name_text) : "";
    std::int64_t balance = sqlite3_column_int64(stmt, 2);
    teams.emplace_back(id, league_id, name, balance);
  }

  sqlite3_finalize(stmt);
  return teams;
}

void Database::updateTeam(const Team &team) {
  sqlite3_stmt *stmt;
  const std::string &sql = SQLLoader::getQuery(Query::UPDATE_TEAM);

  if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  sqlite3_bind_text(stmt, 1, team.getName().c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 2, team.getFinances().getBalance());
  sqlite3_bind_int(stmt, 3, team.getId());

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  sqlite3_finalize(stmt);
}

void Database::addPlayer(int team_id, const Player &player) {
  sqlite3_stmt *stmt;
  const std::string &sql = SQLLoader::getQuery(Query::INSERT_PLAYER);

  if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  nlohmann::json stats_json = player.getStats();
  std::string stats_str = stats_json.dump();

  sqlite3_bind_int(stmt, 1, team_id);
  sqlite3_bind_text(stmt, 2, player.getName().c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 3, player.getAge());
  sqlite3_bind_text(stmt, 4, player.getRole().c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 5, stats_str.c_str(), -1, SQLITE_TRANSIENT);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  sqlite3_finalize(stmt);
}

std::vector<uint32_t> Database::getPlayers(uint16_t team_id) const {
  sqlite3_stmt *stmt;
  const std::string &sql = SQLLoader::getQuery(Query::SELECT_PLAYERS_BY_TEAM);
  std::vector<Player> players;

  if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, nullptr) !=
      SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  sqlite3_bind_int(stmt, 1, team_id);

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    int id = sqlite3_column_int(stmt, 0);
    const unsigned char *name_text = sqlite3_column_text(stmt, 1);
    std::string name =
        name_text ? reinterpret_cast<const char *>(name_text) : "";
    int age = sqlite3_column_int(stmt, 2);
    const unsigned char *role_text = sqlite3_column_text(stmt, 3);
    std::string role =
        role_text ? reinterpret_cast<const char *>(role_text) : "";
    const unsigned char *stats_text = sqlite3_column_text(stmt, 4);
    std::string stats_str =
        stats_text ? reinterpret_cast<const char *>(stats_text) : "";

    Player player(id, name, age, role);
    nlohmann::json stats_json = nlohmann::json::parse(stats_str);
    player.setStats(stats_json.get<std::map<std::string, float>>());
    players.push_back(player);
  }

  sqlite3_finalize(stmt);
  return players;
}

void Database::saveCalendar(const Calendar &calendar, int season,
                            uint8_t league_id) {
  sqlite3_stmt *stmt;
  const std::string &sql = SQLLoader::getQuery(Query::INSERT_CALENDAR_MATCH);

  if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  int week_num = 0;
  for (const auto &week : calendar.getWeeks()) {
    for (const auto &match : week.getMatches()) {
      sqlite3_bind_int(stmt, 1, season);
      sqlite3_bind_int(stmt, 2, week_num);
      sqlite3_bind_int(stmt, 3, match.home_team_id);
      sqlite3_bind_int(stmt, 4, match.away_team_id);
      sqlite3_bind_int(stmt, 5, league_id);

      if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to execute statement: " +
                                 std::string(sqlite3_errmsg(db.get())));
      }

      sqlite3_reset(stmt);
    }
    week_num++;
  }

  sqlite3_finalize(stmt);
}

Calendar Database::loadCalendar(int season, uint8_t league_id) const {
  sqlite3_stmt *stmt;
  const std::string &sql = SQLLoader::getQuery(Query::SELECT_CALENDAR);
  Calendar calendar;
  std::vector<Week> weeks;

  if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  sqlite3_bind_int(stmt, 1, season);
  sqlite3_bind_int(stmt, 2, league_id);

  int current_week = -1;
  std::optional<Week> week;

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    int week_num = sqlite3_column_int(stmt, 0);
    if (week_num != current_week) {
      if (week.has_value()) {
        weeks.push_back(std::move(*week));
      }
      week.emplace(week_num);
      current_week = week_num;
    }
    week->addMatch({sqlite3_column_int(stmt, 1), sqlite3_column_int(stmt, 2)});
  }

  if (week.has_value()) {
    weeks.push_back(std::move(*week));
  }

  sqlite3_finalize(stmt);
  calendar.setWeeks(weeks);
  return calendar;
}

void Database::updateGameState(int season, int week, int managed_team_id) {
  sqlite3_stmt *stmt;
  const std::string &sql = SQLLoader::getQuery(Query::UPSERT_GAME_STATE);

  // Update season
  if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }
  sqlite3_bind_text(stmt, 1, "season", -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 2, season);
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }
  sqlite3_finalize(stmt);

  // Update week
  if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }
  sqlite3_bind_text(stmt, 1, "week", -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 2, week);
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }
  sqlite3_finalize(stmt);

  // Update managed team ID
  if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }
  sqlite3_bind_text(stmt, 1, "managed_team_id", -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 2, managed_team_id);
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }
  sqlite3_finalize(stmt);
}

void Database::loadGameState(int &season, int &week,
                             int &managed_team_id) const {
  sqlite3_stmt *stmt;
  const std::string &sql = SQLLoader::getQuery(Query::SELECT_GAME_STATE);

  if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    std::string key =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    if (key == "season") {
      season = sqlite3_column_int(stmt, 1);
    } else if (key == "week") {
      week = sqlite3_column_int(stmt, 1);
    } else if (key == "managed_team_id") {
      managed_team_id = sqlite3_column_int(stmt, 1);
    }
  }

  sqlite3_finalize(stmt);
}

int Database::loadManagedTeamId() const {
  sqlite3_stmt *stmt;
  const std::string &sql = SQLLoader::getQuery(Query::SELECT_GAME_STATE_BY_KEY);
  int managed_team_id = -1;

  if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  sqlite3_bind_text(stmt, 1, "managed_team_id", -1, SQLITE_TRANSIENT);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    managed_team_id = sqlite3_column_int(stmt, 0);
  }

  sqlite3_finalize(stmt);
  return managed_team_id;
}

void Database::saveManagedTeamId(int team_id) {
  sqlite3_stmt *stmt;
  const std::string &sql = SQLLoader::getQuery(Query::UPSERT_GAME_STATE);

  if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  sqlite3_bind_text(stmt, 1, "managed_team_id", -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 2, team_id);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  sqlite3_finalize(stmt);
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
    int team_id = sqlite3_column_int(stmt, 0);
    int points = sqlite3_column_int(stmt, 1);
    league.setPoints(team_id, points);
  }

  sqlite3_finalize(stmt);
}

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

  sqlite3_bind_text(stmt, 1, player.getName().c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 2, player.getAge());
  sqlite3_bind_text(stmt, 3, player.getRole().c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, stats_str.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 5, player.getId());

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  sqlite3_finalize(stmt);
}

void Database::deletePlayer(int player_id) {
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
  auto &players_map = GameData::instance().getPlayers(); // mutable access

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

std::vector<Player> Database::getFreeAgents() const {
  return getPlayers(FREE_AGENTS_TEAM_ID);
}

std::vector<uint32_t> Database::getAllPlayers() const {
  std::vector<uint32_t> all_players;
  sqlite3_stmt *stmt;
  const std::string &sql = SQLLoader::getQuery(Query::SELECT_ALL_PLAYERS);

  if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, nullptr) !=
      SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db.get())));
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    int id = sqlite3_column_int(stmt, 0);
    // const unsigned char *name_text = sqlite3_column_text(stmt, 2);
    // std::string name =
    //     name_text ? reinterpret_cast<const char *>(name_text) : "";
    // int age = sqlite3_column_int(stmt, 3);
    // const unsigned char *role_text = sqlite3_column_text(stmt, 4);
    // std::string role =
    //     role_text ? reinterpret_cast<const char *>(role_text) : "";
    // const unsigned char *stats_text = sqlite3_column_text(stmt, 5);
    // std::string stats_str =
    //     stats_text ? reinterpret_cast<const char *>(stats_text) : "";
    //
    // Player player(id, name, age, role);
    // nlohmann::json stats_json = nlohmann::json::parse(stats_str);
    // player.setStats(stats_json.get<std::map<std::string, float>>());
    all_players.push_back(id);
  }

  sqlite3_finalize(stmt);
  return all_players;
}
