#pragma once

#include <string_view>
#include <array>
#include "json.hpp"

enum class Role {
   GOALKEEPER,
   DEFENDER,
   MIDFIELDER,
   STRIKER,
};

enum class Stats {
   RUNNING,
   SHOOTING,
   PASSING,
   DEFENDING,
   PHYSIQUE,
   STAMINA,
   DRIBBLING,
};

constexpr std::array<Stats, 7> all_stats = {{
   Stats::RUNNING, 
   Stats::SHOOTING,
   Stats::PASSING,
   Stats::DEFENDING,
   Stats::PHYSIQUE,
   Stats::STAMINA,
   Stats::DRIBBLING,
}};

constexpr std::array<std::pair<std::string_view, Stats>, 7> stats_map = {{
   {"RU", Stats::RUNNING},
   {"SH", Stats::SHOOTING},
   {"PA", Stats::PASSING},
   {"DE", Stats::DEFENDING},
   {"PH", Stats::PHYSIQUE},
   {"ST", Stats::STAMINA},
   {"DR", Stats::DRIBBLING},
}};

constexpr std::array<std::pair<std::string_view, Role>, 4> roles_map = {{
   {"GK", Role::GOALKEEPER},
   {"DF", Role::DEFENDER},
   {"MF", Role::MIDFIELDER},
   {"ST", Role::STRIKER},
}};

constexpr Stats stringToStat(std::string_view sv) {
   for (auto [key, val] : stats_map) {
      if (key == sv) return val;
   }
   throw "Invalid Stats code";
}

constexpr std::string_view statToString(Stats s) {
   for (auto [key, val] : stats_map) {
      if (val == s) return key;
   }
   throw "Invalid Stats enum";
}

constexpr Role stringToRole(std::string_view sv) {
   for (auto [key, val] : roles_map) {
      if (key == sv) return val;
   }
   throw "Invalid Role code";
}

constexpr std::string_view roleToString(Role r) {
   for (auto [key, val] : roles_map) {
      if (val == r) return key;
   }
   throw "Invalid Role enum";
}

// Serialization for storing in JSONs
inline void to_json(nlohmann::json& j, const Role& r) {
    j = std::string(roleToString(r));
}

inline void from_json(const nlohmann::json& j, Role& r) {
    r = stringToRole(j.get<std::string>());
}

inline void to_json(nlohmann::json& j, const Stats& s) {
    j = std::string(statToString(s));
}

inline void from_json(const nlohmann::json& j, Stats& s) {
    s = stringToStat(j.get<std::string>());
}

