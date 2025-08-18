#pragma once

#include <string>
#include <vector>
#include <map>

/* 
 * In this file there are structs to simplify the 
 * json parsing and loading of stats.
 *
 * In the future this file might include other structs
 * useful for the project. In that case I would move 
 * this file to src/ 
*/

struct RoleFocus {
  std::vector<std::string> stats;
  std::vector<double> weights;
};

struct StatsConfig {
  std::map<std::string, RoleFocus> role_focus;
  std::vector<std::string> possible_stats;
};
