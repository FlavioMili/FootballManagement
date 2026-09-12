// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "global/types.h"
#include "model/gamedate.h"

class Calendar;
class DatabaseConnection;
class GameData;
class TransferMarket;
class WorldSimulation;
struct BoardState;

/** @brief What moves the supporters (values are persisted). */
enum class SupporterFactor : std::uint8_t
{
  Results = 0, /*!< League results against expectation. */
  Derby,       /*!< This season's matches against the rival. */
  TicketPrice, /*!< Ticket price against the fair price. */
  StarSale,    /*!< One of the best players was sold. */
  StarSigning, /*!< A signing who walks into the best players. */
  COUNT
};

/** @brief Observable facts the supporters judge the club by. */
struct SupporterFacts
{
  /** League points minus expected points, newest first (up to eight). */
  std::vector<float> result_deltas;
  /** Points taken from this season's derbies, newest first. */
  std::vector<int> derby_points;
  /** Ticket price divided by the fair price (1 when unknown). */
  float ticket_ratio = 1.0f;
  /** Stars sold and signed in the last SupporterModel::STAR_WINDOW_DAYS. */
  int star_sales = 0;
  int star_signings = 0;
};

/** @brief One factor and how many index points it is worth. */
struct SupporterReason
{
  SupporterFactor factor = SupporterFactor::Results;
  float points = 0.0f;
};

/** @brief The supporters' mood: a 0-100 index and why it is there. */
struct SupporterMood
{
  float index = 55.0f;
  /** Factors worth at least a point, strongest first. */
  std::vector<SupporterReason> reasons;
};

/**
 * @brief Pure supporter rules (no game state), unit tested in isolation.
 */
namespace SupporterModel
{
/** Index of supporters with nothing to cheer or complain about. */
inline constexpr float NEUTRAL = 55.0f;
/** Largest weekly move of the board's confidence from the supporters. */
inline constexpr float MAX_BOARD_NUDGE = 0.25f;
/** Distance from 50 within which the board does not listen to the fans. */
inline constexpr float BOARD_DEAD_ZONE = 15.0f;
/** How long a star sale or signing stays on the supporters' minds. */
inline constexpr int STAR_WINDOW_DAYS = 120;
/** Best players of the squad who count as stars. */
inline constexpr int STAR_RANK = 3;

/**
 * The mood the facts point to: results against expectation weigh most
 * (up to +-30), then derbies (+-12), ticket prices (-12 ... +6), star sales
 * (-9 each, at most -18) and star signings (+5 each, at most +10).
 */
SupporterMood evaluate(const SupporterFacts& facts);

/** Weekly step of the index towards @p target (moods change slowly). */
float settle(float previous, float target);

/**
 * Weekly nudge to the board's confidence: nothing within BOARD_DEAD_ZONE
 * of 50, then linear up to +-MAX_BOARD_NUDGE at 0 and 100.
 */
float boardNudge(float index);

/** Language key describing the index ("SUPPORTERS_MOOD_CONTENT", ...). */
const char* moodKey(float index);

/** Language key explaining @p reason ("SUPPORTERS_REASON_DERBY_WON", ...). */
const char* reasonKey(const SupporterReason& reason);
}  // namespace SupporterModel

/**
 * @class Supporters
 * @brief The managed club's supporters: their mood is recomputed once a
 * week from the facts and nudges the board's confidence a little.
 *
 * Persisted in SupporterMood (assets/db/schema.sql) inside the game's save
 * transaction.
 */
class Supporters
{
 public:
  /** The latest weekly mood (meaningful once team() is the managed club). */
  const SupporterMood& mood() const { return current; }
  /** Club the mood belongs to (0: none yet). */
  TeamID team() const { return team_id; }
  /** Day ordinal of the latest recomputation. */
  std::int32_t updatedOn() const { return updated_day; }

  /**
   * Closes @p date: once a week (and at once for a new club) the facts are
   * gathered, the mood moves towards them and the board's confidence is
   * nudged by SupporterModel::boardNudge().
   */
  void onDayEnd(const GameData& gamedata, const GameDateValue& date,
                TeamID managed_team_id, const Calendar& calendar,
                const TransferMarket& transfers, WorldSimulation& world);

  /**
   * Moves the mood of @p team_id towards the mood @p facts point to on
   * @p day (a new club starts there) and returns the board nudge, 0 for a
   * new club.
   */
  float update(TeamID new_team_id, std::int32_t day,
               const SupporterFacts& facts);

  /** The facts about @p team_id on @p date. */
  static SupporterFacts gather(const GameData& gamedata,
                               const GameDateValue& date, TeamID team_id,
                               const BoardState& board,
                               const Calendar& calendar,
                               const TransferMarket& transfers,
                               std::optional<TeamID> rival);

  void load(const std::shared_ptr<DatabaseConnection>& db_conn);
  /** Writes the state; must run inside the caller's transaction. */
  void save(const std::shared_ptr<DatabaseConnection>& db_conn) const;

 private:
  TeamID team_id = 0;
  std::int32_t updated_day = 0;
  SupporterMood current;
};
