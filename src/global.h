#pragma once

#include <array>

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

constexpr std::array<Stats, 7> all_stats = {
   Stats::RUNNING, Stats::SHOOTING, Stats::PASSING,
   Stats::DEFENDING, Stats::PHYSIQUE, Stats::STAMINA,
   Stats::DRIBBLING
};
