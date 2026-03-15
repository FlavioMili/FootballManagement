// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "global/languages.h"
#include "global/stats_config.h"
#include "global/types.h"

enum class Foot : bool
{
  Left = false,
  Right = true
};
enum class TransferStatus
{
  Listed,
  NotListed
};

class Player
{
 public:
  Player(PlayerID new_id, TeamID new_team_id, std::string_view new_first_name,
         std::string_view new_last_name, std::string_view new_role, Language new_nationality,
         uint32_t new_wage, uint32_t new_status, uint8_t new_age, uint8_t new_contract_years,
         uint8_t new_height, Foot new_foot, const std::map<std::string, float>& new_stats);

  PlayerID getId() const;
  TeamID getTeamId() const;
  void setTeamId(TeamID id);
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
  double getOverall(const StatsConfig& stats_config) const;
  const std::map<std::string, float>& getStats() const;
  void setStats(const std::map<std::string, float>& new_stats);
  void agePlayer();
  bool checkRetirement() const;
  void train(const std::vector<std::string>& focus_stats);

  // Market Value & Transfer Logic
  uint32_t getMarketValue() const;
  void updateMarketValue(const StatsConfig& stats_config);
  void setTransferStatus(TransferStatus status);
  TransferStatus getTransferStatus() const;

 private:
  // 32-bit fields first
  PlayerID _id;
  TeamID _team_id;
  uint32_t _wage;
  uint32_t _status;
  uint32_t _cached_market_value = 0;

  // strings (non-POD, heap allocated, alignment not a problem)
  std::string _first_name;
  std::string _last_name;
  std::string _role;

  // Enums and small ints grouped together
  Language _nationality;
  TransferStatus _transfer_status = TransferStatus::NotListed;
  uint8_t _age;
  uint8_t _contract_years;
  uint8_t _height;
  Foot _foot;

  // stats container
  std::map<std::string, float> _stats;
};
