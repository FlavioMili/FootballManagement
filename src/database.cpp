#include "database.h"
#include <sqlite3.h>
#include <iostream>
#include <vector>
#include "league.h"
#include "team.h"
#include "player.h"

Database::Database(const std::string& db_path) : db(nullptr) {
    if (sqlite3_open(db_path.c_str(), &db)) {
        std::cerr << "FATAL: Can't open database: " << sqlite3_errmsg(db) << std::endl;
        db = nullptr;
    }
}

Database::~Database() {
    if (db) {
        sqlite3_close(db);
    }
}

void Database::executeSQL(const std::string& sql) {
    char* err_msg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << err_msg << std::endl;
        sqlite3_free(err_msg);
    }
}

void Database::initialize() {
    std::cout << "Initializing database schema..." << std::endl;
    executeSQL("PRAGMA foreign_keys = ON;");

    const std::string create_leagues_table =
        "CREATE TABLE IF NOT EXISTS leagues ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL UNIQUE);";

    const std::string create_teams_table =
        "CREATE TABLE IF NOT EXISTS teams ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL UNIQUE,"
        "balance INTEGER NOT NULL,"
        "league_id INTEGER,"
        "FOREIGN KEY(league_id) REFERENCES leagues(id));";

    const std::string create_players_table =
        "CREATE TABLE IF NOT EXISTS players ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL,"
        "age INTEGER NOT NULL,"
        "team_id INTEGER NOT NULL,"
        "FOREIGN KEY(team_id) REFERENCES teams(id));";

    const std::string create_player_stats_table =
        "CREATE TABLE IF NOT EXISTS player_stats ("
        "player_id INTEGER NOT NULL,"
        "stat_name TEXT NOT NULL,"
        "stat_value INTEGER NOT NULL,"
        "PRIMARY KEY (player_id, stat_name),"
        "FOREIGN KEY(player_id) REFERENCES players(id));";
    
    const std::string create_metadata_table =
        "CREATE TABLE IF NOT EXISTS metadata ("
        "key TEXT PRIMARY KEY, value TEXT);";

    beginTransaction();
    executeSQL(create_leagues_table);
    executeSQL(create_teams_table);
    executeSQL(create_players_table);
    executeSQL(create_player_stats_table);
    executeSQL(create_metadata_table);
    executeSQL("INSERT OR IGNORE INTO metadata (key, value) VALUES ('initialized', 'true');");
    commitTransaction();
    std::cout << "Database schema initialized." << std::endl;
}

bool Database::isInitialized() {
    sqlite3_stmt* stmt;
    const std::string sql = "SELECT value FROM metadata WHERE key = 'initialized';";
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0);
    
    if (rc != SQLITE_OK) {
        return false;
    }

    rc = sqlite3_step(stmt);
    bool initialized = (rc == SQLITE_ROW);
    sqlite3_finalize(stmt);
    
    return initialized;
}


void Database::beginTransaction() {
    executeSQL("BEGIN TRANSACTION;");
}

void Database::commitTransaction() {
    executeSQL("COMMIT;");
}

long long Database::saveLeague(const std::string& name) {
    const std::string sql = "INSERT INTO leagues (name) VALUES (?);";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "Error inserting league: " << sqlite3_errmsg(db) << std::endl;
    }
    
    sqlite3_finalize(stmt);
    return sqlite3_last_insert_rowid(db);
}

long long Database::saveTeam(const std::string& name, long long balance, const long long* league_id) {
    const std::string sql = "INSERT INTO teams (name, balance, league_id) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, balance);

    if (league_id) {
        sqlite3_bind_int64(stmt, 3, *league_id);
    } else {
        sqlite3_bind_null(stmt, 3);
    }
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "Error inserting team: " << sqlite3_errmsg(db) << std::endl;
    }

    sqlite3_finalize(stmt);
    return sqlite3_last_insert_rowid(db);
}

