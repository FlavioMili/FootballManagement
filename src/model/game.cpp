// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "game.h"
#include "database/datagenerator.h"
#include "database/gamedata.h"
#include "global/global.h"
#include "global/logger.h"
#include "global/paths.h"
#include "json.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>

void from_json(const nlohmann::json &j, RoleFocus &rf) {
  j.at("stats").get_to(rf.stats);
  j.at("weights").get_to(rf.weights);
}

void from_json(const nlohmann::json &j, StatsConfig &sc) {
  j.at("role_focus").get_to(sc.role_focus);
  j.at("possible_stats").get_to(sc.possible_stats);
}

Game::Game()
    : db(std::make_shared<Database>()), current_season(1), current_week(0),
      managed_team_id(FREE_AGENTS_TEAM_ID) {
  loadConfigs();
  GameData::instance().loadFromDB();
  loadData();
}

void Game::loadConfigs() {
  std::ifstream f(STATS_CONFIG_PATH);
  if (!f.is_open()) {
    throw std::runtime_error("FATAL: Could not open stats config file");
  }
  nlohmann::json stats_config_json = nlohmann::json::parse(f);
  stats_config = stats_config_json.get<StatsConfig>();
}

void Game::loadData() {
  int season, week, team_id;
  db->loadGameState(season, week, team_id);
  current_season = season;
  current_week = week;
  managed_team_id = team_id;
}

const StatsConfig &Game::getStatsConfig() const { return stats_config; }

Team &Game::getManagedTeam() {
  return GameData::instance().getTeams().at(managed_team_id);
}

const Team &Game::getManagedTeam() const {
  return GameData::instance().getTeams().at(managed_team_id);
}

void Game::advanceWeek() {
  int managed_league_id = getManagedTeam().getLeagueId();
  const auto &calendar = league_calendars.at(managed_league_id);

  if (current_week >= static_cast<int>(calendar.getWeeks().size()) &&
      !calendar.getWeeks().empty()) {
    handleSeasonTransition();
    return;
  }
  simulateWeek();
}

void Game::simulateWeek() {
  int managed_league_id = getManagedTeam().getLeagueId();
  Calendar &current_calendar = league_calendars.at(managed_league_id);

  if (current_week >= static_cast<int>(current_calendar.getWeeks().size())) {
    std::cout << "No more weeks to simulate in the current season.\n";
    return;
  }

  std::cout << "--- Simulating Week " << current_week + 1 << " ---\n";
  const auto &week_matches =
      current_calendar.getWeeks()[current_week].getMatches();

  for (const auto &match_info : week_matches) {
    Team &home_team = GameData::instance().getTeams().at(match_info.home_team_id);
    Team &away_team = GameData::instance().getTeams().at(match_info.away_team_id);

    Match match(home_team, away_team, stats_config);
    match.simulate();
    updateStandings(match);

    if (home_team.getId() == managed_team_id ||
        away_team.getId() == managed_team_id) {
      std::cout << "Your Team's Match Result: " << home_team.getName() << " "
                << match.getHomeScore() << " - " << match.getAwayScore() << " "
                << away_team.getName() << "\n";
    }
    trainPlayers(home_team.getPlayerIDs());
    trainPlayers(away_team.getPlayerIDs());
  }

  current_week++;
  db->updateGameState(current_season, current_week, managed_team_id);
}

void Game::updateStandings(const Match &match) {
  Team &home_team = GameData::instance().getTeams().at(match.getHomeTeam().getId());
  Team &away_team = GameData::instance().getTeams().at(match.getAwayTeam().getId());
  League &league = GameData::instance().getLeagues().at(home_team.getLeagueId());

  if (match.getHomeScore() > match.getAwayScore()) {
    league.addPoints(home_team.getId(), 3);
  } else if (match.getHomeScore() < match.getAwayScore()) {
    league.addPoints(away_team.getId(), 3);
  } else {
    league.addPoints(home_team.getId(), 1);
    league.addPoints(away_team.getId(), 1);
  }
}

