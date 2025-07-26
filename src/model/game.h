#pragma once

#include <vector>
#include <map>
#include "database.h"
#include "league.h"
#include "calendar.h"

class Game {
 public:
  explicit Game(const Database& db);
  void simulateWeek();
  void startNewSeason();
  void loadData();
  void advanceWeek();
  void endSeason();

  int getCurrentSeason() const;
  int getCurrentWeek() const;
  const std::vector<Team>& getTeams() const;
  Team& getTeamById(int team_id);
  Team& getManagedTeam();
  void updateStandings(const Match& match);
  std::vector<Player> getPlayersForTeam(int team_id);
  std::vector<Team> getTeamsInLeague(int league_id);

 private:
  const Database& db;
  std::vector<League> leagues;
  std::vector<Team> teams;
  std::map<int, Calendar> league_calendars;  // Map league_id to Calendar
  int current_season;
  int current_week;
  int managed_team_id;
};
