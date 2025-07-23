#pragma once

#include "player.h"
#include "team.h"
#include "league.h"
#include "calendar.h"
#include <string>
#include <vector>
#include <memory>

class Database {
public:
   Database(const std::string& db_path);
   ~Database();

   void initialize();
   bool isFirstRun();

   void addLeague(const std::string& name);
   std::vector<League> getLeagues();

   void addTeam(int league_id, const std::string& name, long long balance);
   std::vector<Team> getTeams(int league_id);
   void updateTeam(const Team& team);

   void addPlayer(int team_id, const Player& player);
   std::vector<Player> getPlayers(int team_id);

   void saveCalendar(const Calendar& calendar, int season, int league_id);
   Calendar loadCalendar(int season, int league_id);

   void updateGameState(int season, int week, int managed_team_id);
   int loadManagedTeamId();
   void saveManagedTeamId(int team_id);
   void loadGameState(int& season, int& week, int& managed_team_id);

private:
   struct Impl;
   std::unique_ptr<Impl> pImpl;
};
