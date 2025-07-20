#pragma once

#include <string>
#include <vector>
#include <map>

// Forward declarations to prevent circular dependencies
class League;
class Player;
struct sqlite3;

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
    long long saveTeam(const std::string& name, long long balance, const long long* league_id);
    void savePlayer(const std::string& name, int age, long long team_id, const std::map<std::string, int>& stats);

    // Metadata methods
    void setMetadata(const std::string& key, const std::string& value);
    std::string getMetadata(const std::string& key);

    // Load methods
    League loadLeague(long long league_id);

private:
    sqlite3* db;
    void executeSQL(const std::string& sql);
    std::vector<Player> loadPlayersForTeam(long long team_id);
};