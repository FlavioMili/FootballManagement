#include <iostream>
#include "game.h"
#include "datagenerator.h"

Game::Game(Database& db) : db(db), current_season(0), current_week(0), managed_team_id(-1) {
   loadData();

   // If it's a first run, generate all data
   if (db.isFirstRun()) {
      DataGenerator generator(db);
      generator.generateAll();
      // After generation, reload data to get newly created leagues/teams/players
      loadData();

      // Set initial game state for a new game
      if (!leagues.empty() && !teams.empty()) {
         current_season = 1;
         current_week = 0;
         // TODO make it a choice
         managed_team_id = teams[rand() % teams.size()].getId(); // Assign a random team as managed
         db.updateGameState(current_season, current_week, managed_team_id); // Save this initial state

         // Generate and save calendars for each league for the first season
         for (const auto& league : leagues) {
            Calendar league_calendar;
            std::vector<Team> teams_in_league = db.getTeams(league.getId());
            league_calendar.generate(teams_in_league);
            db.saveCalendar(league_calendar, current_season, league.getId());
            league_calendars[league.getId()] = league_calendar;
         }
      } else {
         std::cerr << "Error: No leagues or teams generated on first run. Cannot set initial game state.\n";
      }
   }

   // Ensure managed_team_id is set if it's still -1 (e.g., old save or error in loading)
   if (managed_team_id == -1 && !teams.empty()) {
      managed_team_id = teams[rand() % teams.size()].getId(); // Fallback to a random team
      db.saveManagedTeamId(managed_team_id);
   }

   // Display managed team only once, after it's definitively set
   if (managed_team_id != -1) {
      std::cout << "You have been appointed as the manager of: " 
         << getManagedTeam().getName() << "\n";
   }
}

void Game::loadData() {
   db.loadGameState(current_season, current_week, managed_team_id);
   // TODO maybe also clear players?
   leagues.clear();
   teams.clear();
   league_calendars.clear();

   leagues = db.getLeagues();
   for (const auto& league : leagues) {
      std::vector<Team> league_teams = db.getTeams(league.getId());
      teams.insert(teams.end(), league_teams.begin(), league_teams.end());
      // Load calendar for each league
      league_calendars[league.getId()] = db.loadCalendar(current_season, league.getId());
   }
}

Team& Game::getManagedTeam() {
   return getTeamById(managed_team_id);
}

void Game::advanceWeek() {
   // Get the calendar for the managed team's league
   int managed_league_id = getManagedTeam().getLeagueId();
   Calendar& current_calendar = league_calendars.at(managed_league_id);

   if (current_week >= (int)current_calendar.getWeeks().size()) {
      endSeason();
      startNewSeason();
   }
   simulateWeek();
}

void Game::simulateWeek() {
   int managed_league_id = getManagedTeam().getLeagueId();
   Calendar& current_calendar = league_calendars.at(managed_league_id);

   if (current_week >= (int)current_calendar.getWeeks().size()) {
      std::cout << "No more weeks to simulate in the current season.\n";
      return;
   }

   std::cout << "--- Simulating Week " << current_week + 1 << " ---\n";

   const auto& week_matches = current_calendar.getWeeks()[current_week].getMatches();
   for (const auto& match_info : week_matches) {
      Team& home_team = getTeamById(match_info.home_team_id);
      Team& away_team = getTeamById(match_info.away_team_id);
      Match match(home_team, away_team);
      match.simulate();
      updateStandings(match);

      if (home_team.getId() == managed_team_id || away_team.getId() == managed_team_id) {
         std::cout << "Your Team's Match Result: " << home_team.getName() << " " << match.getHomeScore()
            << " - " << match.getAwayScore() << " " << away_team.getName() << "\n";
      }
   }

   current_week++;
   db.updateGameState(current_season, current_week, managed_team_id);
}

void Game::updateStandings(const Match& match) {
   Team& home_team = match.getHomeTeam();
   Team& away_team = match.getAwayTeam();

   if (match.getHomeScore() > match.getAwayScore()) {
      home_team.addPoints(3);
   } else if (match.getHomeScore() < match.getAwayScore()) {
      away_team.addPoints(3);
   } else {
      home_team.addPoints(1);
      away_team.addPoints(1);
   }

   db.updateTeam(home_team);
   db.updateTeam(away_team);
}

void Game::endSeason() {
   std::cout << "--- Season " << current_season << " has concluded. ---\n";
   // TODO Display final leaderboard or other season-end information
}

void Game::startNewSeason() {
   current_season++;
   current_week = 0;
   db.updateGameState(current_season, current_week, managed_team_id);

   for (auto& team : teams) {
      team.resetPoints();
      db.updateTeam(team);
   }

   loadData(); // Reload data for the new season

   // Generate and save calendars for each league for the new season
   for (const auto& league : leagues) {
      Calendar league_calendar;
      std::vector<Team> teams_in_league = db.getTeams(league.getId());
      league_calendar.generate(teams_in_league);
      db.saveCalendar(league_calendar, current_season, league.getId());
      league_calendars[league.getId()] = league_calendar;
   }

   std::cout << "--- Generating schedule for " << leagues[0].getName() << " ---\n";
   std::cout << "Generated a schedule with " << league_calendars.at(leagues[0].getId()).getWeeks().size() << " weeks.\n";
}

int Game::getCurrentSeason() const {
   return current_season;
}

int Game::getCurrentWeek() const {
   return current_week;
}

const std::vector<Team>& Game::getTeams() const {
   return teams;
}

Team& Game::getTeamById(int team_id) {
   for (auto& team : teams) {
      if (team.getId() == team_id) {
         return team;
      }
   }
   throw std::runtime_error("Team not found with ID: " + std::to_string(team_id));
}

std::vector<Player> Game::getPlayersForTeam(int team_id) {
   return db.getPlayers(team_id);
}

std::vector<Team> Game::getTeamsInLeague(int league_id) {
   std::vector<Team> teams_in_league;
   for (const auto& team : teams) {
      if (team.getLeagueId() == league_id) {
         teams_in_league.push_back(team);
      }
   }
   return teams_in_league;
}
