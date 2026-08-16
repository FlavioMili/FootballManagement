// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "database/database_connection.h"
#include "model/team.h"

struct sqlite3_stmt;

struct StoredPositionedPlayer
{
  PlayerID playerId = 0;
  Vector2F position{};
};

struct StoredLineup
{
  bool persisted = false;
  std::optional<PlayerID> goalkeeperId;
  std::vector<StoredPositionedPlayer> outfield;
  std::vector<PlayerID> reserves;
};

/**
 * @class TeamRepository
 * @brief Repository class for managing Team entities in the database.
 */
class TeamRepository
{
 public:
  /**
   * @brief Construct a new Team Repository object.
   * @param db_conn Shared pointer to the database connection.
   */
  explicit TeamRepository(std::shared_ptr<DatabaseConnection> db_conn);

  /**
   * @brief Load all teams from the database.
   * @return A vector of Team objects.
   */
  std::vector<Team> loadAllTeams() const;

  /** Load pointer-free lineup data so GameData can resolve player IDs. */
  std::unordered_map<TeamID, StoredLineup> loadAllLineups() const;

  /**
   * @brief Insert a new team into the database.
   * @param team The Team object to insert.
   */
  void insertTeam(const Team& team) const;

  /**
   * @brief Insert a team with a specific ID into the database.
   * @param team The Team object to insert.
   */
  void insertTeamWithId(const Team& team) const;

  /**
   * @brief Insert multiple teams with specific IDs into the database.
   * @param teams A vector of reference wrappers to Team objects.
   */
  void insertTeamsWithId(
      const std::vector<std::reference_wrapper<const Team>>& teams) const;

  /** Persist mutable team state without overwriting lineup data. */
  void updateTeamState(const Team& team) const;

  /** Persist multiple team states with one prepared statement. */
  void updateTeamsState(
      const std::vector<std::reference_wrapper<const Team>>& teams) const;

 private:
  std::shared_ptr<DatabaseConnection> db_conn;

  void bindTeamParams(sqlite3_stmt* stmt, const Team& team,
                      int startIndex) const;
  void bindTeamStateParams(sqlite3_stmt* stmt, const Team& team) const;
};
