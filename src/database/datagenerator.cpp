// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "database/datagenerator.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <random>
#include <string>
#include <vector>

#include "gamedata.h"
#include "global/languages.h"
#include "global/logger.h"
#include "global/paths.h"
#include "global/stats_config.h"
#include "model/league.h"
#include "model/player.h"
#include "model/role_utils.h"
#include "model/team.h"

namespace fs = std::filesystem;
using json = nlohmann::json;

std::vector<std::string> DataGenerator::first_names;
std::vector<std::string> DataGenerator::last_names;

void DataGenerator::loadNames()
{
  if (first_names.empty())
  {
    std::ifstream f(FIRST_NAMES_PATH);
    if (!f.is_open())
    {
      throw std::runtime_error("Could not open first names file");
    }
    json data = json::parse(f);
    first_names = data.at("names").get<std::vector<std::string>>();
  }
  if (last_names.empty())
  {
    std::ifstream f(LAST_NAMES_PATH);
    if (!f.is_open())
    {
      throw std::runtime_error("Could not open last names file");
    }
    json data = json::parse(f);
    last_names = data.at("names").get<std::vector<std::string>>();
  }
}

Player DataGenerator::generateRandomPlayer(const GameData& gamedata,
                                           PlayerID player_id, TeamID team_id,
                                           PlayerRole role)
{
  const auto& stats_config = gamedata.getStatsConfig();
  static std::mt19937 gen(std::random_device{}());
  std::uniform_int_distribution<size_t> name_dist(0, first_names.size() - 1);
  std::uniform_int_distribution<size_t> last_name_dist(0,
                                                       last_names.size() - 1);
  std::uniform_int_distribution<int> age_dist(18, 35);
  std::uniform_int_distribution<int> contract_dist(1, 5);
  std::uniform_int_distribution<int> height_dist(165, 200);
  std::uniform_int_distribution<int> wage_dist(500, 10000);
  std::uniform_int_distribution<int> foot_dist(0, 1);

  std::string first_name = first_names[name_dist(gen)];
  std::string last_name = last_names[last_name_dist(gen)];
  int age = age_dist(gen);
  int contract_years = contract_dist(gen);
  int height = height_dist(gen);
  int wage = wage_dist(gen);

  Foot foot = (foot_dist(gen) == 0) ? Foot::Left : Foot::Right;

  std::map<std::string, float> stats;
  const auto& possible_stats = stats_config.possible_stats;
  std::uniform_real_distribution<float> stat_dist(20.0, 80.0);
  for (const auto& stat_name : possible_stats)
  {
    stats[stat_name] = stat_dist(gen);
  }

  return Player(player_id, team_id, first_name, last_name, role, Language::EN,
                static_cast<uint32_t>(wage), 0, static_cast<uint8_t>(age),
                static_cast<uint8_t>(contract_years),
                static_cast<uint8_t>(height), foot, stats);
}

std::vector<League> DataGenerator::generateLeagues()
{
  std::ifstream f(LEAGUES_PATH);
  json data = json::parse(f);

  std::vector<League> leagues;

  for (const auto& item : data)
  {
    uint8_t id = item.at("id").get<uint8_t>();
    std::string name = item.at("name").get<std::string>();

    std::optional<uint8_t> parent = std::nullopt;
    if (item.contains("parent_league"))
    {
      parent = item.at("parent_league").get<uint8_t>();
    }

    leagues.emplace_back(id, name, std::vector<uint16_t>{}, parent);
  }
  return leagues;
}

std::vector<Team> DataGenerator::generateTeams()
{
  std::vector<Team> teams;
  for (const auto& entry : fs::directory_iterator(TEAMS_DIR))
  {
    if (entry.is_regular_file() && entry.path().extension() == ".json")
    {
      std::ifstream f(entry.path());
      json data = json::parse(f);
      for (const auto& item : data)
      {
        teams.emplace_back(item.at("id").get<uint16_t>(),
                           item.at("league_id").get<uint8_t>(),
                           item.at("name").get<std::string>(),
                           item.value<int64_t>("balance", 50'000'000));
      }
    }
  }

  return teams;
}

std::vector<Player> DataGenerator::generatePlayers(const GameData& gamedata)
{
  loadNames();
  std::vector<Player> players;
  std::map<uint16_t, int> player_counts;
  PlayerID next_player_id = 50'000;

  // Load pre-defined players from JSON and count them
  for (const auto& entry : fs::directory_iterator(PLAYERS_DIR))
  {
    if (entry.is_regular_file() && entry.path().extension() == ".json")
    {
      std::ifstream f(entry.path());
      json data = json::parse(f);
      for (const auto& item : data)
      {
        TeamID team_id = item.at("team_id").get<uint16_t>();
        player_counts[team_id]++;

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
            RoleUtils::fromString(item.at("role").get<std::string>()),
            nationality, item.at("wage").get<uint32_t>(),
            item.value("status", 0), item.at("age").get<uint8_t>(),
            item.at("contract_years").get<uint8_t>(),
            item.at("height").get<uint8_t>(), foot,
            item.at("stats").get<std::map<std::string, float>>());
        next_player_id =
            std::max(next_player_id, item.at("id").get<PlayerID>() + 1U);
      }
    }
  }

  // Ensure every team has at least 30 players
  static constexpr std::array<PlayerRole, 30> SQUAD_ROLES = {
      PlayerRole::GK,  PlayerRole::GK,  PlayerRole::GK,  PlayerRole::CB,
      PlayerRole::CB,  PlayerRole::CB,  PlayerRole::CB,  PlayerRole::CB,
      PlayerRole::LB,  PlayerRole::LB,  PlayerRole::RB,  PlayerRole::RB,
      PlayerRole::CDM, PlayerRole::CDM, PlayerRole::CM,  PlayerRole::CM,
      PlayerRole::CM,  PlayerRole::CM,  PlayerRole::CAM, PlayerRole::CAM,
      PlayerRole::LM,  PlayerRole::RM,  PlayerRole::LW,  PlayerRole::LW,
      PlayerRole::RW,  PlayerRole::RW,  PlayerRole::ST,  PlayerRole::ST,
      PlayerRole::ST,  PlayerRole::ST};

  auto teams = gamedata.getTeamsVector();
  Logger::debug("Ensuring player rosters for " + std::to_string(teams.size()) +
                " teams.");
  size_t generated_players = 0;
  for (const auto& teamRef : teams)
  {
    const Team& team = teamRef.get();
    int current_player_count = player_counts[team.getId()];

    int players_to_generate = 30 - current_player_count;
    if (players_to_generate > 0)
    {
      for (int i = 0; i < players_to_generate; ++i)
      {
        const auto role_index =
            static_cast<size_t>(current_player_count + i) % SQUAD_ROLES.size();
        players.push_back(generateRandomPlayer(
            gamedata, next_player_id++, team.getId(), SQUAD_ROLES[role_index]));
        ++generated_players;
      }
    }
  }
  Logger::debug("Generated " + std::to_string(generated_players) +
                " players to complete all rosters.");
  return players;
}
