#include "game_controller.h"
#include <vector>

GameController::GameController(Game& game) : game(game) {}

Team GameController::getManagedTeam() const {
  return game.getManagedTeam();
}

std::vector<Team> GameController::getTeams() const {
  return game.getTeams();
}

void GameController::advanceWeek() {
  game.advanceWeek();
}

std::vector<Player> GameController::getPlayersForTeam(int team_id) {
  return game.getPlayersForTeam(team_id);
}

std::vector<Team> GameController::getTeamsInLeague(int league_id) {
  return game.getTeamsInLeague(league_id);
}

League& GameController::getLeagueById(int league_id) {
  return game.getLeagueById(league_id);
}

void GameController::saveGame() {
  game.saveGame();
}
