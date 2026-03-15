// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "global/types.h"

/**
 * League class, seems like it has pretty much
 * all we need to make it work, maybe it could
 * be improved with stats or something like that.
 *
 * TODO add stats after the match engine improves and
 * makes players actually perform stuff
 */
class League
{
 public:
  League(LeagueID league_id, const std::string& league_name,
         const std::vector<TeamID>& initial_team_ids = {},
         std::optional<LeagueID> parent = std::nullopt);

  // Accessors
  uint8_t getId() const;
  std::string getName() const;

  const std::vector<TeamID>& getTeamIDs() const;
  void addTeamID(TeamID team_id);
  void removeTeamID(TeamID team_id);

  uint8_t getPoints(TeamID team_id) const;
  void addPoints(TeamID team_id, uint8_t points);
  void setPoints(TeamID team_id, uint8_t points);
  void resetPoints();

  const std::map<TeamID, uint8_t>& getLeaderboard() const;
  const std::optional<LeagueID> getParentLeagueID() const;

 private:
  const LeagueID id;
  std::string name;
  const std::optional<LeagueID> parent_league_id;
  std::vector<TeamID> team_ids;
  std::map<TeamID, uint8_t> leaderboard;  // team_id -> points
};