void Database::savePlayer(const std::string& name, int age, long long team_id, const std::map<std::string, int>& stats) {
    const std::string player_sql = "INSERT INTO players (name, age, team_id) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, player_sql.c_str(), -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, age);
    sqlite3_bind_int64(stmt, 3, team_id);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "Error inserting player: " << sqlite3_errmsg(db) << std::endl;
    }
    sqlite3_finalize(stmt);
    long long player_id = sqlite3_last_insert_rowid(db);

    const std::string stats_sql = "INSERT INTO player_stats (player_id, stat_name, stat_value) VALUES (?, ?, ?);";
    for (const auto& pair : stats) {
        sqlite3_prepare_v2(db, stats_sql.c_str(), -1, &stmt, 0);
        sqlite3_bind_int64(stmt, 1, player_id);
        sqlite3_bind_text(stmt, 2, pair.first.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, pair.second);
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::cerr << "Error inserting player stat: " << sqlite3_errmsg(db) << std::endl;
        }
        sqlite3_finalize(stmt);
    }
}

void Database::setMetadata(const std::string& key, const std::string& value) {
    const std::string sql = "INSERT OR REPLACE INTO metadata (key, value) VALUES (?, ?);";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, value.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "Error setting metadata: " << sqlite3_errmsg(db) << std::endl;
    }
    sqlite3_finalize(stmt);
}

std::string Database::getMetadata(const std::string& key) {
    sqlite3_stmt* stmt;
    const std::string sql = "SELECT value FROM metadata WHERE key = ?;";
    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC);

    std::string value = "";
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        value = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
    }
    sqlite3_finalize(stmt);
    return value;
}


League Database::loadLeague(long long league_id) {
    sqlite3_stmt* stmt;
    const std::string sql = "SELECT name FROM leagues WHERE id = ?;";
    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0);
    sqlite3_bind_int64(stmt, 1, league_id);

    League league("Unknown League");
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        league.setName(std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0))));
    }
    sqlite3_finalize(stmt);

    // Load teams
    std::vector<Team> teams;
    const std::string teams_sql = "SELECT id, name, balance FROM teams WHERE league_id = ?;";
    sqlite3_prepare_v2(db, teams_sql.c_str(), -1, &stmt, 0);
    sqlite3_bind_int64(stmt, 1, league_id);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        long long team_id = sqlite3_column_int64(stmt, 0);
        std::string team_name = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
        long long balance = sqlite3_column_int64(stmt, 2);
        
        Team team(team_name);
        team.setBalance(balance);
        team.setPlayers(loadPlayersForTeam(team_id));
        teams.push_back(team);
    }
    sqlite3_finalize(stmt);
    league.overrideTeams(teams);

    return league;
}

std::vector<Player> Database::loadPlayersForTeam(long long team_id) {
    std::vector<Player> players;
    sqlite3_stmt* stmt;
    const std::string sql = "SELECT id, name, age FROM players WHERE team_id = ?;";
    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0);
    sqlite3_bind_int64(stmt, 1, team_id);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        long long player_id = sqlite3_column_int64(stmt, 0);
        std::string player_name = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
        int age = sqlite3_column_int(stmt, 2);

        Player player(player_name, age);

        // Load stats for the player
        sqlite3_stmt* stats_stmt;
        const std::string stats_sql = "SELECT stat_name, stat_value FROM player_stats WHERE player_id = ?;";
        sqlite3_prepare_v2(db, stats_sql.c_str(), -1, &stats_stmt, 0);
        sqlite3_bind_int64(stats_stmt, 1, player_id);
        
        std::map<std::string, int> stats;
        while (sqlite3_step(stats_stmt) == SQLITE_ROW) {
            std::string stat_name = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stats_stmt, 0)));
            int stat_value = sqlite3_column_int(stats_stmt, 1);
            stats[stat_name] = stat_value;
        }
        sqlite3_finalize(stats_stmt);
        player.setStats(stats);
        players.push_back(player);
    }
    sqlite3_finalize(stmt);
    return players;
}