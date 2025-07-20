#pragma once

#include "team.h"
#include <vector>
#include <utility>

class Calendar {
public:
    Calendar() = default;
    void generateSeasonSchedule(const std::vector<Team>& teams);
    const std::vector<std::vector<std::pair<int, int>>>& getSchedule() const;

private:
    std::vector<std::vector<std::pair<int, int>>> schedule; // week -> list of (home_team_idx, away_team_idx)
};
