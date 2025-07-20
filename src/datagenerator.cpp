#include "datagenerator.h"
#include "json.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <random>
#include <algorithm>
#include <set>

using json = nlohmann::json;

// Helper function to load names from a JSON file
std::vector<std::string> load_names(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << path << std::endl;
        return {};
    }
    json data;
    file >> data;
    return data["names"].get<std::vector<std::string>>();
}

// Helper for random number generation
int get_random(int min, int max) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(min, max);
    return distrib(gen);
}

void DataGenerator::generate(Database& db) {
    std::cout << "Generating initial game data..." << std::endl;

    auto first_names = load_names("assets/first_names.json");
    auto last_names = load_names("assets/last_names.json");
    auto team_names = load_names("assets/team_names.json");
    auto league_names = load_names("assets/league_names.json");

    if (first_names.empty() || last_names.empty() || team_names.empty() || league_names.empty()) {
        std::cerr << "Error: Failed to load one or more name files. Aborting generation." << std::endl;
        return;
    }
    
    std::cout << "Name files loaded successfully." << std::endl;

    // Load stats configuration
    std::ifstream stats_file("assets/stats_config.json");
    json stats_config;
    if (stats_file.is_open()) {
        stats_file >> stats_config;
    } else {
        std::cerr << "Error: Could not open stats_config.json" << std::endl;
        return;
    }
    auto possible_stats = stats_config["possible_stats"].get<std::vector<std::string>>();
    auto roles_config = stats_config["role_focus"];
    std::vector<std::string> roles;
    for (auto it = roles_config.begin(); it != roles_config.end(); ++it) {
        roles.push_back(it.key());
    }
    int min_age = stats_config["age_distribution"]["min"];
    int max_age = stats_config["age_distribution"]["max"];

    if (first_names.empty() || last_names.empty() || team_names.empty() || league_names.empty()) {
        std::cerr << "Error: Failed to load one or more name files. Aborting generation." << std::endl;
        return;
    }
    
    std::cout << "Name files loaded successfully." << std::endl;

    db.beginTransaction();
    db.setMetadata("season", "1");

    auto create_player_with_role = [&](long long team_id) {
        std::string player_name = first_names[get_random(0, first_names.size() - 1)] + " " + last_names[get_random(0, last_names.size() - 1)];
        int age = get_random(min_age, max_age);
        
        // Assign a role and generate stats based on it
        std::string role = roles[get_random(0, roles.size() - 1)];
        auto focus_stats_vec = roles_config[role]["stats"].get<std::vector<std::string>>();
        std::set<std::string> focus_stats(focus_stats_vec.begin(), focus_stats_vec.end());

        std::map<std::string, int> stats;
        for (const auto& stat_name : possible_stats) {
            if (focus_stats.count(stat_name)) {
                // High stats for focus areas
                stats[stat_name] = get_random(65, 95);
            } else {
                // Lower stats for other areas
                stats[stat_name] = get_random(30, 70);
            }
        }
        db.savePlayer(player_name, age, team_id, stats);
    };

    // 1. Create Free Agents Team
    std::cout << "Creating Free Agents team..." << std::endl;
    long long free_agents_team_id = db.saveTeam("Free Agents", 0, nullptr);
    for (int i = 0; i < 50; ++i) {
        create_player_with_role(free_agents_team_id);
    }
    std::cout << "Free Agents team created with 50 players." << std::endl;


    // 2. Create Leagues, Teams, and Players
    size_t team_name_idx = 0;
    for (size_t i = 0; i < 2; ++i) {
        if (i >= league_names.size()) {
            std::cerr << "Warning: Not enough league names to generate 2 leagues." << std::endl;
            break;
        }
        std::string current_league_name = league_names[i];
        long long league_id = db.saveLeague(current_league_name);
        std::cout << "Creating League: " << current_league_name << " (ID: " << league_id << ")" << std::endl;

        for (int j = 0; j < 18; ++j) {
            if (team_name_idx >= team_names.size()) {
                std::cerr << "Warning: Not enough team names to generate all teams." << std::endl;
                break;
            }
            std::string current_team_name = team_names[team_name_idx++];
            long long team_balance = get_random(5000000, 20000000); // 5M to 20M
            long long team_id = db.saveTeam(current_team_name, team_balance, &league_id);
            std::cout << "  - Creating Team: " << current_team_name << " (ID: " << team_id << ")" << std::endl;

            int num_players = get_random(22, 28);
            for (int k = 0; k < num_players; ++k) {
                create_player_with_role(team_id);
            }
        }
    }

    db.commitTransaction();
    std::cout << "Data generation complete." << std::endl;
}
