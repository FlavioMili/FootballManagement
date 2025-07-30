#pragma once

#include <vector>
#include <map>
#include "database.h"
#include "league.h"
#include "calendar.h"
#include "stats_config.h"

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
  Team& getTeamById(int team_id);
  League& getLeagueById(int league_id);
  Team& getManagedTeam();
  void updateStandings(const Match& match);
  std::vector<Player> getPlayersForTeam(int team_id);
  std::vector<Team> getTeamsInLeague(int league_id);
  void saveGame();

 private:
  Database& db;
  StatsConfig stats_config;
  std::vector<std::string> league_names;
  std::vector<std::string> team_names;
  std::vector<League> leagues;
  std::vector<Team> teams;
  std::map<int, Calendar> league_calendars;  // Map league_id to Calendar
  int current_season;
  int current_week;
  int managed_team_id;
  void generateAllCalendars(const std::vector<League> leagues);
  void trainPlayers(std::vector<Player>& players);
};
