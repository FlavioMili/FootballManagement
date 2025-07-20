
#pragma once

#include <string>
#include <vector>
#include <sqlite3.h>

#include <map>

class Database {
public:
    Database(const std::string& db_path);
    ~Database();

    void initialize();
    bool isInitialized();

    // Transaction handling
    void beginTransaction();
    void commitTransaction();

    // Granular save methods
    long long saveLeague(const std::string& name);
    long long saveTeam(const std::string& name, const long long* league_id);
    void savePlayer(const std::string& name, int age, long long team_id, const std::map<std::string, int>& stats);


private:
    sqlite3* db;
    void executeSQL(const std::string& sql);
};
