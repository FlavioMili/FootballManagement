// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "global/types.h"
#include "model/board.h"
#include "model/club_economy.h"
#include "model/gamedate.h"
#include "model/inbox.h"
#include "model/training.h"
#include "model/scouting.h"

class DatabaseConnection;
class GameData;
class Match;
class Player;
class Team;
struct MatchReport;

/**
 * @struct PlayerMatchConsequence
 * @brief Physical outcome of one player's match.
 *
 * - end_condition < 0: the condition drain is estimated from minutes and
 *   stamina and the injury hazard is rolled here (ECIS match incidence).
 * - end_condition >= 0: the match engine measured the condition and owns
 *   injuries; set injured=true for players it injured and the diagnosis and
 *   layoff are drawn here.
 */
struct PlayerMatchConsequence
{
  PlayerID player_id = 0;
  std::uint8_t minutes_played = 0;
  float end_condition = -1.0f;
  bool injured = false;
};

/**
 * @enum SquadRole
 * @brief Playing-time expectation derived from ability rank in the squad.
 */
enum class SquadRole : std::uint8_t
{
  KeyPlayer, /*!< Top 5: expects to start nearly every match. */
  FirstTeam, /*!< Rank 6-11: regular starter. */
  Rotation,  /*!< Rank 12-16. */
  Backup,    /*!< Rank 17-22. */
  Fringe     /*!< Beyond the matchday squad (young players: prospect). */
};

/** Language key naming @p role (e.g. "SQUAD_ROLE_KEY_PLAYER"). */
const char* squadRoleKey(SquadRole role);

/**
 * @class WorldSimulation
 * @brief Everything that happens between matches.
 *
 * Owns the managed club's inbox and board state and processes, from the
 * Game's day loop: condition recovery, training and match injuries, weekly
 * payroll, training development, morale, monthly revenue and costs, the
 * March youth intake and season-end prize money, contract renewals and
 * retirements. All randomness is derived from the save's world seed.
 */
class WorldSimulation
{
 public:
  explicit WorldSimulation(std::shared_ptr<GameData> gamedata);

  // ---- Hooks called by Game ----

  /** Daily/weekly/monthly processing; call once per new date. */
  void onDayAdvanced(const GameDateValue& date, TeamID managed_team_id);

  /**
   * Applies the consequences of a played match: condition, sharpness,
   * injuries, ratings, morale, recent form, gate receipts (written to
   * report.attendance), board confidence and news. Participants come from
   * report.players (starters of the line-ups when empty); ratings of 0 are
   * estimated. Players whose physical consequences were already applied on
   * the match date through applyMatchConsequences() are not drained twice.
   */
  void onMatchPlayed(const Match& match, MatchReport& report,
                     TeamID managed_team_id);

  /** Prize money, reputation, board review, renewals and retirements. */
  void onSeasonEnd(const GameDateValue& date, TeamID managed_team_id);

  /** Budgets, ticket prices, season counters and the board objective. */
  void onSeasonStart(const GameDateValue& date, TeamID managed_team_id);

  /** Loads the inbox and board state (after GameData is loaded). */
  void load(const std::shared_ptr<DatabaseConnection>& db_conn);

  /** Writes inbox, board, world state and retired players (in a
   * transaction owned by the caller). */
  void save(const std::shared_ptr<DatabaseConnection>& db_conn) const;

  /** Marks ledgers and removals as persisted after a successful commit. */
  void onSaved();

  // ---- Per-player match pipeline ----

  /**
   * Applies one player's physical match consequences on @p date (condition,
   * sharpness, minutes, injuries). Idempotent per player and day: returns
   * false if already applied for that date or the player is unknown.
   */
  bool applyMatchConsequences(const GameDateValue& date,
                              const PlayerMatchConsequence& consequence,
                              TeamID managed_team_id);

  /**
   * Supplies final league tables (1st place first) for season-end prize
   * money, board reviews and reputation. Without a provider clubs are
   * ordered by points.
   */
  void setStandingsProvider(
      std::function<std::vector<TeamID>(LeagueID)> provider);

