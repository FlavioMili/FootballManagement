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
#include "model/league.h"

/**
 * @class LeagueRepository
 * @brief Repository class for managing League entities in the database.
 */
class LeagueRepository
{
 public:
  /**
   * @brief Construct a new League Repository object.
   * @param db_conn Shared pointer to the database connection.
   */
  explicit LeagueRepository(std::shared_ptr<DatabaseConnection> db_conn);

  /**
   * @brief Load all leagues from the database.
   * @return A vector of League objects.
   */
  std::vector<League> loadAllLeagues() const;

  /**
   * @brief Insert a new league into the database.
   * @param league The League object to insert.
   */
  void insertLeague(const League& league) const;

  /**
   * @brief Insert a league with a specific ID into the database.
   * @param league The League object to insert.
   */
  void insertLeagueWithId(const League& league) const;

  /**
   * @brief Save the points of teams in a league to the database.
   * @param league The League object whose points are to be saved.
   */
  void saveLeaguePoints(const League& league) const;

  /**
   * @brief Load the points of teams for a specific league from the database.
   * @param league The League object to populate with points.
   */
  void loadLeaguePoints(League& league) const;

  /**
   * @brief Reset points for all leagues in the database.
   */
  void resetAllLeaguePoints() const;

 private:
  std::shared_ptr<DatabaseConnection> db_conn;
  void loadTeamsForLeague(League& league) const;
};
