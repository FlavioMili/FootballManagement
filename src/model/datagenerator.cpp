// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "datagenerator.h"
#include <algorithm>
#include <random>
#include <chrono>
#include <map>
#include <string>
#include <vector>
#include "player.h"
#include "team.h"
#include "league.h"
#include "global.h"

DataGenerator::DataGenerator(Database& db,
     const nlohmann::json& stats_config,
     const std::vector<std::string>& league_names,
     const std::vector<std::string>& team_names)
: db(db),
  stats_config(stats_config),
  league_names(league_names),
  team_names(team_names) {
  first_names = db.getFirstNames();
  last_names = db.getLastNames();
}

void DataGenerator::generateLeagues(const std::vector<std::string>& names) {
  for (const auto& name : names) {
    db.addLeague(name);
  }
}

void DataGenerator::generateTeams(int league_id, const std::vector<std::string>& names) {
  for (const auto& name : names) {
    db.addTeam(league_id, name, 1000000);
  }
}

void DataGenerator::generatePlayers(int team_id, const std::string& role, int count) {
  int age;
  for (int i = 0; i < count; ++i) {
    std::string name = first_names[rand() % first_names.size()] + " " + last_names[rand() % last_names.size()];
    age = 16 + rand() % 15;
    Player player(0, name, age, role);

    std::map<std::string, float> stats;
    for (const auto& stat_name : stats_config["possible_stats"]) {
      stats[stat_name] = static_cast<float>(MIN_STAT_VAL + (rand() % (MAX_STAT_VAL - MIN_STAT_VAL))) + 
                         static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
      if (stats[stat_name] > MAX_STAT_VAL) stats[stat_name] = MAX_STAT_VAL;
    }
    player.setStats(stats);

    db.addPlayer(team_id, player);
  }
}

void DataGenerator::generateAll() {
  auto existing_teams = db.getTeams(0);
  if (existing_teams.empty()) {
    db.addTeam(/*league_id=*/0, FREE_AGENTS_TEAM_NAME, -1);
  }

  std::vector<std::string> selected_league_names;
  for (size_t i = 0; i < 2 && i < league_names.size(); ++i) {
    selected_league_names.emplace_back(league_names[i]);
  }

  generateLeagues(selected_league_names);
  std::vector<League> leagues = db.getLeagues();

  for (const auto& league : leagues) {
    std::vector<std::string> current_league_team_names = team_names;
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::shuffle(current_league_team_names.begin(),
                 current_league_team_names.end(), 
                 std::default_random_engine(seed));

    for (size_t i = 0; i < TEAMS_PER_LEAGUE && i < current_league_team_names.size(); ++i) {
      db.addTeam(league.getId(), current_league_team_names[i], 1000000);
    }

    std::vector<Team> teams = db.getTeams(league.getId());
    for (const auto& team : teams) {
      generatePlayers(team.getId(), "Goalkeeper", 3);
      generatePlayers(team.getId(), "Defender", 6);
      generatePlayers(team.getId(), "Midfielder", 8);
      generatePlayers(team.getId(), "Striker", 4);
    }
  }
}
