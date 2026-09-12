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
#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "global/types.h"
#include "model/board.h"
#include "model/gamedate.h"

class DatabaseConnection;
class GameData;
class Inbox;

/** @brief A facility project approved by the board. */
struct FacilityProject
{
  std::uint32_t id = 0;
  TeamID team_id = 0;
  FacilityProjectType type = FacilityProjectType::TrainingGround;
  GameDateValue start;
  GameDateValue end; /*!< Completion day. */
  std::int64_t cost = 0;
  std::int64_t paid = 0;
  std::uint32_t amount = 0;
  std::uint32_t disruption = 0;
  bool completed = false;

  /** Share of the works done on @p today, in [0, 1]. */
  float progress(const GameDateValue& today) const;
};

/**
 * @class FacilityProjects
 * @brief Training ground, medical centre and stadium projects.
 *
 * The board approves a request from its confidence and the club's cash
 * (BoardModel::reviewProject). A quarter of the cost is paid on approval
 * and the rest in monthly instalments; a completed training ground raises
 * the training facilities (training quality), a medical centre shortens
 * injury layoffs and a stadium expansion adds seats (attendance and gate
 * receipts once demand exceeds the old capacity). During a stadium
 * expansion part of the ground is closed. Refused requests of a type wait
 * BoardModel::PROJECT_COOLDOWN_DAYS.
 */
class FacilityProjects
{
 public:
  /** Quote for a club today (seats only matter for stadiums). */
  ProjectQuote quote(const GameData& gamedata, TeamID team_id,
                     FacilityProjectType type, std::uint32_t seats = 0) const;

  /**
   * Asks the board; on approval the project starts today and the deposit is
   * booked to the ledger. A refusal starts the cooldown (except for
   * AlreadyRunning / TooManyProjects / Cooldown).
   */
  ProjectVerdict request(GameData& gamedata, const BoardState& board,
                         TeamID team_id, FacilityProjectType type,
                         std::uint32_t seats, const GameDateValue& today);

  /** Instalments on the 1st, completion (with news for the managed club). */
  void onDay(GameData& gamedata, const GameDateValue& date,
             TeamID managed_team_id, Inbox& inbox);

  /** Medical centre level of a club (1-100, 50 = standard). */
  std::uint8_t medicalLevel(TeamID team_id) const;
  /** Injury layoff multiplier of a club's medical centre. */
  float layoffMultiplier(TeamID team_id) const;

  /** Projects of a club, running first, then completed (newest first). */
  std::vector<FacilityProject> projectsFor(TeamID team_id) const;
  /** Day a refused type may be requested again (nullopt when free). */
  std::optional<GameDateValue> cooldownUntil(TeamID team_id,
                                             FacilityProjectType type,
                                             const GameDateValue& today) const;

  void clear();
  void load(const std::shared_ptr<DatabaseConnection>& db_conn);
  void save(const std::shared_ptr<DatabaseConnection>& db_conn) const;

 private:
  void complete(GameData& gamedata, FacilityProject& project,
                const GameDateValue& date, TeamID managed_team_id,
                Inbox& inbox);

  std::vector<FacilityProject> projects;
  std::unordered_map<TeamID, std::uint8_t> medical;
  std::map<std::pair<TeamID, std::uint8_t>, GameDateValue> cooldowns;
  std::uint32_t next_id = 1;
};
