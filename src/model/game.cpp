#include "game.h"
#include <iostream>
#include "datagenerator.h"

Game::Game(Database& db)
: db(db), current_season(1),
  current_week(0), managed_team_id(-1) {
  if (db.isFirstRun()) {
    std::cout << "First run detected. Generating new game data...\n";
    DataGenerator generator(db);
    generator.generateAll();

    // We need to get the generated teams to pick one to manage.
    // We'll just get teams from the first league for simplicity.
    std::vector<League> all_leagues = db.getLeagues();
    if (all_leagues.empty()) {
      throw std::runtime_error("FATAL: No leagues were generated on first run.");
    }
    std::vector<Team> teams_in_first_league = db.getTeams(all_leagues[0].getId());
    if (teams_in_first_league.empty()) {
      throw std::runtime_error("FATAL: No teams were generated on first run.");
    }
    managed_team_id = teams_in_first_league[rand() % teams_in_first_league.size()].getId();

    // Save the very first state of the game
    db.updateGameState(current_season, current_week, managed_team_id);

    // Generate and save calendars for the first season
    for (const auto& league : all_leagues) {
      Calendar league_calendar;
      std::vector<Team> teams_in_league = db.getTeams(league.getId());
      league_calendar.generate(teams_in_league);
      db.saveCalendar(league_calendar, current_season, league.getId());
    }
  }

  // Load all game data. On a first run, this will load the data that was
  // just generated. On subsequent runs, it will load the saved game state.
  loadData();

  if (managed_team_id != -1) {
    std::cout << "You have been appointed as the manager of: "
              << getManagedTeam().getName() << "\n";
  } else {
      throw std::runtime_error("FATAL: Could not load or assign a managed team.");
  }
}

void Game::loadData() {
  db.loadGameState(current_season, current_week, managed_team_id);
  // TODO maybe also clear players?
  leagues.clear();
  teams.clear();
  league_calendars.clear();

  leagues = db.getLeagues();
  for (auto& league : leagues) {
    std::vector<Team> league_teams = db.getTeams(league.getId());
    league.setTeams(league_teams);
    db.loadLeaguePoints(league);
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

  if (current_week >= static_cast<int>(current_calendar.getWeeks().size())) {
    endSeason();
    startNewSeason();
  }
  simulateWeek();
}

void Game::simulateWeek() {
  int managed_league_id = getManagedTeam().getLeagueId();
  Calendar& current_calendar = league_calendars.at(managed_league_id);

  if (current_week >= static_cast<int>(current_calendar.getWeeks().size())) {
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
  Team& home_team = getTeamById(match.getHomeTeam().getId());
  Team& away_team = getTeamById(match.getAwayTeam().getId());
  League& league = getLeagueById(home_team.getLeagueId());

  if (match.getHomeScore() > match.getAwayScore()) {
    league.addPoints(home_team.getId(), 3);
  } else if (match.getHomeScore() < match.getAwayScore()) {
    league.addPoints(away_team.getId(), 3);
  } else {
    league.addPoints(home_team.getId(), 1);
    league.addPoints(away_team.getId(), 1);
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

  for (auto& league : leagues) {
    league.resetPoints();
  }

  loadData();  // Reload data for the new season

  // Generate and save calendars for each league for the new season
  for (const auto& league : leagues) {
    Calendar league_calendar;
    std::vector<Team> teams_in_league = db.getTeams(league.getId());
    league_calendar.generate(teams_in_league);
    db.saveCalendar(league_calendar, current_season, league.getId());
    league_calendars[league.getId()] = league_calendar;
  }

  std::cout << "--- Generating schedule for " << leagues[0].getName() << " ---\n";
  std::cout << "Generated a schedule with " <<
    league_calendars.at(leagues[0].getId()).getWeeks().size() << " weeks.\n";
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

League& Game::getLeagueById(int league_id) {
    auto it = std::find_if(leagues.begin(), leagues.end(),
                           [league_id](const League& l) { return l.getId() == league_id; });
    if (it != leagues.end()) return *it;
    throw std::runtime_error("League not found with ID: " + std::to_string(league_id));
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

void Game::saveGame() {
  db.updateGameState(current_season, current_week, managed_team_id);
  for(const auto& league : leagues) {
    db.saveLeaguePoints(league);
  }
}
