#pragma once

#include <memory>
#include <vector> 
#include "game.h"
#include "manager.h"

class GameController {
 public:
  GameController(Game& game);

  Team getManagedTeam() const;
  std::vector<Team> getTeams() const;
  void advanceWeek();

  std::vector<Player> getPlayersForTeam(int team_id);
  std::vector<Team> getTeamsInLeague(int league_id);
  League& getLeagueById(int league_id);
  void saveGame();

 private:
  Game& game;
  std::unique_ptr<Manager> manager;
};
