#include "database.h"
#include "SQLLoader.h"
#include <sqlite3.h>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include <iostream>
#include <stdexcept>
#include "global.h"
#include "json.hpp"

struct Database::Impl {
  sqlite3* db;
  SQLLoader* sql_loader;

  Impl() : db(nullptr), sql_loader(&SQLLoader::getInstance()) {}
};

Database::Database(const std::string& db_path, 
                   const std::string& schema_path,
                   const std::string& queries_path) 
: pImpl(std::make_unique<Impl>()), schema_path_(schema_path), queries_path_(queries_path) {

  if (sqlite3_open(db_path.c_str(), &pImpl->db)) {
    std::cerr << "Can't open database: " << sqlite3_errmsg(pImpl->db) << "\n";
    throw std::runtime_error("Failed to open database: " + db_path);
  } else {
    sqlite3_exec(pImpl->db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
  }

  loadSQLFiles();
}

Database::~Database() {
  close();
}

void Database::close() {
  if (pImpl->db) {
    int rc = sqlite3_close(pImpl->db);
    if (rc != SQLITE_OK) {
      std::cerr << "Error closing database: " << sqlite3_errmsg(pImpl->db) << std::endl;
    }
    pImpl->db = nullptr;
  }
}

void Database::loadSQLFiles() {
  try {
    pImpl->sql_loader->loadQueriesFromFile(queries_path_);
    // Debug line
    // std::cout << "SQL queries loaded successfully from: " << queries_path_ << std::endl;
  } catch (const std::exception& e) {
    std::cerr << "Failed to load SQL queries: " << e.what() << std::endl;
    throw;
  }
}

void Database::initialize() {
  try {
    std::string schema_sql = pImpl->sql_loader->loadSchemaFromFile(schema_path_);
    char* err_msg = nullptr;

    if (sqlite3_exec(pImpl->db, schema_sql.c_str(), 0, 0, &err_msg) != SQLITE_OK) {
      std::string error_str = "SQL error: " + std::string(err_msg);
      sqlite3_free(err_msg);
      throw std::runtime_error(error_str);
    }

    // Debug line
    // std::cout << "Database schema initialized successfully." << std::endl;
  } catch (const std::exception& e) {
    std::cerr << "Failed to initialize database: " << e.what() << std::endl;
    throw;
  }
}

bool Database::isFirstRun() const {
  sqlite3_stmt* stmt;
  const std::string& sql = pImpl->sql_loader->getQuery("SELECT_GAME_STATE_BY_KEY");

  if (sqlite3_prepare_v2(pImpl->db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
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

void Database::addLeague(const std::string& name) {
  sqlite3_stmt* stmt;
  const std::string& sql = pImpl->sql_loader->getQuery("INSERT_LEAGUE");

  if (sqlite3_prepare_v2(pImpl->db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(pImpl->db)));
  }

  sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute statement: " + std::string(sqlite3_errmsg(pImpl->db)));
  }

  sqlite3_finalize(stmt);
}

std::vector<League> Database::getLeagues() const {
  sqlite3_stmt* stmt;
  const std::string& sql = pImpl->sql_loader->getQuery("SELECT_LEAGUES");
  std::vector<League> leagues;

  if (sqlite3_prepare_v2(pImpl->db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(pImpl->db)));
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    int id = sqlite3_column_int(stmt, 0);
    const unsigned char* name_text = sqlite3_column_text(stmt, 1);
    std::string name = name_text ? reinterpret_cast<const char*>(name_text) : "";
    leagues.emplace_back(id, name);
  }

  sqlite3_finalize(stmt);
  return leagues;
}

std::vector<int> Database::getAllTeamIds() const {
  std::vector<int> team_ids;
  sqlite3_stmt* stmt;
  const std::string& sql = pImpl->sql_loader->getQuery("SELECT_ALL_TEAM_IDS");

  if (sqlite3_prepare_v2(pImpl->db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(pImpl->db)));
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    team_ids.push_back(sqlite3_column_int(stmt, 0));
  }

  sqlite3_finalize(stmt);
  return team_ids;
}

void Database::transferPlayer(int player_id, int new_team_id) {
  sqlite3_stmt* stmt;
  const std::string& sql = pImpl->sql_loader->getQuery("TRANSFER_PLAYER");

  if (sqlite3_prepare_v2(pImpl->db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(pImpl->db)));
  }

  sqlite3_bind_int(stmt, 1, new_team_id);
  sqlite3_bind_int(stmt, 2, player_id);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute statement: " + std::string(sqlite3_errmsg(pImpl->db)));
  }

  sqlite3_finalize(stmt);
}

