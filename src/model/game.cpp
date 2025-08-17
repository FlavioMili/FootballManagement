#include "game.h"
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>

#include "datagenerator.h"
#include "global.h"
#include "json.hpp"

void from_json(const nlohmann::json& j, RoleFocus& rf) {
  j.at("stats").get_to(rf.stats);
  j.at("weights").get_to(rf.weights);
}

void from_json(const nlohmann::json& j, StatsConfig& sc) {
  j.at("role_focus").get_to(sc.role_focus);
  j.at("possible_stats").get_to(sc.possible_stats);
}

Game::Game(Database& db)
: db(db), current_season(1), 
  current_week(0), managed_team_id(FREE_AGENTS_TEAM_ID) 
{
  loadConfigs();

  if (db.isFirstRun()) {
    initializeDatabase();
  }

  loadData();

  /* TODO
   * This is now enforced in the view side, must be improved and made safer
   * ensureManagedTeamAssigned();
  */
}

void Game::loadConfigs() {
  {
    std::ifstream f(STATS_CONFIG_PATH);
    if (!f.is_open()) {
      throw std::runtime_error("FATAL: Could not open stats config file");
    }
    nlohmann::json stats_config_json = nlohmann::json::parse(f);
    stats_config = stats_config_json.get<StatsConfig>();
    raw_stats_config_json = std::move(stats_config_json);
  }

  league_names = loadJsonFileKey<std::vector<std::string>>(LEAGUE_NAMES_PATH, "names");
  team_names   = loadJsonFileKey<std::vector<std::string>>(TEAM_NAMES_PATH, "names");
}

void Game::initializeDatabase() {
  std::cout << "First run detected. Populating database with initial data...\n";

  auto first_names = loadJsonFileKey<std::vector<std::string>>(FIRST_NAMES_PATH, "names");
  for (const auto& name : first_names) db.addFirstName(name);

  auto last_names  = loadJsonFileKey<std::vector<std::string>>(LAST_NAMES_PATH, "names");
  for (const auto& name : last_names) db.addLastName(name);

  // Generate game data
  std::cout << "Generating new game data...\n";
  DataGenerator generator(db, raw_stats_config_json, league_names, team_names);
  generator.generateAll();

  // Verify generated data and get leagues from DB
  auto all_leagues = db.getLeagues();
  if (all_leagues.empty()) throw std::runtime_error("FATAL: No leagues generated.");
  auto teams_in_first_league = db.getTeams(all_leagues[0].getId());
  if (teams_in_first_league.empty()) throw std::runtime_error("FATAL: No teams generated.");

  // Assign free agents as initial managed team just to give a choice
  managed_team_id = FREE_AGENTS_TEAM_ID;
  db.updateGameState(current_season, current_week, managed_team_id);

  generateAllCalendars(all_leagues);
}

void Game::ensureManagedTeamAssigned() {
  if (managed_team_id != -1) {
    std::cout << "You have been appointed as the manager of: "
      << getManagedTeam().getName() << "\n";
  } else {
    throw std::runtime_error("FATAL: Could not load or assign a managed team.");
  }
}

void Game::loadData() {
  db.loadGameState(current_season, current_week, managed_team_id);
  leagues.clear();
  teams.clear();
  league_calendars.clear();

  leagues = db.getLeagues();
  for (auto& league : leagues) {
    std::vector<Team> league_teams = db.getTeams(league.getId());
    league.setTeams(league_teams);
    db.loadLeaguePoints(league);
    teams.insert(teams.end(), league_teams.begin(), league_teams.end());
    league_calendars[league.getId()] = db.loadCalendar(current_season, league.getId());
  }
}

const StatsConfig& Game::getStatsConfig() const { return stats_config; }

Team& Game::getManagedTeam() { return getTeamById(managed_team_id); }
const Team& Game::getManagedTeam() const { return getTeamById(managed_team_id); }