  /**
   * Supplies the days to every club's next fixture (within a week) so that
   * training follows the real match rhythm. Without a provider a weekly
   * rhythm after each club's last match is assumed.
   */
  void setFixtureOutlookProvider(
      std::function<void(const GameDateValue&, TrainingSystem::FixtureOutlook&)>
          provider);

  // ---- Transfer events (called by the controller) ----

  /** A club bid for a player; unsettles ambitious players. */
  void onTransferBid(const GameDateValue& date, PlayerID player_id,
                     TeamID bidder_id, std::uint32_t amount,
                     TeamID managed_team_id);

  /** A transfer completed. */
  void onTransferCompleted(const GameDateValue& date, PlayerID player_id,
                           TeamID from_team_id, TeamID to_team_id,
                           std::uint32_t fee, TeamID managed_team_id);

  // ---- Queries ----

  const Inbox& getInbox() const { return inbox; }
  Inbox& getInbox() { return inbox; }
  const BoardState& getBoardState() const { return board; }

  /** Scouting knowledge, assignments, reports and shortlist. */
  ScoutingSystem& getScouting() { return scouting; }
  const ScoutingSystem& getScouting() const { return scouting; }

  /** Playing-time expectation of a player in his squad. */
  SquadRole squadRole(PlayerID player_id) const;

  /** Crowd of the club's latest home match (0 if none this session). */
  std::uint32_t lastHomeAttendance(TeamID team_id) const;

  /** League position of a club by points (1-based, 0 if unknown); a cheap
   * approximation used during the season. */
  int leaguePosition(TeamID team_id) const;

  /** Mean overall of the club's selected XI. */
  float lineupStrength(TeamID team_id) const;

 private:
  struct FocusStats
  {
    std::vector<std::string> all;
    double all_weight = 1.0;
    std::vector<std::string> non_physical;
    double non_physical_weight = 1.0;
  };

  void processDaily(const GameDateValue& date, std::int32_t ordinal,
                    TeamID managed_team_id);
  void processWeekly(const GameDateValue& date);
  void processMonthly(const GameDateValue& date, TeamID managed_team_id);
  void runYouthIntake(const GameDateValue& date, TeamID managed_team_id);
  void sendContractNotices(const GameDateValue& date, TeamID managed_team_id);
  void awardPrizeMoney(const GameDateValue& date, TeamID managed_team_id);
  void renewAiContracts(TeamID managed_team_id);
  void retirePlayers(const GameDateValue& date, TeamID managed_team_id);
  void ensureBoard(const GameDateValue& date, TeamID managed_team_id,
                   bool new_season);
  void developPlayer(Player& player, float training_quality,
                     std::int32_t ordinal);
  void updateWeeklyMorale(Team& team);
  void injure(Player& player, const GameDateValue& date, std::int32_t ordinal,
              bool in_match, TeamID managed_team_id);
  void recordGateReceipts(const Match& match);
  void postTrainingWarning(const GameDateValue& date, TeamID managed_team_id);
  void refreshAiLineups(TeamID managed_team_id);
  void dropUnavailableFromAiLineups(TeamID managed_team_id);
  std::vector<TeamID> standings(LeagueID league_id) const;
  std::vector<TeamID> pointsOrder(LeagueID league_id) const;
  double sportingSuccess(const Team& team) const;
  void post(const GameDateValue& date, InboxCategory category,
            std::string title_key, std::string body_key,
            std::vector<std::string> args,
            std::optional<PlayerID> player_id = std::nullopt,
            std::optional<TeamID> team_id = std::nullopt);
  const FocusStats& focusFor(const Player& player) const;
  static std::uint16_t seasonYear(const GameDateValue& date);

  std::shared_ptr<GameData> gamedata;
  ScoutingSystem scouting;
  Inbox inbox;
  BoardState board;
  mutable std::unordered_map<std::string, FocusStats> focus_by_category;
  std::unordered_map<TeamID, std::uint32_t> last_attendance;
  std::function<std::vector<TeamID>(LeagueID)> standings_provider;
  std::function<void(const GameDateValue&, TrainingSystem::FixtureOutlook&)>
      fixture_outlook_provider;
  TrainingSystem::FixtureOutlook fixture_outlook;
  std::unordered_set<TeamID> lineup_dirty;
};
