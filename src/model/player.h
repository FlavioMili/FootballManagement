// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include "global/languages.h"
#include "global/stats_config.h"
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

// TODO mode this class in some header file
enum class Foot : bool { Left  = false, Right = true };

/**
 * TODO add status (enum PlayerStatus) a vector and/or a bitset
 * just so that we can check if a player is on transfer list,
 * or injured, or had a red card, and so on.
 * This way we can manage more things easily later on I guess.
 * Also these for modding reasons might come from a modifiable json?
 */

class Player {
 public:
  Player(uint32_t id,
         std::string_view first_name,
         std::string_view last_name,
         std::string_view role,
         Language nationality,
         uint32_t wage,
         uint8_t contract_years,
         uint8_t age,
         uint8_t height,
         Foot foot,
         const std::map<std::string, float> &stats);

  uint32_t getId() const;
  std::string getName() const;
  int getAge() const;
  void setAge(int age);
  std::string getRole() const;
  double getOverall(const StatsConfig &stats_config) const;
  const std::map<std::string, float> &getStats() const;
  void setStats(const std::map<std::string, float> &new_stats);
  void agePlayer();
  bool checkRetirement() const;
  void train(const std::vector<std::string> &focus_stats);

 private:
  uint32_t id;
  std::string_view first_name;
  std::string_view last_name;
  std::string_view role; // TODO URGENT: change to enum
  Language nationality;
  uint32_t wage;
  uint8_t contract_years;
  uint8_t age;
  uint8_t height;
  Foot foot;
  std::map<std::string, float> stats;
};