void Game::endSeason() {
  std::cout << "--- Season " << current_season << " has concluded. ---\n";
  db->ageAllPlayers();
}

void Game::handleSeasonTransition() {
  endSeason();
  startNewSeason();
  loadData();
}

void Game::startNewSeason() {
  current_season++;
  current_week = 0;
  db->updateGameState(current_season, current_week, managed_team_id);
  db->resetAllLeaguePoints();

  for (auto &pair : GameData::instance().getLeagues()) {
    pair.second.resetPoints();
  }
  generateAllCalendars();
}

int Game::getCurrentSeason() const { return current_season; }

int Game::getCurrentWeek() const { return current_week; }

const std::vector<Team> Game::getTeams() const {
  std::vector<Team> teams;
  for (const auto &pair : GameData::instance().getTeams()) {
    teams.push_back(pair.second);
  }
  return teams;
}

Team &Game::getTeamById(int team_id) {
  return GameData::instance().getTeams().at(team_id);
}

const Team &Game::getTeamById(int team_id) const {
  return GameData::instance().getTeams().at(team_id);
}

League &Game::getLeagueById(int league_id) {
  return GameData::instance().getLeagues().at(league_id);
}

const League &Game::getLeagueById(int league_id) const {
  return GameData::instance().getLeagues().at(league_id);
}

std::vector<Player> Game::getPlayersForTeam(int team_id) {
  std::vector<Player> players;
  const auto &player_ids = GameData::instance().getTeams().at(team_id).getPlayerIDs();
  for (const auto &player_id : player_ids) {
    players.push_back(GameData::instance().getPlayers().at(player_id));
  }
  return players;
}

std::vector<Team> Game::getTeamsInLeague(int league_id) {
  std::vector<Team> teams_in_league;
  for (const auto &pair : GameData::instance().getTeams()) {
    if (pair.second.getLeagueId() == league_id)
      teams_in_league.push_back(pair.second);
  }
  return teams_in_league;
}

void Game::saveGame() {
  db->updateGameState(current_season, current_week, managed_team_id);
  for (const auto &pair : GameData::instance().getLeagues()) {
    db->saveLeaguePoints(pair.second);
  }
}

void Game::generateAllCalendars() {
  for (const auto &pair : GameData::instance().getLeagues()) {
    Calendar league_calendar;
    std::vector<Team> teams_in_league;
    for (const auto &team_pair : GameData::instance().getTeams()) {
      if (team_pair.second.getLeagueId() == pair.first) {
        teams_in_league.push_back(team_pair.second);
      }
    }
    league_calendar.generate(teams_in_league);
    db->saveCalendar(league_calendar, current_season, pair.first);

    league_calendars[pair.first] = league_calendar;
  }
}

void Game::trainPlayers(const std::vector<uint32_t> &player_ids) {
  for (auto &player_id : player_ids) {
    auto &player = GameData::instance().getPlayers().at(player_id);
    const auto &focus_stats = stats_config.role_focus.at(player.getRole()).stats;
    player.train(focus_stats);
    db->updatePlayer(player);
  }
}

void Game::selectManagedTeam(int team_id) {
  if (GameData::instance().getTeams().count(team_id) == 0) {
    throw std::runtime_error("Invalid team ID");
  }

  managed_team_id = team_id;
  db->saveManagedTeamId(team_id);
  std::cout << "You are now managing: " << getTeamById(team_id).getName()
            << '\n';
}

bool Game::hasSelectedTeam() const {
  return managed_team_id != FREE_AGENTS_TEAM_ID;
}

std::vector<Team> Game::getAvailableTeams() const {
  std::vector<Team> available_teams;
  for (const auto &pair : GameData::instance().getTeams()) {
    if (pair.first != FREE_AGENTS_TEAM_ID) {
      available_teams.push_back(pair.second);
    }
  }
  return available_teams;
}
