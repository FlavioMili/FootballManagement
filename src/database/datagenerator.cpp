#include "datagenerator.h"
#include "json.hpp"
#include "global/languages.h"
#include <fstream>

using json = nlohmann::json;

std::vector<League> DataGenerator::generateLeagues(const std::string &file_path) {
  std::ifstream f(file_path);
  json data = json::parse(f);
  std::vector<League> leagues;
  for (const auto &item : data) {
    leagues.emplace_back(item.at("id").get<uint8_t>(),
                         item.at("name").get<std::string>());
  }
  return leagues;
}

std::vector<Team> DataGenerator::generateTeams(const std::string &file_path) {
  std::ifstream f(file_path);
  json data = json::parse(f);
  std::vector<Team> teams;
  for (const auto &item : data) {
    teams.emplace_back(item.at("id").get<uint16_t>(),
                       item.at("league_id").get<uint8_t>(),
                       item.at("name").get<std::string>(), 0);
  }
  return teams;
}

std::vector<Player>
DataGenerator::generatePlayers(const std::string &file_path) {
  std::ifstream f(file_path);
  json data = json::parse(f);
  std::vector<Player> players;
  for (const auto &item : data) {
    auto it = stringToLanguage.find(item.at("nationality").get<std::string>());
    Language nationality =
      (it != stringToLanguage.end()) ? it->second : Language::EN;
    Foot foot = (item.at("preferred_foot").get<std::string>() == "Left")
      ? Foot::Left
      : Foot::Right;
    players.emplace_back(
      item.at("id").get<uint32_t>(),
      item.at("team_id").get<uint32_t>(),
      item.at("first_name").get<std::string>(),
      item.at("last_name").get<std::string>(),
      item.at("role").get<std::string>(),
      nationality,
      item.at("wage").get<uint32_t>(),
      item.value("status", 0),
      item.at("age").get<uint8_t>(),
      item.at("contract_years").get<uint8_t>(),
      item.at("height").get<uint8_t>(),
      foot,
      item.at("stats").get<std::map<std::string,float>>()
    );
  }
  return players;
}
