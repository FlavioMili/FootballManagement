#include "database.h"
#include <sqlite3.h>
#include <iostream>
#include "json.hpp"

struct Database::Impl {
   sqlite3* db;
};

Database::Database(const std::string& db_path) : pImpl(std::make_unique<Impl>()) {
   if (sqlite3_open(db_path.c_str(), &pImpl->db)) {
      std::cerr << "Can't open database: " << sqlite3_errmsg(pImpl->db) << "\n";
   }
}

Database::~Database() {
   sqlite3_close(pImpl->db);
}

void Database::initialize() {
   const char* sql =
      "CREATE TABLE IF NOT EXISTS game_state (key TEXT PRIMARY KEY, value INTEGER);"
      "INSERT OR IGNORE INTO game_state (key, value) VALUES ('initialized', 0);" /* Initialize with 0, will be set to 1 after first run */
      "INSERT OR IGNORE INTO game_state (key, value) VALUES ('managed_team_id', -1);" // Initialize with -1 or a suitable default
      "CREATE TABLE IF NOT EXISTS leagues (id INTEGER PRIMARY KEY, name TEXT);"
      "CREATE TABLE IF NOT EXISTS teams (id INTEGER PRIMARY KEY, league_id INTEGER, name TEXT, balance INTEGER, points INTEGER);"
      "CREATE TABLE IF NOT EXISTS players (id INTEGER PRIMARY KEY, team_id INTEGER, name TEXT, age INTEGER, role TEXT, stats TEXT);"
      "CREATE TABLE IF NOT EXISTS calendar (season INTEGER, week INTEGER, home_team_id INTEGER, away_team_id INTEGER, league_id INTEGER);";
   char* err_msg = 0;
   if (sqlite3_exec(pImpl->db, sql, 0, 0, &err_msg) != SQLITE_OK) {
      std::cerr << "SQL error: " << err_msg << "\n";
      sqlite3_free(err_msg);
   }
}

/**
* This can be later improved by removing the initialized 
* value and just considering the state of weeks passed.
* There could be a problem when quitting immediately 
* after starting the game
*/
bool Database::isFirstRun() {
   sqlite3_stmt* stmt;
   const char* sql = "SELECT value FROM game_state WHERE key = 'initialized';";
   bool first_run = true;
   if (sqlite3_prepare_v2(pImpl->db, sql, -1, &stmt, 0) == SQLITE_OK) {
      if (sqlite3_step(stmt) == SQLITE_ROW) {
         first_run = (sqlite3_column_int(stmt, 0) == 0);
      }
   }
   sqlite3_finalize(stmt);
   if (first_run) {
      const char* insert_sql = "INSERT OR REPLACE INTO game_state (key, value) VALUES ('initialized', 1);";
      sqlite3_exec(pImpl->db, insert_sql, 0, 0, 0);
   }
   return first_run;
}

void Database::addLeague(const std::string& name) {
   sqlite3_stmt* stmt;
   const char* sql = "INSERT INTO leagues (name) VALUES (?);";
   if (sqlite3_prepare_v2(pImpl->db, sql, -1, &stmt, 0) == SQLITE_OK) {
      sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
      sqlite3_step(stmt);
   }
   sqlite3_finalize(stmt);
}

std::vector<League> Database::getLeagues() {
   sqlite3_stmt* stmt;
   const char* sql = "SELECT id, name FROM leagues;";
   std::vector<League> leagues;
   if (sqlite3_prepare_v2(pImpl->db, sql, -1, &stmt, 0) == SQLITE_OK) {
      while (sqlite3_step(stmt) == SQLITE_ROW) {
         leagues.emplace_back(sqlite3_column_int(stmt, 0), (const char*)sqlite3_column_text(stmt, 1));
      }
   }
   sqlite3_finalize(stmt);
   return leagues;
}

void Database::addTeam(int league_id, const std::string& name, long long balance) {
   sqlite3_stmt* stmt;
   const char* sql = "INSERT INTO teams (league_id, name, balance, points) VALUES (?, ?, ?, 0);";
   if (sqlite3_prepare_v2(pImpl->db, sql, -1, &stmt, 0) == SQLITE_OK) {
      sqlite3_bind_int(stmt, 1, league_id);
      sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_STATIC);
      sqlite3_bind_int64(stmt, 3, balance);
      sqlite3_step(stmt);
   }
   sqlite3_finalize(stmt);
}

std::vector<Team> Database::getTeams(int league_id) {
   sqlite3_stmt* stmt;
   const char* sql = "SELECT id, league_id, name, balance, points FROM teams WHERE league_id = ?;";
   std::vector<Team> teams;
   if (sqlite3_prepare_v2(pImpl->db, sql, -1, &stmt, 0) == SQLITE_OK) {
      sqlite3_bind_int(stmt, 1, league_id);
      while (sqlite3_step(stmt) == SQLITE_ROW) {
         Team team(sqlite3_column_int(stmt, 0), sqlite3_column_int(stmt, 1), (const char*)sqlite3_column_text(stmt, 2), sqlite3_column_int64(stmt, 3));
         team.addPoints(sqlite3_column_int(stmt, 4));
         teams.push_back(team);
      }
   }
   sqlite3_finalize(stmt);
   return teams;
}

