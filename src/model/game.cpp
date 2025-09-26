// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "model/game.h"
#include "database/gamedata.h"
#include "global/paths.h"
#include <iostream>
#include <memory>
#include <sys/stat.h>

Game::Game() : current_season(1), current_week(0), current_date("2025-07-01") {
  GameData::instance().loadStatsConfig();

  db = std::make_shared<Database>();
  GameData::instance().loadFromDB(db);
  loadData();
}

void Game::loadData() {
  uint8_t season, week;
  std::string game_date;
  db->loadGameState(season, week, managed_team_id, game_date);
  current_season = season;
  current_week = week;
  current_date = game_date;
  GameData::instance().setManagedTeamId(managed_team_id);
  managed_league_id = GameData::instance()
                          .getTeams()
                          .at(GameData::instance().getManagedTeamId())
                          .getLeagueId();
}

void Game::advanceWeek() {
  // Simple date advancement: add 7 days.
  int day = std::stoi(current_date.substr(8, 2));
  int month = std::stoi(current_date.substr(5, 2));
  int year = std::stoi(current_date.substr(0, 4));
  day += 7;
  if (day > 30) { // Assuming 30 days per month for simplicity
    day -= 30;
    month++;
    if (month > 12) {
      month = 1;
      year++;
    }
  }
  char date_buf[11];
  snprintf(date_buf, sizeof(date_buf), "%04u-%02u-%02u",
           static_cast<unsigned int>(year), static_cast<unsigned int>(month),
           static_cast<unsigned int>(day));
  current_date = date_buf;

  const auto &calendar = league_calendars.at(managed_league_id);

  if (current_week >= static_cast<int>(calendar.getWeeks().size()) &&
      !calendar.getWeeks().empty()) {
    handleSeasonTransition();
    return;
  }
  simulateWeek();
}

void Game::simulateWeek() {
  Calendar &current_calendar = league_calendars.at(managed_league_id);

  if (current_week >= static_cast<int>(current_calendar.getWeeks().size())) {
    std::cout << "No more weeks to simulate in the current season.\n";
    return;
  }

  std::cout << "--- Simulating Week " << current_week + 1 << " ---\n";
  const auto &week_matches =
      current_calendar.getWeeks()[current_week].getMatches();
  current_calendar.getWeeks()[current_week].getMatches();

  for (const auto &match_info : week_matches) {
    Team &home_team =
        GameData::instance().getTeams().at(match_info.home_team_id);
    Team &away_team =
        GameData::instance().getTeams().at(match_info.away_team_id);

    Match match(home_team, away_team, stats_config);
    match.simulate();
    updateStandings(match);

    if (home_team.getId() == GameData::instance().getManagedTeamId() ||
        away_team.getId() == GameData::instance().getManagedTeamId()) {
      std::cout << "Your Team's Match Result: " << home_team.getName() << " "
                << match.getHomeScore() << " - " << match.getAwayScore() << " "
                << away_team.getName() << "\n";
    }
    trainPlayers(home_team.getPlayerIDs());
    trainPlayers(away_team.getPlayerIDs());
  }

  current_week++;
}

void Game::updateStandings(const Match &match) {
  Team &home_team =
      GameData::instance().getTeams().at(match.getHomeTeam().getId());
  Team &away_team =
      GameData::instance().getTeams().at(match.getAwayTeam().getId());
  League &league =
      GameData::instance().getLeagues().at(home_team.getLeagueId());

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
}

void Game::startNewSeason() {
  current_season++;
  current_week = 0;
  current_date =
      std::to_string(std::stoi(current_date.substr(0, 4)) + 1) + "-07-01";
  db->updateGameState(current_season, current_week,
                      GameData::instance().getManagedTeamId(), current_date);
  db->resetAllLeaguePoints();

  for (auto &pair : GameData::instance().getLeagues()) {
    pair.second.resetPoints();
  }
  generateAllCalendars();
}

int Game::getCurrentSeason() const { return current_season; }

int Game::getCurrentWeek() const { return current_week; }

void Game::saveGame() {
  db->updateGameState(current_season, current_week,
                      GameData::instance().getManagedTeamId(), current_date);
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
    const auto &focus_stats =
        stats_config.role_focus.at(player.getRole()).stats;
    player.train(focus_stats);
    db->updatePlayer(player);
  }
}
