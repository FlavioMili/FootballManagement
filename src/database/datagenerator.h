#pragma once

#include "league.h"
#include "player.h"
#include "team.h"
#include <string>
#include <vector>

class DataGenerator {
public:
  static std::vector<League> generateLeagues(const std::string &file_path);
  static std::vector<Team> generateTeams(const std::string &file_path);
  static std::vector<Player> generatePlayers(const std::string &file_path);
};
