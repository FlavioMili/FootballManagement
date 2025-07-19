#include "league.h"

League::League() {
   teams.emplace_back("team 1 ");
   teams.emplace_back("team 2 ");
}

League::~League() = default;

std::vector<Team> League::getTeams() const {
   return teams;
}

void League::overrideTeams(const std::vector<Team>& newTeams) {
   teams = newTeams;
}

void to_json(nlohmann::json& j, const League& l) {
   j = nlohmann::json{{"teams", l.getTeams()}};
}

void from_json(const nlohmann::json& j, League& l) {
   l.overrideTeams(j.at("teams").get<std::vector<Team>>());
}
