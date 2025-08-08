#pragma once

#include <memory>
#include <vector> 
#include "game.h"
#include "manager.h"
#include "stats_config.h"

class GameController {
 public:
  explicit GameController(Game& game);

  int getCurrentSeason() const;
  int getCurrentWeek() const;
  bool hasSelectedTeam() const;

  Team getManagedTeam() const;
  void selectManagedTeam(int team_id);

  std::vector<Team> getTeams() const;
  std::vector<Team> getAvailableTeams() const;
  std::vector<Player> getPlayersForTeam(int team_id) const;
  std::vector<Team> getTeamsInLeague(int league_id) const;
  League& getLeagueById(int league_id);
  const StatsConfig& getStatsConfig() const;

  void advanceWeek();
  void startNewSeason();

  void saveGame();
  void loadGame();

 private:
  Game& game;
  std::unique_ptr<Manager> manager;
};