void Database::updateTeam(const Team& team) {
   sqlite3_stmt* stmt;
   const char* sql = "UPDATE teams SET balance = ?, points = ? WHERE id = ?;";
   if (sqlite3_prepare_v2(pImpl->db, sql, -1, &stmt, 0) == SQLITE_OK) {
      sqlite3_bind_int64(stmt, 1, team.getBalance());
      sqlite3_bind_int(stmt, 2, team.getPoints());
      sqlite3_bind_int(stmt, 3, team.getId());
      sqlite3_step(stmt);
   }
   sqlite3_finalize(stmt);
}

void Database::addPlayer(int team_id, const Player& player) {
   sqlite3_stmt* stmt;
   const char* sql = "INSERT INTO players (team_id, name, age, role, stats) VALUES (?, ?, ?, ?, ?);";
   if (sqlite3_prepare_v2(pImpl->db, sql, -1, &stmt, 0) == SQLITE_OK) {
      nlohmann::json stats_json = player.getStats();
      std::string stats_str = stats_json.dump();
      sqlite3_bind_int(stmt, 1, team_id);
      sqlite3_bind_text(stmt, 2, player.getName().c_str(), -1, SQLITE_STATIC);
      sqlite3_bind_int(stmt, 3, player.getAge());
      sqlite3_bind_text(stmt, 4, player.getRole().c_str(), -1, SQLITE_STATIC);
      sqlite3_bind_text(stmt, 5, stats_str.c_str(), -1, SQLITE_STATIC);
      sqlite3_step(stmt);
   }
   sqlite3_finalize(stmt);
}

std::vector<Player> Database::getPlayers(int team_id) {
   sqlite3_stmt* stmt;
   const char* sql = "SELECT id, name, age, role, stats FROM players WHERE team_id = ?;";
   std::vector<Player> players;
   if (sqlite3_prepare_v2(pImpl->db, sql, -1, &stmt, 0) == SQLITE_OK) {
      sqlite3_bind_int(stmt, 1, team_id);
      while (sqlite3_step(stmt) == SQLITE_ROW) {
         Player player(sqlite3_column_int(stmt, 0), (const char*)sqlite3_column_text(stmt, 1), sqlite3_column_int(stmt, 2), (const char*)sqlite3_column_text(stmt, 3));
         nlohmann::json stats_json = nlohmann::json::parse((const char*)sqlite3_column_text(stmt, 4));
         player.setStats(stats_json.get<std::map<std::string, int>>());
         players.push_back(player);
      }
   }
   sqlite3_finalize(stmt);
   return players;
}

void Database::saveCalendar(const Calendar& calendar, int season, int league_id) {
   sqlite3_stmt* stmt;
   const char* sql = "INSERT INTO calendar (season, week, home_team_id, away_team_id, league_id) VALUES (?, ?, ?, ?, ?);";
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

Calendar Database::loadCalendar(int season, int league_id) {
   sqlite3_stmt* stmt;
   const char* sql = "SELECT week, home_team_id, away_team_id FROM calendar WHERE season = ? AND league_id = ? ORDER BY week;";
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

   const char* sql_season = "INSERT OR REPLACE INTO game_state (key, value) VALUES ('season', ?);";
   if (sqlite3_prepare_v2(pImpl->db, sql_season, -1, &stmt, 0) == SQLITE_OK) {
      sqlite3_bind_int(stmt, 1, season);
      sqlite3_step(stmt);
   }
   sqlite3_finalize(stmt);

   const char* sql_week = "INSERT OR REPLACE INTO game_state (key, value) VALUES ('week', ?);";
   if (sqlite3_prepare_v2(pImpl->db, sql_week, -1, &stmt, 0) == SQLITE_OK) {
      sqlite3_bind_int(stmt, 1, week);
      sqlite3_step(stmt);
   }
   sqlite3_finalize(stmt);

   const char* sql_managed_team = "INSERT OR REPLACE INTO game_state (key, value) VALUES ('managed_team_id', ?);";
   if (sqlite3_prepare_v2(pImpl->db, sql_managed_team, -1, &stmt, 0) == SQLITE_OK) {
      sqlite3_bind_int(stmt, 1, managed_team_id);
      sqlite3_step(stmt);
   }
   sqlite3_finalize(stmt);
}

void Database::loadGameState(int& season, int& week, int& managed_team_id) {
   sqlite3_stmt* stmt;
   const char* sql = "SELECT key, value FROM game_state;";
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

int Database::loadManagedTeamId() {
   sqlite3_stmt* stmt;
   const char* sql = "SELECT value FROM game_state WHERE key = 'managed_team_id';";
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
   const char* sql = "INSERT OR REPLACE INTO game_state (key, value) VALUES ('managed_team_id', ?);";
   if (sqlite3_prepare_v2(pImpl->db, sql, -1, &stmt, 0) == SQLITE_OK) {
      sqlite3_bind_int(stmt, 1, team_id);
      sqlite3_step(stmt);
   }
   sqlite3_finalize(stmt);
}