void Database::addFirstName(const std::string& name) {
  sqlite3_stmt* stmt;
  const std::string& sql = pImpl->sql_loader->getQuery("INSERT_FIRST_NAME");

  if (sqlite3_prepare_v2(pImpl->db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(pImpl->db)));
  }

  sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute statement: " + std::string(sqlite3_errmsg(pImpl->db)));
  }

  sqlite3_finalize(stmt);
}

void Database::addLastName(const std::string& name) {
  sqlite3_stmt* stmt;
  const std::string& sql = pImpl->sql_loader->getQuery("INSERT_LAST_NAME");

  if (sqlite3_prepare_v2(pImpl->db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(pImpl->db)));
  }

  sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute statement: " + std::string(sqlite3_errmsg(pImpl->db)));
  }

  sqlite3_finalize(stmt);
}

std::vector<std::string> Database::getFirstNames() const {
  sqlite3_stmt* stmt;
  const std::string& sql = pImpl->sql_loader->getQuery("SELECT_FIRST_NAMES");
  std::vector<std::string> names;

  if (sqlite3_prepare_v2(pImpl->db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(pImpl->db)));
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const unsigned char* name_text = sqlite3_column_text(stmt, 0);
    names.emplace_back(reinterpret_cast<const char*>(name_text));
  }

  sqlite3_finalize(stmt);
  return names;
}

std::vector<std::string> Database::getLastNames() const {
  sqlite3_stmt* stmt;
  const std::string& sql = pImpl->sql_loader->getQuery("SELECT_LAST_NAMES");
  std::vector<std::string> names;

  if (sqlite3_prepare_v2(pImpl->db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(pImpl->db)));
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const unsigned char* name_text = sqlite3_column_text(stmt, 0);
    names.emplace_back(reinterpret_cast<const char*>(name_text));
  }

  sqlite3_finalize(stmt);
  return names;
}

void Database::addTeam(int league_id, const std::string& name, uint64_t balance) {
  sqlite3_stmt* stmt;
  const std::string& sql = pImpl->sql_loader->getQuery("INSERT_TEAM");

  if (sqlite3_prepare_v2(pImpl->db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(pImpl->db)));
  }

  sqlite3_bind_int(stmt, 1, league_id);
  sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 3, balance);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute statement: " + std::string(sqlite3_errmsg(pImpl->db)));
  }

  sqlite3_finalize(stmt);
}

std::vector<Team> Database::getTeams(const int league_id) const {
  std::vector<Team> teams;
  sqlite3_stmt* stmt;
  const std::string& sql = pImpl->sql_loader->getQuery("SELECT_TEAMS_BY_LEAGUE");

  if (sqlite3_prepare_v2(pImpl->db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(pImpl->db)));
  }

  sqlite3_bind_int(stmt, 1, league_id);

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    int id = sqlite3_column_int(stmt, 0);
    const unsigned char* name_text = sqlite3_column_text(stmt, 1);
    std::string name = name_text ? reinterpret_cast<const char*>(name_text) : "";
    uint64_t balance = sqlite3_column_int64(stmt, 2);
    teams.emplace_back(id, league_id, name, balance);
  }

  sqlite3_finalize(stmt);
  return teams;
}

void Database::updateTeam(const Team& team) {
  sqlite3_stmt* stmt;
  const std::string& sql = pImpl->sql_loader->getQuery("UPDATE_TEAM");

  if (sqlite3_prepare_v2(pImpl->db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(pImpl->db)));
  }

  sqlite3_bind_text(stmt, 1, team.getName().c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 2, team.getBalance());
  sqlite3_bind_int(stmt, 3, team.getId());

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute statement: " + std::string(sqlite3_errmsg(pImpl->db)));
  }

  sqlite3_finalize(stmt);
}

void Database::addPlayer(int team_id, const Player& player) {
  sqlite3_stmt* stmt;
  const std::string& sql = pImpl->sql_loader->getQuery("INSERT_PLAYER");

  if (sqlite3_prepare_v2(pImpl->db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(pImpl->db)));
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
    throw std::runtime_error("Failed to execute statement: " + std::string(sqlite3_errmsg(pImpl->db)));
  }

  sqlite3_finalize(stmt);
}

