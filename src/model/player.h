// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
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

/**
 * @enum Foot
 * @brief Represents a player's preferred foot.
 */
enum class Foot : bool
{
  Left = false,
  Right = true
};

/**
 * @enum TransferStatus
 * @brief Represents a player's transfer listing status.
 */
enum class TransferStatus
{
  Listed,
  NotListed
};

/**
 * @class Player
 * @brief Represents a football player.
 */
class Player
{
 public:
  /**
   * @brief Constructs a Player object.
   * @param new_id The unique ID for the player.
   * @param new_team_id The ID of the team the player belongs to.
   * @param new_first_name The player's first name.
   * @param new_last_name The player's last name.
   * @param new_role The player's role (e.g., ST, CB).
   * @param new_nationality The player's nationality.
   * @param new_wage The player's wage.
   * @param new_status The player's status.
   * @param new_age The player's age.
   * @param new_contract_years Years remaining on the contract.
   * @param new_height The player's height in cm.
   * @param new_foot The player's preferred foot.
   * @param new_stats A map of the player's attributes/stats.
   */
  Player(PlayerID new_id, TeamID new_team_id, std::string_view new_first_name,
         std::string_view new_last_name, PlayerRole new_role,
         Language new_nationality, uint32_t new_wage, uint32_t new_status,
         uint8_t new_age, uint8_t new_contract_years, uint8_t new_height,
         Foot new_foot, const std::map<std::string, float>& new_stats);

  /** @brief Gets the player's ID. */
  PlayerID getId() const;

  /** @brief Gets the ID of the player's team. */
  TeamID getTeamId() const;

  /** @brief Sets the ID of the player's team. */
  void setTeamId(TeamID id);

  /** @brief Gets the player's full name. */
  std::string getName() const;

  /** @brief Gets the player's first name. */
  std::string getFirstName() const;

  /** @brief Gets the player's last name. */
  std::string getLastName() const;

  /** @brief Gets the player's age. */
  int getAge() const;

  /** @brief Sets the player's age. */
  void setAge(uint8_t new_age);

  /** @brief Gets the player's role. */
  PlayerRole getRole() const;

  /** @brief Gets the player's nationality. */
  Language getNationality() const;

  /** @brief Gets the player's wage. */
  uint32_t getWage() const;

  /** @brief Gets the player's remaining contract years. */
  uint8_t getContractYears() const;

  /** @brief Gets the player's height. */
  uint8_t getHeight() const;

  /** @brief Gets the player's preferred foot. */
  Foot getFoot() const;

  /** @brief Gets the player's status. */
  uint32_t getStatus() const;

  /**
   * @brief Calculates the player's overall rating based on stats and
   * configuration.
   * @param stats_config Configuration weights for the stats.
   * @return The overall rating.
   */
  double getOverall(const StatsConfig& stats_config) const;

  /** @brief Gets the player's stats map. */
  const std::map<std::string, float>& getStats() const;

  /** @brief Sets the player's stats map. */
  void setStats(const std::map<std::string, float>& new_stats);

  /** @brief Increases the player's age by 1. */
  void agePlayer();

  /**
   * @brief Checks if the player is ready for retirement.
   * @return True if the player retires, false otherwise.
   */
  bool checkRetirement() const;

  /**
   * @brief Trains the player, improving specific focus stats.
   * @param focus_stats The stats to focus on during training.
   */
  void train(const std::vector<std::string>& focus_stats);

  // Market Value & Transfer Logic

  /** @brief Gets the player's market value. */
  uint32_t getMarketValue() const;

  /**
   * @brief Updates the player's market value.
   * @param stats_config The stats configuration to use for the calculation.
   */
  void updateMarketValue(const StatsConfig& stats_config);

  /** @brief Sets the player's transfer status. */
  void setTransferStatus(TransferStatus status);

  /** @brief Gets the player's transfer status. */
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

  // Enums and small ints grouped together
  PlayerRole _role;
  Language _nationality;
  TransferStatus _transfer_status = TransferStatus::NotListed;
  uint8_t _age;
  uint8_t _contract_years;
  uint8_t _height;
  Foot _foot;

  // stats container
  std::map<std::string, float> _stats;
};
