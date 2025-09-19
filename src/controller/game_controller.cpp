// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "game_controller.h"

GameController::GameController(std::unique_ptr<Game> game_ptr)
: game(std::move(game_ptr)) {}

int GameController::getCurrentSeason() const {
  return game->getCurrentSeason();
}

int GameController::getCurrentWeek() const {
  return game->getCurrentWeek();
}

bool GameController::hasSelectedTeam() const {
  return game->hasSelectedTeam();
}

Team& GameController::getManagedTeam() {
  return game->getManagedTeam();
}

const Team& GameController::getManagedTeam() const {
  return game->getManagedTeam();
}

void GameController::selectManagedTeam(int team_id) {
  game->selectManagedTeam(team_id);
}

std::vector<Team> GameController::getTeams() const {
  return game->getTeams();
}

std::vector<Team> GameController::getAvailableTeams() const {
  return game->getAvailableTeams();
}

std::vector<Player>& GameController::getPlayersForTeam(int team_id) {
  return game->getPlayersForTeam(team_id);
}

const std::vector<Player>& GameController::getPlayersForTeam(int team_id) const {
  return game->getPlayersForTeam(team_id);
}

std::vector<Team> GameController::getTeamsInLeague(int league_id) const {
  return game->getTeamsInLeague(league_id);
}

League& GameController::getLeagueById(int league_id) {
  return game->getLeagueById(league_id);
}

Team& GameController::getTeamById(int team_id) {
  return game->getTeamById(team_id);
}

const Team& GameController::getTeamById(int team_id) const {
  return game->getTeamById(team_id);
}

const StatsConfig& GameController::getStatsConfig() const {
  return game->getStatsConfig();
}

void GameController::advanceWeek() {
  game->advanceWeek();
}

void GameController::startNewSeason() {
  game->startNewSeason();
}

void GameController::saveGame() {
  game->saveGame();
}

void GameController::loadGame() {
  game->loadData();
}
