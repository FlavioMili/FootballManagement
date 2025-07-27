#include "datagenerator.h"
#include <fstream>
#include <iostream>
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

DataGenerator::DataGenerator(Database& db) : db(db) {
  std::ifstream f("assets/stats_config.json");
  if (!f.is_open()) {
    std::cerr << "Error: Could not open assets/stats_config.json\n";
    return;
  }
  stats_config = nlohmann::json::parse(f);

  std::ifstream f_first("assets/first_names.json");
  first_names = nlohmann::json::parse(f_first)["names"].get<std::vector<std::string>>();

  std::ifstream f_last("assets/last_names.json");
  last_names = nlohmann::json::parse(f_last)["names"].get<std::vector<std::string>>();
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

    std::map<std::string, int> stats;
    for (const auto& stat_name : stats_config["possible_stats"]) {
      stats[stat_name] = MIN_STAT_VAL + rand() % (MAX_STAT_VAL - MIN_STAT_VAL + 1);
    }
    player.setStats(stats);

    std::map<std::string, double> weights;
    const auto& role_stats = stats_config["role_focus"][role]["stats"];
    const auto& role_weights = stats_config["role_focus"][role]["weights"];
    for (size_t j = 0; j < role_stats.size(); ++j) {
      weights[role_stats[j]] = role_weights[j];
    }

    player.calculateOverall(weights);

    db.addPlayer(team_id, player);
  }
}

void DataGenerator::generateAll() {
  std::ifstream f_leagues("assets/league_names.json");
  std::vector<std::string> all_league_names = nlohmann::json::parse(f_leagues)["names"].get<std::vector<std::string>>();
  std::vector<std::string> selected_league_names;
  for (size_t i = 0; i < 2 && i < all_league_names.size(); ++i) {
    selected_league_names.push_back(all_league_names[i]);
  }

  generateLeagues(selected_league_names);
  std::vector<League> leagues = db.getLeagues();
  std::ifstream f_teams("assets/team_names.json");
  std::vector<std::string> all_team_names = nlohmann::json::parse(f_teams)["names"].get<std::vector<std::string>>();

  for (const auto& league : leagues) {
    // Shuffle team names and take only TEAMS_PER_LEAGUE
    std::vector<std::string> current_league_team_names = all_team_names;
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::shuffle(current_league_team_names.begin(),
                 current_league_team_names.end(), 
                 std::default_random_engine(seed));

    for (size_t i = 0; i < TEAMS_PER_LEAGUE && i < current_league_team_names.size(); ++i) {
      // TODO this should become random balance
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
