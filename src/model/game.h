#pragma once

#include "database.h"
#include "league.h"
#include "manager.h"
#include "calendar.h"
#include <string>
#include <memory>
#include <vector>

class Game {
public:
   Game(Database& db);
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
   Database& db;
   std::vector<League> leagues;
   std::vector<Team> teams;
   std::map<int, Calendar> league_calendars; // Map league_id to Calendar
   int current_season;
   int current_week;
   int managed_team_id;
};
