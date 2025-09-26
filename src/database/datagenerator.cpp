// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "database/datagenerator.h"

#include "gamedata.h"
#include "global/languages.h"
#include "global/paths.h"
#include "global/stats_config.h"
#include "json.hpp"
#include "model/league.h"
#include "model/player.h"
#include "model/team.h"

#include <filesystem>
#include <fstream>
#include <random>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;

std::vector<std::string> DataGenerator::first_names;
std::vector<std::string> DataGenerator::last_names;

void DataGenerator::loadNames() {
  if (first_names.empty()) {
    std::ifstream f(FIRST_NAMES_PATH);
    if (!f.is_open()) {
      throw std::runtime_error("Could not open first names file");
    }
    json data = json::parse(f);
    first_names = data.at("names").get<std::vector<std::string>>();
  }
  if (last_names.empty()) {
    std::ifstream f(LAST_NAMES_PATH);
    if (!f.is_open()) {
      throw std::runtime_error("Could not open last names file");
    }
    json data = json::parse(f);
    last_names = data.at("names").get<std::vector<std::string>>();
  }
}

Player DataGenerator::generateRandomPlayer(uint16_t team_id) {
  auto stats_config = GameData::instance().getStatsConfig();
  static std::mt19937 gen(std::random_device{}());
  std::uniform_int_distribution<size_t> name_dist(0, first_names.size() - 1);
  std::uniform_int_distribution<size_t> last_name_dist(0,
                                                       last_names.size() - 1);
  std::uniform_int_distribution<int> age_dist(18, 35);
  std::uniform_int_distribution<int> contract_dist(1, 5);
  std::uniform_int_distribution<int> height_dist(165, 200);
  std::uniform_int_distribution<int> wage_dist(500, 10000);
  std::uniform_int_distribution<size_t> role_dist(
      0, stats_config.role_focus.size() - 1);
  std::uniform_int_distribution<int> foot_dist(0, 1);

  std::string first_name = first_names[name_dist(gen)];
  std::string last_name = last_names[last_name_dist(gen)];
  int age = age_dist(gen);
  int contract_years = contract_dist(gen);
  int height = height_dist(gen);
  int wage = wage_dist(gen);

  auto it = stats_config.role_focus.begin();
  std::advance(it, role_dist(gen));
  std::string role = it->first;

  Foot foot = (foot_dist(gen) == 0) ? Foot::Left : Foot::Right;

  std::map<std::string, float> stats;
  const auto &possible_stats = stats_config.possible_stats;
  std::uniform_real_distribution<float> stat_dist(20.0, 80.0);
  for (const auto &stat_name : possible_stats) {
    stats[stat_name] = stat_dist(gen);
  }

  return Player(0, team_id, first_name, last_name, role, Language::EN,
                static_cast<uint32_t>(wage), 0, static_cast<uint8_t>(age),
                static_cast<uint8_t>(contract_years),
                static_cast<uint8_t>(height), foot, stats);
}

std::vector<League> DataGenerator::generateLeagues() {
  std::ifstream f(LEAGUES_PATH);
  json data = json::parse(f);

  std::vector<League> leagues;

  for (const auto &item : data) {
    uint8_t id = item.at("id").get<uint8_t>();
    std::string name = item.at("name").get<std::string>();

    std::optional<uint8_t> parent = std::nullopt;
    if (item.contains("parent_league")) {
      parent = item.at("parent_league").get<uint8_t>();
    }

    leagues.emplace_back(id, name, std::vector<uint16_t>{}, parent);
  }
  return leagues;
}

std::vector<Team> DataGenerator::generateTeams() {
  std::vector<Team> teams;
  for (const auto &entry : fs::directory_iterator(TEAMS_DIR)) {
    if (entry.is_regular_file() && entry.path().extension() == ".json") {
      std::ifstream f(entry.path());
      json data = json::parse(f);
      for (const auto &item : data) {
        teams.emplace_back(item.at("id").get<uint16_t>(),
                           item.at("league_id").get<uint8_t>(),
                           item.at("name").get<std::string>(), 0);
      }
    }
  }

  return teams;
}

std::vector<Player> DataGenerator::generatePlayers() {
  loadNames();
  std::vector<Player> players;
  std::map<uint16_t, bool> teams_with_players;

  for (const auto &entry : fs::directory_iterator(PLAYERS_DIR)) {
    if (entry.is_regular_file() && entry.path().extension() == ".json") {
      std::ifstream f(entry.path());
      json data = json::parse(f);
      for (const auto &item : data) {
        uint16_t team_id = item.at("team_id").get<uint16_t>();
        teams_with_players[team_id] = true;

        auto it =
            stringToLanguage.find(item.at("nationality").get<std::string>());
        Language nationality =
            (it != stringToLanguage.end()) ? it->second : Language::EN;
        Foot foot = (item.at("preferred_foot").get<std::string>() == "Left")
                        ? Foot::Left
                        : Foot::Right;

        players.emplace_back(
            item.at("id").get<uint32_t>(), team_id,
            item.at("first_name").get<std::string>(),
            item.at("last_name").get<std::string>(),
            item.at("role").get<std::string>(), nationality,
            item.at("wage").get<uint32_t>(), item.value("status", 0),
            item.at("age").get<uint8_t>(),
            item.at("contract_years").get<uint8_t>(),
            item.at("height").get<uint8_t>(), foot,
            item.at("stats").get<std::map<std::string, float>>());
      }
    }
  }

  auto teams = GameData::instance().getTeamsVector();
  for (const auto &teamRef : teams) {
    const Team &team = teamRef.get();
    if (teams_with_players.contains(team.getId())) {
      for (int i = 0; i < 22; ++i) {
        players.push_back(generateRandomPlayer(team.getId()));
      }
    }
  }

  return players;
}
