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

enum class Foot : bool { Left = false, Right = true };

class Player {
 public:
  Player(uint32_t new_id, uint32_t new_team_id, std::string_view new_first_name,
         std::string_view new_last_name, std::string_view new_role,
         Language new_nationality, uint32_t new_wage, uint32_t new_status, uint8_t new_age,
         uint8_t new_contract_years, uint8_t new_height, Foot new_foot,
         const std::map<std::string, float> &new_stats);

  uint32_t getId() const;
  uint32_t getTeamId() const;
  std::string getName() const;
  std::string getFirstName() const;
  std::string getLastName() const;
  int getAge() const;
  void setAge(uint8_t new_age);
  std::string getRole() const;
  Language getNationality() const;
  uint32_t getWage() const;
  uint8_t getContractYears() const;
  uint8_t getHeight() const;
  Foot getFoot() const;
  uint32_t getStatus() const;
  double getOverall(const StatsConfig &stats_config) const;
  const std::map<std::string, float> &getStats() const;
  void setStats(const std::map<std::string, float> &new_stats);
  void agePlayer();
  bool checkRetirement() const;
  void train(const std::vector<std::string> &focus_stats);

 private:
  // 32-bit fields first
  uint32_t _id;
  uint32_t _team_id;
  uint32_t _wage;
  uint32_t _status;

  // strings (non-POD, heap allocated, alignment not a problem)
  std::string _first_name;
  std::string _last_name;
  std::string _role;

  // Enums and small ints grouped together
  Language _nationality;
  uint8_t _age;
  uint8_t _contract_years;
  uint8_t _height;
  Foot _foot;

  // stats container
  std::map<std::string, float> _stats;
};
