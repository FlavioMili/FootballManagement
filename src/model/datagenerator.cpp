#include "datagenerator.h"
#include "player.h"
#include "team.h"
#include "league.h"
#include "global.h" // Include global.h for TEAMS_PER_LEAGUE
#include <fstream>
#include <iostream>
#include <algorithm> // For std::random_shuffle
#include <random>    // For std::default_random_engine
#include <chrono>    // For std::chrono::system_clock

DataGenerator::DataGenerator(Database& db) : db(db) {
   std::ifstream f("assets/stats_config.json");
   if (!f.is_open()) {
      std::cerr << "Error: Could not open assets/stats_config.json" << std::endl;
      return;
   }
   stats_config = nlohmann::json::parse(f);
   if (!stats_config.contains("stats")) {
      std::cerr << "Error: assets/stats_config.json is missing the 'stats' section." << std::endl;
   }

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
   for (int i = 0; i < count; ++i) {
      std::string name = first_names[rand() % first_names.size()] + " " + last_names[rand() % last_names.size()];
      int age = 16 + rand() % 15;
      Player player(0, name, age, role);

      std::map<std::string, int> stats;
      for (auto const& [stat_name, stat_props] : stats_config["stats"].items()) {
         int min_val = stat_props["min"];
         int max_val = stat_props["max"];
         stats[stat_name] = min_val + rand() % (max_val - min_val + 1);
      }
      std::cout << "DEBUG: DataGenerator::generatePlayers() - Generated stats for " << name << ": ";
      for (const auto& pair : stats) {
         std::cout << pair.first << ": " << pair.second << ", ";
      }
      std::cout << std::endl;
      player.setStats(stats);

      std::map<std::string, double> weights;
      const auto& role_stats = stats_config["role_focus"][role]["stats"];
      const auto& role_weights = stats_config["role_focus"][role]["weights"];
      for (size_t j = 0; j < role_stats.size(); ++j) {
         weights[role_stats[j]] = role_weights[j];
      }
      std::cout << "DEBUG: DataGenerator::generatePlayers() - Role: " << role << ", Weights: ";
      for (const auto& pair : weights) {
         std::cout << pair.first << ": " << pair.second << ", ";
      }
      std::cout << std::endl;
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
      std::shuffle(current_league_team_names.begin(), current_league_team_names.end(), std::default_random_engine(seed));

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
