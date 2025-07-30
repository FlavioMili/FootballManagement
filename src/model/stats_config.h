#pragma once

#include <string>
#include <vector>
#include <map>

struct RoleFocus {
    std::vector<std::string> stats;
    std::vector<double> weights;
};

struct StatsConfig {
    std::map<std::string, RoleFocus> role_focus;
    std::vector<std::string> possible_stats;
};
