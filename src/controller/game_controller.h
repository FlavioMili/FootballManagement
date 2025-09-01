// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <memory>
#include <vector> 
#include "game.h"
#include "manager.h"
#include "stats_config.h"

/**
  * This class is used to separate the view and
  * the model in a traditional MVC pattern.
  * This might change in the future
*/
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
