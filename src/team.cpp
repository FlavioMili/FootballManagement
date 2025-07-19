#include "team.h"
#include "player.h"

Team::Team(std::string name) {
   this->name = name;
   generateTeam();
}

Team::~Team() = default;

void Team::generateTeam() {
   for (int i = 0; i < 11; ++i)
      players.emplace_back();
}

std::string Team::getName() const {
   return name;
}

int Team::getStats() const {
   int res {};
   for (auto& p : players) 
      res += p.getStats(Stats::SHOOTING); // just for testing yet
   return res;
}

std::vector<Player> Team::getPlayers() const {
   return players;
}

void to_json(nlohmann::json& j, const Team& t) {
   j = {
      {"name", t.name},
      {"balance", t.balance},
      {"players", t.players}
   };
}

void from_json(const nlohmann::json& j, Team& t) {
   j.at("name").get_to(t.name);
   j.at("balance").get_to(t.balance);
   j.at("players").get_to(t.players);
}
