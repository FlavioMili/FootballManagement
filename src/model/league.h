// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <string_view>
#include <vector>
#include <map>

/**
  * League class, seems like it has pretty much 
  * all we need to make it work, maybe it could 
  * be improved with stats or something like that.
  *
  * TODO add stats after the match engine improves and
  * makes players actually perform stuff
*/
class League {
 public:
  League(uint8_t league_id, std::string_view league_name,
         const std::vector<size_t>& initial_team_ids = {});

  // Accessors
  uint8_t getId() const;
  std::string_view getName() const;

  const std::vector<size_t>& getTeamIDs() const;
  void addTeamID(uint16_t team_id);
  void removeTeamID(uint16_t team_id);

  uint8_t getPoints(uint16_t team_id) const;
  void addPoints(uint16_t team_id, uint8_t points);
  void setPoints(uint16_t team_id, uint8_t points);
  void resetPoints();

  const std::map<uint8_t, uint8_t>& getLeaderboard() const;

 private:
  uint8_t id;
  std::string_view name;
  std::vector<uint16_t> team_ids;
  std::map<uint16_t, uint8_t> leaderboard; // team_id -> points
};
