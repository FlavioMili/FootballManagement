// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "controller/game_controller.h"
#include "database/gamedata.h"

GameController::GameController(std::unique_ptr<Game> game_ptr)
    : game(std::move(game_ptr)) {}

int GameController::getCurrentSeason() const {
  return game->getCurrentSeason();
}

int GameController::getCurrentWeek() const { return game->getCurrentWeek(); }

bool GameController::hasSelectedTeam() const { return game->hasSelectedTeam(); }

std::optional<std::reference_wrapper<Team>> GameController::getManagedTeam() {
  return GameData::instance().getTeam(GameData::instance().getManagedTeamId());
}

std::optional<std::reference_wrapper<const Team>>
GameController::getManagedTeam() const {
  return GameData::instance().getTeam(GameData::instance().getManagedTeamId());
}

void GameController::selectManagedTeam(uint16_t team_id) {
  GameData::instance().setManagedTeamId(team_id);
}

const std::vector<std::reference_wrapper<const Team>> &
GameController::getTeams() const {
  return GameData::instance().getTeamsVector();
}

std::vector<std::reference_wrapper<const Player>>
GameController::getPlayersForTeam(uint16_t team_id) const {
  return GameData::instance().getPlayersForTeam(team_id);
}

std::vector<std::reference_wrapper<const Team>>
GameController::getTeamsInLeague(uint8_t league_id) const {
  std::vector<std::reference_wrapper<const Team>> teams;
  for (const auto &team : GameData::instance().getTeamsVector()) {
    if (team.get().getLeagueId() == league_id) {
      teams.push_back(team);
    }
  }
  return teams;
}

std::optional<std::reference_wrapper<const League>>
GameController::getLeagueById(uint8_t league_id) const {
  return GameData::instance().getLeague(league_id);
}

std::optional<std::reference_wrapper<const Team>>
GameController::getTeamById(uint16_t team_id) const {
  return GameData::instance().getTeam(team_id);
}

const StatsConfig &GameController::getStatsConfig() const {
  return GameData::instance().getStatsConfig();
}

void GameController::advanceWeek() { game->advanceWeek(); }

void GameController::startNewSeason() { game->startNewSeason(); }

void GameController::saveGame() { game->saveGame(); }

void GameController::loadGame() { game->loadData(); }