std::vector<Player> Database::getPlayers(int team_id) const {
  sqlite3_stmt* stmt;
  const std::string& sql = pImpl->sql_loader->getQuery("SELECT_PLAYERS_BY_TEAM");
  std::vector<Player> players;

  if (sqlite3_prepare_v2(pImpl->db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(pImpl->db)));
  }

  sqlite3_bind_int(stmt, 1, team_id);

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    int id = sqlite3_column_int(stmt, 0);
    const unsigned char* name_text = sqlite3_column_text(stmt, 1);
    std::string name = name_text ? reinterpret_cast<const char*>(name_text) : "";
    int age = sqlite3_column_int(stmt, 2);
    const unsigned char* role_text = sqlite3_column_text(stmt, 3);
    std::string role = role_text ? reinterpret_cast<const char*>(role_text) : "";
    const unsigned char* stats_text = sqlite3_column_text(stmt, 4);
    std::string stats_str = stats_text ? reinterpret_cast<const char*>(stats_text) : "";

    Player player(id, name, age, role);
    nlohmann::json stats_json = nlohmann::json::parse(stats_str);
    player.setStats(stats_json.get<std::map<std::string, float>>());
    players.push_back(player);
  }

  sqlite3_finalize(stmt);
  return players;
}

void Database::saveCalendar(const Calendar& calendar, int season, int league_id) {
  sqlite3_stmt* stmt;
  const std::string& sql = pImpl->sql_loader->getQuery("INSERT_CALENDAR_MATCH");

  if (sqlite3_prepare_v2(pImpl->db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(pImpl->db)));
  }

  int week_num = 0;
  for (const auto& week : calendar.getWeeks()) {
    for (const auto& match : week.getMatches()) {
      sqlite3_bind_int(stmt, 1, season);
      sqlite3_bind_int(stmt, 2, week_num);
      sqlite3_bind_int(stmt, 3, match.home_team_id);
      sqlite3_bind_int(stmt, 4, match.away_team_id);
      sqlite3_bind_int(stmt, 5, league_id);

      if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to execute statement: " + std::string(sqlite3_errmsg(pImpl->db)));
      }

      sqlite3_reset(stmt);
    }
    week_num++;
  }

  sqlite3_finalize(stmt);
}

Calendar Database::loadCalendar(int season, int league_id) const {
  sqlite3_stmt* stmt;
  const std::string& sql = pImpl->sql_loader->getQuery("SELECT_CALENDAR");
  Calendar calendar;
  std::vector<Week> weeks;

  if (sqlite3_prepare_v2(pImpl->db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(pImpl->db)));
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
  calendar.setWeeks(std::move(weeks));
  return calendar;
}

void Database::updateGameState(int season, int week, int managed_team_id) {
  sqlite3_stmt* stmt;
  const std::string& sql = pImpl->sql_loader->getQuery("UPSERT_GAME_STATE");

  // Update season
  if (sqlite3_prepare_v2(pImpl->db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(pImpl->db)));
  }
  sqlite3_bind_text(stmt, 1, "season", -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 2, season);
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute statement: " + std::string(sqlite3_errmsg(pImpl->db)));
  }
  sqlite3_finalize(stmt);

  // Update week
  if (sqlite3_prepare_v2(pImpl->db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(pImpl->db)));
  }
  sqlite3_bind_text(stmt, 1, "week", -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 2, week);
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute statement: " + std::string(sqlite3_errmsg(pImpl->db)));
  }
  sqlite3_finalize(stmt);

  // Update managed team ID
  if (sqlite3_prepare_v2(pImpl->db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(pImpl->db)));
  }
  sqlite3_bind_text(stmt, 1, "managed_team_id", -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 2, managed_team_id);
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute statement: " + std::string(sqlite3_errmsg(pImpl->db)));
  }
  sqlite3_finalize(stmt);
}

void Database::loadGameState(int& season, int& week, int& managed_team_id) const {
  sqlite3_stmt* stmt;
  const std::string& sql = pImpl->sql_loader->getQuery("SELECT_GAME_STATE");

  if (sqlite3_prepare_v2(pImpl->db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(pImpl->db)));
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    std::string key = (const char*)sqlite3_column_text(stmt, 0);
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
  sqlite3_stmt* stmt;
  const std::string& sql = pImpl->sql_loader->getQuery("SELECT_GAME_STATE_BY_KEY");
  int managed_team_id = -1;

  if (sqlite3_prepare_v2(pImpl->db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(pImpl->db)));
  }

  sqlite3_bind_text(stmt, 1, "managed_team_id", -1, SQLITE_TRANSIENT);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    managed_team_id = sqlite3_column_int(stmt, 0);
  }

  sqlite3_finalize(stmt);
  return managed_team_id;
}

