#include "database.h"
#include <sqlite3.h>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include <iostream>
#include "json.hpp"

struct Database::Impl {
  sqlite3* db;
};

Database::Database(const std::string& db_path) : pImpl(std::make_unique<Impl>()) {
  if (sqlite3_open(db_path.c_str(), &pImpl->db)) {
    std::cerr << "Can't open database: " << sqlite3_errmsg(pImpl->db) << "\n";
  } else {
    sqlite3_exec(pImpl->db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
  }
}

Database::~Database() {
  close();
}

void Database::close() {
  if (pImpl->db) {
    int rc = sqlite3_close(pImpl->db);
    if (rc != SQLITE_OK) {
      // Log error, but don't throw from destructor context
      std::cerr << "Error closing database: " << sqlite3_errmsg(pImpl->db) << std::endl;
    }
    pImpl->db = nullptr;
  }
}

void Database::initialize() {
  char* err_msg = nullptr;
  const char* sql =
    "CREATE TABLE IF NOT EXISTS Leagues (id INTEGER PRIMARY KEY, name TEXT NOT NULL);"
    "CREATE TABLE IF NOT EXISTS Teams (id INTEGER PRIMARY KEY, league_id INTEGER, name TEXT NOT NULL, balance INTEGER, FOREIGN KEY(league_id) REFERENCES Leagues(id));"
    "CREATE TABLE IF NOT EXISTS Players (id INTEGER PRIMARY KEY, team_id INTEGER, name TEXT NOT NULL, age INTEGER, role TEXT, stats TEXT, FOREIGN KEY(team_id) REFERENCES Teams(id));"
    "CREATE TABLE IF NOT EXISTS Calendar (season INTEGER, week INTEGER, home_team_id INTEGER, away_team_id INTEGER, league_id INTEGER, PRIMARY KEY(season, league_id, week, home_team_id, away_team_id));"
    "CREATE TABLE IF NOT EXISTS GameState (key TEXT PRIMARY KEY, value INTEGER);"
    "CREATE TABLE IF NOT EXISTS LeaguePoints (league_id INTEGER, team_id INTEGER, points INTEGER, PRIMARY KEY(league_id, team_id));"
    "CREATE TABLE IF NOT EXISTS FirstNames (id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE);"
    "CREATE TABLE IF NOT EXISTS LastNames (id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE);";

  if (sqlite3_exec(pImpl->db, sql, 0, 0, &err_msg) != SQLITE_OK) {
    std::string error_str = "SQL error: " + std::string(err_msg);
    sqlite3_free(err_msg);
    throw std::runtime_error(error_str);
  }
}

bool Database::isFirstRun() const {
  sqlite3_stmt* stmt;
  const char* sql = "SELECT value FROM GameState WHERE key = 'season';";
  if (sqlite3_prepare_v2(pImpl->db, sql, -1, &stmt, 0) != SQLITE_OK) {
    // This can happen if the table doesn't exist yet, which is a "first run"
    return true;
  }
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
  const char* sql = "INSERT INTO leagues (name) VALUES (?);";
  if (sqlite3_prepare_v2(pImpl->db, sql, -1, &stmt, 0) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
  }
  sqlite3_finalize(stmt);
}

std::vector<League> Database::getLeagues() const {
  sqlite3_stmt* stmt;
  const char* sql = "SELECT id, name FROM leagues;";
  std::vector<League> leagues;
  if (sqlite3_prepare_v2(pImpl->db, sql, -1, &stmt, 0) == SQLITE_OK) {
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      int id = sqlite3_column_int(stmt, 0);
      const unsigned char* name_text = sqlite3_column_text(stmt, 1);
      std::string name = name_text ? reinterpret_cast<const char*>(name_text) : "";
      leagues.emplace_back(id, name);
    }
  }
  sqlite3_finalize(stmt);
  return leagues;
}

void Database::addFirstName(const std::string& name) {
  sqlite3_stmt* stmt;
  const char* sql = "INSERT INTO FirstNames (name) VALUES (?);";
  if (sqlite3_prepare_v2(pImpl->db, sql, -1, &stmt, 0) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
  }
  sqlite3_finalize(stmt);
}

void Database::addLastName(const std::string& name) {
  sqlite3_stmt* stmt;
  const char* sql = "INSERT INTO LastNames (name) VALUES (?);";
  if (sqlite3_prepare_v2(pImpl->db, sql, -1, &stmt, 0) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
  }
  sqlite3_finalize(stmt);
}

