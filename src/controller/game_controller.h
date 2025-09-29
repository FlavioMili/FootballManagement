// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include "global/stats_config.h"
#include "model/game.h"
#include "model/league.h"
#include "model/player.h"
#include "model/team.h"
#include <functional>
#include <memory>
#include <optional>
#include <vector>

class GameController {
 public:
  explicit GameController(std::unique_ptr<Game> game_ptr);

  int getCurrentSeason() const;
  GameDateValue getCurrentDate() const;
  bool hasSelectedTeam() const;

  std::optional<std::reference_wrapper<Team>> getManagedTeam();
  std::optional<std::reference_wrapper<const Team>> getManagedTeam() const;
  void selectManagedTeam(uint16_t team_id);

  const std::vector<std::reference_wrapper<const League>> &getLeagues() const;
  const std::vector<std::reference_wrapper<const Team>> &getTeams() const;
  std::vector<std::reference_wrapper<const Player>>
  getPlayersForTeam(uint16_t team_id) const;
  std::vector<std::reference_wrapper<const Team>>
  getTeamsInLeague(uint8_t league_id) const;
  std::optional<std::reference_wrapper<const League>>
  getLeagueById(uint8_t league_id) const;
  std::optional<std::reference_wrapper<const Team>>
  getTeamById(uint16_t team_id) const;
  const StatsConfig &getStatsConfig() const;

  void advanceDay();

  void saveGame();

 private:
  std::unique_ptr<Game> game;
};
