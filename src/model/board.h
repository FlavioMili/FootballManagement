// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <array>
#include <cstdint>

#include "global/types.h"

/**
 * @enum BoardObjective
 * @brief Season objective set from the club's expected finish (persisted).
 */
enum class BoardObjective : std::uint8_t
{
  WinLeague = 0,
  TopFour,
  TopHalf,
  MidTable,
  AvoidRelegation
};

/** @brief Outcome of a monthly board review. */
enum class BoardReviewOutcome : std::uint8_t
{
  Satisfied,
  Warning,
  Dismissed
};

/**
 * @struct BoardState
 * @brief The board's view of the manager of the managed club.
 */
struct BoardState
{
  static constexpr std::size_t RESULT_WINDOW = 8;

  TeamID team_id = 0;
  std::uint16_t season_year = 0; /*!< Calendar year the season started. */
  BoardObjective objective = BoardObjective::MidTable;
  std::uint8_t expected_position = 0; /*!< From the wage-bill rank. */
  std::uint8_t target_position = 0;   /*!< Worst acceptable finish. */
  float confidence = 60.0f;           /*!< 0-100. */
  std::uint8_t low_reviews = 0;       /*!< Consecutive critical reviews. */
  std::uint8_t league_matches = 0;    /*!< League matches this season. */
  bool dismissed = false;
  std::uint8_t result_count = 0;
  /** Points minus expected points of recent league matches, newest first. */
  std::array<float, RESULT_WINDOW> recent_deltas{};
};

namespace BoardModel
{
/**
 * Objective for a club expected to finish @p expected_position. The
 * expectation comes from the wage-bill rank because wages explain most of
 * the variation in league position (Szymanski).
 */
BoardObjective objectiveFor(int expected_position, int league_size);

/** Worst acceptable league position for @p objective. */
std::uint8_t targetPosition(BoardObjective objective, int league_size);

/** Language key naming @p objective. */
const char* objectiveKey(BoardObjective objective);

/**
 * Expected league points of a match from both line-up strengths (mean
 * overall), including home advantage. [P] Elo-style logistic with a draw
 * band.
 */
float expectedPoints(float own_strength, float opponent_strength, bool home);

/**
 * Records a league result: confidence moves with results relative to
 * expectation over the last eight matches, B = 0.9 B + 0.1 (50 + 25 z).
 */
void recordMatch(BoardState& state, float points, float expected_points);

/**
 * Monthly review of league position (vs target) and finances. Dismissal
 * needs several consecutive critical reviews after enough matches, so only
 * sustained, extreme failure ends the job.
 */
BoardReviewOutcome monthlyReview(BoardState& state, int position,
                                 int league_size, bool negative_balance,
                                 bool over_wage_budget);

/** Season-end adjustment from the final position. */
void seasonReview(BoardState& state, int final_position);
}  // namespace BoardModel

// ---------------------------------------------------------------------------
// Facility projects the manager can ask the board to fund
// ---------------------------------------------------------------------------

/** @brief Facility project (values are persisted). */
enum class FacilityProjectType : std::uint8_t
{
  TrainingGround = 0, /*!< +training facilities. */
  MedicalCentre,      /*!< Shorter injury layoffs. */
  StadiumExpansion,   /*!< More seats; part of the ground closed meanwhile. */
  COUNT
};

/** @brief The board's answer to a project request. */
enum class ProjectVerdict : std::uint8_t
{
  Approved = 0,
  AlreadyRunning,  /*!< A project of this type is under way. */
  TooManyProjects, /*!< Two projects at once at most. */
  Cooldown,        /*!< Refused recently; ask again later. */
  AtMaximum,       /*!< Nothing left to build. */
  NotNeeded,       /*!< Far beyond what the club's stature needs. */
  LowConfidence,   /*!< The board does not trust the manager enough. */
  CannotAfford     /*!< The cost would eat into the cash reserve. */
};

/** @brief Cost, duration and size of a project for one club. */
struct ProjectQuote
{
  FacilityProjectType type = FacilityProjectType::TrainingGround;
  std::int64_t cost = 0;
  std::uint16_t days = 0;
  std::uint32_t amount = 0;     /*!< Facility levels or seats added. */
  std::uint32_t disruption = 0; /*!< Seats closed during the works. */
};

/** @brief What the board weighs when a project is requested. */
struct ProjectRequestContext
{
  float confidence = 60.0f;
  std::int64_t balance = 0;
  std::int64_t weekly_payroll = 0;
  std::uint8_t reputation = 50;
  std::uint8_t current_level = 50; /*!< Facility level (0 for stadiums). */
  int running_projects = 0;
  bool same_type_running = false;
  bool cooling_down = false;
};

namespace BoardModel
{
/** Days a refused request of the same type waits before it can be asked
 * again. */
inline constexpr int PROJECT_COOLDOWN_DAYS = 90;
inline constexpr int MAX_RUNNING_PROJECTS = 2;
/** Weeks of payroll the cash must still cover after paying the project. */
inline constexpr int PROJECT_CASH_RESERVE_WEEKS = 16;
inline constexpr float PROJECT_MIN_CONFIDENCE = 45.0f;
inline constexpr std::uint32_t MIN_EXPANSION_SEATS = 1'000;
inline constexpr std::uint32_t MAX_STADIUM_CAPACITY = 100'000;

/**
 * Quote for @p type: facilities rise 10 levels (6% / 3.5% of a season's
 * income, more at higher levels) over six / four months; a stadium grows
 * by @p seats (rounded to 500, at most half the current capacity) at a
 * per-seat price rising with the club's stature, over 8-14 months, with a
 * tenth of the ground closed meanwhile. [P]
 */
ProjectQuote quoteProject(FacilityProjectType type, std::uint8_t current_level,
                          std::uint32_t capacity, std::uint8_t reputation,
                          double season_income, std::uint32_t seats);

/** Board decision on a quoted project. */
ProjectVerdict reviewProject(const ProjectQuote& quote,
                             const ProjectRequestContext& context);

/** Layoff multiplier of a medical centre level: 1.0 up to 50, 0.85 at 100. */
float medicalLayoffMultiplier(std::uint8_t medical_level);

/** Language keys (e.g. "PROJECT_TRAINING_GROUND", "PROJECT_VERDICT_..."). */
const char* projectTypeKey(FacilityProjectType type);
const char* projectVerdictKey(ProjectVerdict verdict);
}  // namespace BoardModel
