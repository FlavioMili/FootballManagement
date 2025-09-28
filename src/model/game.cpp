// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "model/game.h"
#include "database/gamedata.h"
#include "global/logger.h"
#include <iostream>
#include <memory>
#include <sys/stat.h>

Game::Game() : current_season(1) {
  db = std::make_shared<Database>();
  GameData::instance().loadFromDB(db);
  loadData();
}

void Game::loadData() {
  uint16_t managed_team_id = 0;
  std::string game_date_str;
  if (db->loadGameState(current_season, managed_team_id, game_date_str)) {
    GameDate::instance().setCurrent(GameDateValue::fromString(game_date_str));
    for (auto &pair : GameData::instance().getLeagues()) {
      Calendar cal;
      db->loadFixtures(cal, pair.first);
      league_calendars[pair.first] = std::move(cal);
    }
  } else {
    generateAllCalendars();
  }

  if (GameData::instance().getTeams().find(managed_team_id) ==
      GameData::instance().getTeams().end()) {
    managed_team_id = FREE_AGENTS_TEAM_ID;
  }
  GameData::instance().setManagedTeamId(managed_team_id);
}

void Game::advanceDay() {
  uint16_t managed_team_id = GameData::instance().getManagedTeamId();
  if (managed_team_id == FREE_AGENTS_TEAM_ID)
    return;

  uint8_t managed_league_id =
      GameData::instance().getTeam(managed_team_id)->get().getLeagueId();

  GameDate::instance().nextDay();
  auto curr = GameDate::instance().getCurrent();
  Logger::debug("Date changed to: " + curr.toString());

  if (curr.month == 7 && curr.day == 1) { // Season start
    handleSeasonTransition();
    return;
  }

  if (league_calendars.count(managed_league_id)) {
    const auto &matches = league_calendars.at(managed_league_id)
                              .getMatches(GameDate::instance().getCurrent());
    if (matches.size() > 0)
      simulateMatches(matches);
  }
}

void Game::simulateMatches(const std::vector<Matchup> &matches) {
  uint16_t managed_team_id = GameData::instance().getManagedTeamId();
  for (const auto &matchup : matches) {
    Team &home = GameData::instance().getTeams().at(matchup.home_team_id);
    Team &away = GameData::instance().getTeams().at(matchup.away_team_id);

    Match match(home, away, stats_config);
    match.simulate();
    updateStandings(match);

    trainPlayers(home.getPlayerIDs());
    trainPlayers(away.getPlayerIDs());

    if (home.getId() == managed_team_id || away.getId() == managed_team_id) {
      std::cout << home.getName() << " " << match.getHomeScore() << " - "
                << match.getAwayScore() << " " << away.getName() << "\n";
    }
  }
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
  std::cout << "--- Season " << current_season << " has concluded. ---" << "\n";
  db->ageAllPlayers();
}

void Game::handleSeasonTransition() {
  endSeason();
  startNewSeason();
  current_season++;
}

void Game::startNewSeason() {
  GameDate::instance().setCurrent(GameDateValue(current_season + 2025, 7, 1));

  db->resetAllLeaguePoints();
  for (auto &pair : GameData::instance().getLeagues()) {
    pair.second.resetPoints();
  }

  generateAllCalendars();

  db->updateGameState(current_season, GameData::instance().getManagedTeamId(),
                      GameDate::instance().getCurrent().toString());
}

int Game::getCurrentSeason() const { return current_season; }

GameDateValue Game::getCurrentDate() const {
  return GameDate::instance().getCurrent();
}

void Game::saveGame() {
  db->updateGameState(current_season, GameData::instance().getManagedTeamId(),
                      GameDate::instance().getCurrent().toString());
  for (const auto &pair : GameData::instance().getLeagues()) {
    db->saveLeaguePoints(pair.second);
  }
  Logger::debug("Game saved");
}

void Game::generateAllCalendars() {
  GameDateValue seasonStart = GameDate::instance().getCurrent();
  for (const auto &pair : GameData::instance().getLeagues()) {
    const League &league = pair.second;
    Calendar league_calendar;
    league_calendar.generate(league, seasonStart);
    db->saveFixtures(league_calendar, pair.first);
    league_calendars[pair.first] = std::move(league_calendar);
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