std::vector<std::string> Database::getFirstNames() const {
  sqlite3_stmt* stmt;
  const char* sql = "SELECT name FROM FirstNames;";
  std::vector<std::string> names;
  if (sqlite3_prepare_v2(pImpl->db, sql, -1, &stmt, 0) == SQLITE_OK) {
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      const unsigned char* name_text = sqlite3_column_text(stmt, 0);
      names.emplace_back(reinterpret_cast<const char*>(name_text));
    }
  }
  sqlite3_finalize(stmt);
  return names;
}

std::vector<std::string> Database::getLastNames() const {
  sqlite3_stmt* stmt;
  const char* sql = "SELECT name FROM LastNames;";
  std::vector<std::string> names;
  if (sqlite3_prepare_v2(pImpl->db, sql, -1, &stmt, 0) == SQLITE_OK) {
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      const unsigned char* name_text = sqlite3_column_text(stmt, 0);
      names.emplace_back(reinterpret_cast<const char*>(name_text));
    }
  }
  sqlite3_finalize(stmt);
  return names;
}

void Database::addTeam(int league_id, const std::string& name, uint64_t balance) {
  sqlite3_stmt* stmt;
  const char* sql = "INSERT INTO Teams (league_id, name, balance) VALUES (?, ?, ?);";
  if (sqlite3_prepare_v2(pImpl->db, sql, -1, &stmt, 0) != SQLITE_OK) {
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
  const char* sql = "SELECT id, name, balance FROM Teams WHERE league_id = ?;";
  if (sqlite3_prepare_v2(pImpl->db, sql, -1, &stmt, 0) != SQLITE_OK) {
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
  const char* sql = "UPDATE Teams SET name = ?, balance = ? WHERE id = ?;";
  if (sqlite3_prepare_v2(pImpl->db, sql, -1, &stmt, 0) != SQLITE_OK) {
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
  const char* sql = "INSERT INTO Players (team_id, name, age, role, stats) VALUES (?, ?, ?, ?, ?);";
  if (sqlite3_prepare_v2(pImpl->db, sql, -1, &stmt, 0) == SQLITE_OK) {
    nlohmann::json stats_json = player.getStats();
    std::string stats_str = stats_json.dump();
    sqlite3_bind_int(stmt, 1, team_id);
    sqlite3_bind_text(stmt, 2, player.getName().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, player.getAge());
    sqlite3_bind_text(stmt, 4, player.getRole().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, stats_str.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
  }
  sqlite3_finalize(stmt);
}

std::vector<Player> Database::getPlayers(int team_id) const {
  sqlite3_stmt* stmt;
  const char* sql = "SELECT id, name, age, role, stats FROM Players WHERE team_id = ?;";
  std::vector<Player> players;
  if (sqlite3_prepare_v2(pImpl->db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
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
  }
  sqlite3_finalize(stmt);
  return players;
}

void Database::saveCalendar(const Calendar& calendar, int season, int league_id) {
  sqlite3_stmt* stmt;
  const char* sql = "INSERT INTO Calendar (season, week, home_team_id, away_team_id, league_id) VALUES (?, ?, ?, ?, ?);";
  if (sqlite3_prepare_v2(pImpl->db, sql, -1, &stmt, 0) == SQLITE_OK) {
    int week_num = 0;
    for (const auto& week : calendar.getWeeks()) {
      for (const auto& match : week.getMatches()) {
        sqlite3_bind_int(stmt, 1, season);
        sqlite3_bind_int(stmt, 2, week_num);
        sqlite3_bind_int(stmt, 3, match.home_team_id);
        sqlite3_bind_int(stmt, 4, match.away_team_id);
        sqlite3_bind_int(stmt, 5, league_id);
        sqlite3_step(stmt);
        sqlite3_reset(stmt);
      }
      week_num++;
    }
  }
  sqlite3_finalize(stmt);
}

Calendar Database::loadCalendar(int season, int league_id) const {
  sqlite3_stmt* stmt;
  const char* sql = "SELECT week, home_team_id, away_team_id FROM Calendar WHERE season = ? AND league_id = ? ORDER BY week;";
  Calendar calendar;
  std::vector<Week> weeks;
  if (sqlite3_prepare_v2(pImpl->db, sql, -1, &stmt, 0) == SQLITE_OK) {
    sqlite3_bind_int(stmt, 1, season);
    sqlite3_bind_int(stmt, 2, league_id);
    int current_week = -1;
    Week* week = nullptr;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      int week_num = sqlite3_column_int(stmt, 0);
      if (week_num != current_week) {
        if (week) {
          weeks.push_back(*week);
        }
        week = new Week(week_num);
        current_week = week_num;
      }
      week->addMatch({sqlite3_column_int(stmt, 1), sqlite3_column_int(stmt, 2)});
    }
    if (week) {
      weeks.push_back(*week);
      delete week;
    }
  }
  sqlite3_finalize(stmt);
  calendar.setWeeks(weeks);
  return calendar;
}

void Database::updateGameState(int season, int week, int managed_team_id) {
  sqlite3_stmt* stmt;

  const char* sql_season = "INSERT OR REPLACE INTO GameState (key, value) VALUES ('season', ?);";
  if (sqlite3_prepare_v2(pImpl->db, sql_season, -1, &stmt, 0) == SQLITE_OK) {
    sqlite3_bind_int(stmt, 1, season);
    sqlite3_step(stmt);
  }
  sqlite3_finalize(stmt);

  const char* sql_week = "INSERT OR REPLACE INTO GameState (key, value) VALUES ('week', ?);";
  if (sqlite3_prepare_v2(pImpl->db, sql_week, -1, &stmt, 0) == SQLITE_OK) {
    sqlite3_bind_int(stmt, 1, week);
    sqlite3_step(stmt);
  }
  sqlite3_finalize(stmt);

  const char* sql_managed_team = "INSERT OR REPLACE INTO GameState (key, value) VALUES ('managed_team_id', ?);";
  if (sqlite3_prepare_v2(pImpl->db, sql_managed_team, -1, &stmt, 0) == SQLITE_OK) {
    sqlite3_bind_int(stmt, 1, managed_team_id);
    sqlite3_step(stmt);
  }
  sqlite3_finalize(stmt);
}

void Database::loadGameState(int& season, int& week, int& managed_team_id) const {
  sqlite3_stmt* stmt;
  const char* sql = "SELECT key, value FROM GameState;";
  if (sqlite3_prepare_v2(pImpl->db, sql, -1, &stmt, 0) == SQLITE_OK) {
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
  }
  sqlite3_finalize(stmt);
}

int Database::loadManagedTeamId() const {
  sqlite3_stmt* stmt;
  const char* sql = "SELECT value FROM GameState WHERE key = 'managed_team_id';";
  int managed_team_id = -1;
  if (sqlite3_prepare_v2(pImpl->db, sql, -1, &stmt, 0) == SQLITE_OK) {
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      managed_team_id = sqlite3_column_int(stmt, 0);
    }
  }
  sqlite3_finalize(stmt);
  return managed_team_id;
}

void Database::saveManagedTeamId(int team_id) {
  sqlite3_stmt* stmt;
  const char* sql = "INSERT OR REPLACE INTO GameState (key, value) VALUES ('managed_team_id', ?);";
  if (sqlite3_prepare_v2(pImpl->db, sql, -1, &stmt, 0) == SQLITE_OK) {
    sqlite3_bind_int(stmt, 1, team_id);
    sqlite3_step(stmt);
  }
  sqlite3_finalize(stmt);
}

void Database::saveLeaguePoints(const League& league) {
  const char* sql = "INSERT OR REPLACE INTO LeaguePoints (league_id, team_id, points) VALUES (?, ?, ?);";
  sqlite3_stmt* stmt;
  if (sqlite3_prepare_v2(pImpl->db, sql, -1, &stmt, 0) != SQLITE_OK) {
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
  const char* sql = "SELECT team_id, points FROM LeaguePoints WHERE league_id = ?;";
  sqlite3_stmt* stmt;
  if (sqlite3_prepare_v2(pImpl->db, sql, -1, &stmt, 0) != SQLITE_OK) {
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

void Database::updatePlayer(const Player& player) {
  sqlite3_stmt* stmt;
  const char* sql = "UPDATE Players SET name = ?, age = ?, role = ?, stats = ? WHERE id = ?;";
  if (sqlite3_prepare_v2(pImpl->db, sql, -1, &stmt, 0) != SQLITE_OK) {
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
  const char* sql = "DELETE FROM Players WHERE id = ?;";
  if (sqlite3_prepare_v2(pImpl->db, sql, -1, &stmt, 0) != SQLITE_OK) {
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
  std::vector<Player> all_players;
  // Retrieve all players from the database
  sqlite3_stmt* stmt;
  const char* sql = "SELECT id, team_id, name, age, role, stats FROM Players;";
  if (sqlite3_prepare_v2(pImpl->db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
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
  }
  sqlite3_finalize(stmt);

  std::vector<int> retired_player_ids;
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
