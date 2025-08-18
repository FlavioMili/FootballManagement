#pragma once

#include <vector>
#include <map>
#include <string>
#include <fstream>
#include "json.hpp"

#include "database.h"
#include "league.h"
#include "calendar.h"
#include "stats_config.h"
#include "player.h"
#include "match.h"
#include "team.h"

/** This is the main class where things happen
  * It could possibly be simplified by making an 
  * Event class, and then manage different type of
  * events, considering that we should randomize the 
  * AI behaviours generating random events.
*/

class Game {
public:
  explicit Game(Database& db);

  void simulateWeek();
  void startNewSeason();
  void loadData();
  void advanceWeek();
  void endSeason();
  void handleSeasonTransition();

  int getCurrentSeason() const;
  int getCurrentWeek() const;
  const StatsConfig& getStatsConfig() const;

  const std::vector<Team>& getTeams() const;

  // both overloads
  Team& getTeamById(int team_id);
  const Team& getTeamById(int team_id) const;

  League& getLeagueById(int league_id);
  const League& getLeagueById(int league_id) const;

  Team& getManagedTeam();
  const Team& getManagedTeam() const;

  void updateStandings(const Match& match);

  std::vector<Player>& getPlayersForTeam(int team_id);
  const std::vector<Player>& getPlayersForTeam(int team_id) const;

  std::vector<Team> getTeamsInLeague(int league_id);
  const std::vector<Team> getTeamsInLeague(int league_id) const;

  void selectManagedTeam(int team_id);
  std::vector<Team> getAvailableTeams() const;
  bool hasSelectedTeam() const;

  void saveGame();

private:
  Database& db;
  StatsConfig stats_config;
  nlohmann::json raw_stats_config_json;
  std::vector<std::string> league_names;
  std::vector<std::string> team_names;
  std::vector<League> leagues;
  std::vector<Team> teams;
  std::map<int, Calendar> league_calendars;  // league_id to Calendar

  int current_season;
  int current_week;
  int managed_team_id;

  void loadConfigs();
  void initializeDatabase();
  void ensureManagedTeamAssigned();

  void generateAllCalendars(const std::vector<League>& leagues);
  void trainPlayers(std::vector<Player>& players);

  template<typename T>
  T loadJsonFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
      throw std::runtime_error("FATAL: Could not open " + path);
    }
    return nlohmann::json::parse(f).get<T>();
  }

  template<typename T>
  T loadJsonFileKey(const std::string& path, const std::string& key) {
    std::ifstream f(path);
    if (!f.is_open()) {
      throw std::runtime_error("FATAL: Could not open " + path);
    }
    return nlohmann::json::parse(f).at(key).get<T>();
  }
};