void Database::saveManagedTeamId(int team_id) {
  sqlite3_stmt* stmt;
  const std::string& sql = pImpl->sql_loader->getQuery("UPSERT_GAME_STATE");

  if (sqlite3_prepare_v2(pImpl->db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(pImpl->db)));
  }

  sqlite3_bind_text(stmt, 1, "managed_team_id", -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 2, team_id);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute statement: " + std::string(sqlite3_errmsg(pImpl->db)));
  }

  sqlite3_finalize(stmt);
}

void Database::saveLeaguePoints(const League& league) {
  const std::string& sql = pImpl->sql_loader->getQuery("UPSERT_LEAGUE_POINTS");
  sqlite3_stmt* stmt;

  if (sqlite3_prepare_v2(pImpl->db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(pImpl->db)));
  }

  for (const auto& pair : league.getLeaderboard()) {
    sqlite3_bind_int(stmt, 1, league.getId());
    sqlite3_bind_int(stmt, 2, pair.first);
    sqlite3_bind_int(stmt, 3, pair.second);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
      sqlite3_finalize(stmt);
      throw std::runtime_error("Failed to execute statement: " + std::string(sqlite3_errmsg(pImpl->db)));
    }

    sqlite3_reset(stmt);
  }

  sqlite3_finalize(stmt);
}

void Database::loadLeaguePoints(League& league) const {
  const std::string& sql = pImpl->sql_loader->getQuery("SELECT_LEAGUE_POINTS");
  sqlite3_stmt* stmt;

  if (sqlite3_prepare_v2(pImpl->db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(pImpl->db)));
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
  const std::string& sql = pImpl->sql_loader->getQuery("RESET_ALL_LEAGUE_POINTS");
  sqlite3_stmt* stmt;

  if (sqlite3_prepare_v2(pImpl->db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(pImpl->db)));
  }

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute statement: " + std::string(sqlite3_errmsg(pImpl->db)));
  }

  sqlite3_finalize(stmt);
}

void Database::updatePlayer(const Player& player) {
  sqlite3_stmt* stmt;
  const std::string& sql = pImpl->sql_loader->getQuery("UPDATE_PLAYER");

  if (sqlite3_prepare_v2(pImpl->db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(pImpl->db)));
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
    throw std::runtime_error("Failed to execute statement: " + std::string(sqlite3_errmsg(pImpl->db)));
  }

  sqlite3_finalize(stmt);
}

void Database::deletePlayer(int player_id) {
  sqlite3_stmt* stmt;
  const std::string& sql = pImpl->sql_loader->getQuery("DELETE_PLAYER");
  if (sqlite3_prepare_v2(pImpl->db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(pImpl->db)));
  }

  sqlite3_bind_int(stmt, 1, player_id);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute statement: " + std::string(sqlite3_errmsg(pImpl->db)));
  }

  sqlite3_finalize(stmt);
}

void Database::ageAllPlayers() {
  std::vector<Player> all_players = getAllPlayers();

  for (auto& player : all_players) {
    player.agePlayer();
    if (player.checkRetirement()) {
      deletePlayer(player.getId());
      std::cout << player.getName() << " just retired at the age of " << player.getAge() << "!\n";
    } else {
      updatePlayer(player);
    }
  }
}

std::vector<Player> Database::getFreeAgents() const {
  return getPlayers(FREE_AGENTS_TEAM_ID);
}

std::vector<Player> Database::getAllPlayers() const {
  std::vector<Player> all_players;
  sqlite3_stmt* stmt;
  const std::string& sql = pImpl->sql_loader->getQuery("SELECT_ALL_PLAYERS");

  if (sqlite3_prepare_v2(pImpl->db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(pImpl->db)));
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    int id = sqlite3_column_int(stmt, 0);
    const unsigned char* name_text = sqlite3_column_text(stmt, 2);
    std::string name = name_text ? reinterpret_cast<const char*>(name_text) : "";
    int age = sqlite3_column_int(stmt, 3);
    const unsigned char* role_text = sqlite3_column_text(stmt, 4);
    std::string role = role_text ? reinterpret_cast<const char*>(role_text) : "";
    const unsigned char* stats_text = sqlite3_column_text(stmt, 5);
    std::string stats_str = stats_text ? reinterpret_cast<const char*>(stats_text) : "";

    Player player(id, name, age, role);
    nlohmann::json stats_json = nlohmann::json::parse(stats_str);
    player.setStats(stats_json.get<std::map<std::string, float>>());
    all_players.push_back(player);
  }

  sqlite3_finalize(stmt);
  return all_players;
}
