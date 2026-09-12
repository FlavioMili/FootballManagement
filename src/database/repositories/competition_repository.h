// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <memory>
#include <vector>

#include "database/database_connection.h"
#include "model/discipline.h"
#include "model/season_history.h"

/**
 * @class CompetitionRepository
 * @brief Persists season history and per-player season statistics.
 */
class CompetitionRepository
{
 public:
  explicit CompetitionRepository(std::shared_ptr<DatabaseConnection> db_conn);

  /** @brief Creates the competition tables missing from older saves. */
  void ensureSchema() const;

  /** @brief Inserts or replaces history rows (season, type, competition). */
  void saveSeasonHistory(const std::vector<SeasonHistoryEntry>& entries) const;

  /** @brief Loads all history rows ordered by season then competition. */
  std::vector<SeasonHistoryEntry> loadSeasonHistory() const;

  /** @brief Inserts or replaces the given player season rows. */
  void savePlayerSeasonStats(const PlayerSeasonTable& table) const;

  /** @brief Loads the player season rows of one season. */
  PlayerSeasonTable loadPlayerSeasonStats(uint16_t season) const;

  /** @brief Loads every season row of a player (career history). */
  std::vector<PlayerSeasonStats> loadPlayerCareer(PlayerID player_id) const;

  /** @brief Replaces all stored disciplinary records. */
  void saveDiscipline(const std::vector<DisciplinaryRecord>& records) const;

  std::vector<DisciplinaryRecord> loadDiscipline() const;

 private:
  std::shared_ptr<DatabaseConnection> db_conn;
};