void Game::advanceWeek() {
  int managed_league_id = getManagedTeam().getLeagueId();
  const auto& calendar = league_calendars.at(managed_league_id);

  if (current_week >= static_cast<int>(calendar.getWeeks().size()) && !calendar.getWeeks().empty()) {
    handleSeasonTransition();
    return;
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

    getPlayersForTeam(home_team.getId());
    getPlayersForTeam(away_team.getId());

    Match match(home_team, away_team, stats_config);
    match.simulate();
    updateStandings(match);

    if (home_team.getId() == managed_team_id || away_team.getId() == managed_team_id) {
      std::cout << "Your Team's Match Result: " << home_team.getName() << " " << match.getHomeScore()
        << " - " << match.getAwayScore() << " " << away_team.getName() << "\n";
    }
    trainPlayers(home_team.getPlayers());
    trainPlayers(away_team.getPlayers());
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
  db.ageAllPlayers();
}

void Game::handleSeasonTransition() {
  endSeason();
  startNewSeason();
  loadData();
}

void Game::startNewSeason() {
  current_season++;
  current_week = 0;
  db.updateGameState(current_season, current_week, managed_team_id);
  db.resetAllLeaguePoints();

  for (auto& league : leagues) {
    league.resetPoints();
  }
  generateAllCalendars(leagues);

  if (!leagues.empty()) {
    std::cout << "--- Generating schedule for " << leagues[0].getName() << " ---\n";
    std::cout << "Generated a schedule with "
      << league_calendars.at(leagues[0].getId()).getWeeks().size() << " weeks.\n";
  }
}

int Game::getCurrentSeason() const { return current_season; }
int Game::getCurrentWeek() const { return current_week; }
const std::vector<Team>& Game::getTeams() const { return teams; }

Team& Game::getTeamById(int team_id) {
  for (auto& team : teams) {
    if (team.getId() == team_id) return team;
  }
  throw std::runtime_error("Team not found with ID: " + std::to_string(team_id));
}

const Team& Game::getTeamById(int team_id) const {
  for (const auto& team : teams) {
    if (team.getId() == team_id) return team;
  }
  throw std::runtime_error("Team not found with ID: " + std::to_string(team_id));
}

League& Game::getLeagueById(int league_id) {
  auto it = std::find_if(leagues.begin(), leagues.end(),
                         [league_id](const League& l) { return l.getId() == league_id; });
  if (it != leagues.end()) return *it;
  throw std::runtime_error("League not found with ID: " + std::to_string(league_id));
}

const League& Game::getLeagueById(int league_id) const {
  auto it = std::find_if(leagues.cbegin(), leagues.cend(),
                         [league_id](const League& l) { return l.getId() == league_id; });
  if (it != leagues.cend()) return *it;
  throw std::runtime_error("League not found with ID: " + std::to_string(league_id));
}

std::vector<Player>& Game::getPlayersForTeam(int team_id) {
  Team& team = getTeamById(team_id);
  if (team.getPlayers().empty()) {
    team.setPlayers(db.getPlayers(team_id));
  }
  return team.getPlayers();
}

std::vector<Team> Game::getTeamsInLeague(int league_id) {
  std::vector<Team> teams_in_league;
  for (const auto& team : teams) {
    if (team.getLeagueId() == league_id) teams_in_league.push_back(team);
  }
  return teams_in_league;
}

void Game::saveGame() {
  db.updateGameState(current_season, current_week, managed_team_id);
  for (const auto& league : leagues) {
    db.saveLeaguePoints(league);
  }
}

void Game::generateAllCalendars(const std::vector<League>& leagues_param) {
  for (const auto& league : leagues_param) {
    Calendar league_calendar;
    std::vector<Team> teams_in_league = db.getTeams(league.getId());
    league_calendar.generate(teams_in_league);
    db.saveCalendar(league_calendar, current_season, league.getId());

    league_calendars[league.getId()] = league_calendar;
  }
}

void Game::trainPlayers(std::vector<Player>& players) {
  for (auto& p : players) {
    const auto& focus_stats = stats_config.role_focus.at(p.getRole()).stats;
    p.train(focus_stats);
    db.updatePlayer(p);
  }
}

void Game::selectManagedTeam(int team_id) {
  auto it = std::find_if(teams.begin(), teams.end(),
    [team_id](const Team& t) { return t.getId() == team_id; });

  if (it == teams.end()) throw std::runtime_error("Invalid team ID");

  managed_team_id = team_id;
  db.saveManagedTeamId(team_id);
  std::cout << "You are now managing: " << getTeamById(team_id).getName() << std::endl;
}

bool Game::hasSelectedTeam() const {
  return managed_team_id != FREE_AGENTS_TEAM_ID;
}

std::vector<Team> Game::getAvailableTeams() const {
  std::vector<Team> available_teams;
  for (const auto& team : teams) {
    if (team.getId() != FREE_AGENTS_TEAM_ID) {
      available_teams.push_back(team);
    }
  }
  return available_teams;
}
