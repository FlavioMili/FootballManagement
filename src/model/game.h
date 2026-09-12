// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include "database/database_connection.h"
#include "model/calendar.h"
#include "model/competition_manager.h"
#include "model/gamedate.h"
#include "model/guidance.h"
#include "model/manager_career.h"
#include "model/match.h"
#include "model/match_report.h"
#include "model/match_scheduler.h"
#include "model/national_teams.h"
#include "model/transfer_market.h"
#include "model/world_simulation.h"

/**
 * @class Game
 * @brief Represents the core game state and logic.
 *
 * The Game class orchestrates the simulation mechanics, handling the
 * progression of time, saving and loading the game state, and triggering match
 * simulations.
 */
class Game
{
 public:
  /**
   * @brief Constructs a new Game object.
   * @param gd Shared pointer to the GameData instance.
   * @param conn Shared pointer to the DatabaseConnection.
   */
  explicit Game(std::shared_ptr<class GameData> gd,
                std::shared_ptr<DatabaseConnection> conn);

  /**
   * @brief Advances the game time by one day, simulating any matches or events
   * scheduled for the current date.
   */
  void advanceDay();

  /** Records a managed match exactly once and applies its consequences. */
  bool setMatchResult(const GameDateValue& date, TeamID home_id, TeamID away_id,
                      uint8_t home_score, uint8_t away_score);

  /**
   * Records a managed match with its structured report (e.g. built from the
   * live MatchEngine). A drawn cup tie is settled by extra time and
   * penalties unless the report already carries them. @p consequences
   * (MatchdaySquad::consequences of the engine) apply the measured condition
   * and injuries exactly as for simulated matches.
   */
  bool setMatchResult(
      const GameDateValue& date, TeamID home_id, TeamID away_id,
      MatchReport report,
      std::span<const PlayerMatchConsequence> consequences = {});

  /** Fit (not injured), outside friendlies not suspended for @p type, and
   * not away with his national team on @p date (default: today). */
  bool isEligible(const Player& player, MatchType type,
                  std::optional<GameDateValue> date = std::nullopt) const;
  /** Selected players of a club who may not play a match of @p type. */
  std::vector<PlayerID> ineligibleSelections(TeamID team_id,
                                             MatchType type) const;
  /**
   * The assistant's matchday selection: ineligible players are replaced by
   * eligible squad and academy players, then by trialists. When nobody fit
   * is left for a starting place, injured players play through it (the
   * least injured first), so a side always takes the field; players who may
   * not play at all (suspended, on national duty) and cannot be replaced
   * leave the XI and the side plays with the players it has. Availability
   * is judged on @p date (default: today). Returns how many selections
   * changed.
   */
  std::size_t fixMatchdaySquad(TeamID team_id, MatchType type,
                               std::optional<GameDateValue> date = std::nullopt);
  /** What fixMatchdaySquad() would change: (replaced, replacement or 0). */
  std::vector<std::pair<PlayerID, PlayerID>> previewMatchdaySquadFix(
      TeamID team_id, MatchType type) const;
  /**
   * Whether the club's selection may kick off as it is: it is exactly what
   * fixMatchdaySquad() would pick, so players still listed by
   * ineligibleSelections() are injured players nobody fit can replace.
   */
  bool canKickOff(TeamID team_id, MatchType type) const;
  /**
   * Whether the assistant keeps the managed selection eligible: after each
   * day, and at kick-off, injured or suspended players are replaced.
   */
  void setAssistantFixesLineup(bool enabled)
  {
    guidance.delegation.set(Duty::LineupFixes,
                            enabled ? DutyOwner::Assistant : DutyOwner::Manager);
  }
  bool getAssistantFixesLineup() const
  {
    return guidance.delegation.delegated(Duty::LineupFixes);
  }

  /** Checklist, delegation, opposition instructions, match analytics. */
  CareerGuidance& getGuidance() { return guidance; }
  const CareerGuidance& getGuidance() const { return guidance; }

  /** Standings, cups, reports, player season stats and season history. */
  const CompetitionManager& getCompetitions() const { return competitions; }

  /** National teams: calendar, call-ups, finals, caps. */
  const NationalTeams& getNationalTeams() const { return international; }

  /** Inbox, board, injuries, finances and development between matches. */
  WorldSimulation& getWorld() { return world; }
  const WorldSimulation& getWorld() const { return world; }

  /** Deals, loans, pre-contracts, scheduled payments and transfer history. */
  TransferMarket& getTransfers() { return transfers; }
  const TransferMarket& getTransfers() const { return transfers; }

  /** The human manager's career and the market for managers. */
  ManagerCareer& getCareer() { return career; }
  const ManagerCareer& getCareer() const { return career; }
  /** What the manager market did on the latest simulated day. */
  const CareerDayEvents& getLastCareerEvents() const
  {
    return last_career_events;
  }

  /**
   * The manager leaves the managed club (sacked, resigned, contract
   * expired, or moving on): the club gets a vacancy and an AI manager in
   * due course, its board, shortlist, transfer talks and offers are dropped
   * and no command reaches it any more. The inbox stays.
   */
  void leaveManagedTeam(DepartureReason reason);

  /**
   * The manager takes charge of @p team_id under @p contract (leaving the
   * current club first): the board sets objectives and budgets for the new
   * club, scouting starts afresh and the club's squad, line-up, tactics and
   * training plan become the manager's.
   */
  void takeJob(TeamID team_id, const ManagerContract& contract);

  /**
   * Matches simulated / scheduled so far in the current advanceDay(). Safe to
   * poll from another thread while advanceDay() runs.
   */
  SimulationProgress getSimulationProgress() const
  {
    return scheduler.getProgress();
  }
  void resetSimulationProgress() { scheduler.resetProgress(); }
  /** Threads used to simulate a matchday (1 = one match after another). */
  void setSimulationThreads(unsigned threads)
  {
    scheduler.setThreadCount(threads);
  }
  unsigned getSimulationThreads() const { return scheduler.getThreadCount(); }

  /**
   * @brief Retrieves the current in-game date.
   * @return A constant reference to the current GameDateValue.
   */
  const GameDateValue& getCurrentDate() const;

  /**
   * @brief Retrieves the calendar containing all fixtures.
   * @return A constant reference to the game Calendar.
   */
  const Calendar& getCalendar() const;
  Calendar& getCalendar();

  /**
   * @brief Retrieves the current season number.
   * @return The current season as an integer.
   */
  int getCurrentSeason() const;

  /**
   * @brief Retrieves the ID of the team currently managed by the user.
   * @return The managed team's ID.
   */
  uint16_t getManagedTeamId() const;

  /**
   * @brief Sets the team managed by the user.
   *
   * Taking charge of a new club sets the board's objective and budgets
   * immediately and posts the day-one inbox (welcome, squad report,
   * pre-season schedule, scouting suggestion).
   * @param id The ID of the team to manage.
   */
  void setManagedTeamId(uint16_t id);

  /**
   * @brief Saves the current game state, including calendar and game variables,
   * to the database.
   */
  void saveGame();

 private:
  void loadGame();
  void endSeason();
  void handleSeasonTransition();
  void startNewSeason();
  /** The club's fixtures from today up to its first competitive match. */
  std::vector<UpcomingFixture> upcomingFixtures(TeamID team_id) const;

  /** Replaces unavailable managed players for the next managed fixture. */
  void keepManagedSelectionEligible();
  /** fixMatchdaySquad() applied to @p lineup, a selection of @p team_id. */
  std::size_t fillMatchdaySquad(Lineup& lineup, TeamID team_id, MatchType type,
                                const GameDateValue& date) const;

  /** Manager market day and the board's verdict on the manager. */
  void runCareerDay();
  /** Job security of AI managers and the human's record. */
  void recordCareerMatch(const Match& match);

  // Matchday simulation helper
  void simulateMatches(std::vector<Match>& matches,
                       bool include_managed = false);

  std::shared_ptr<DatabaseConnection> db_conn;
  std::shared_ptr<class GameData> gamedata;
  Calendar calendar;
  CompetitionManager competitions;
  WorldSimulation world;
  TransferMarket transfers;
  ManagerCareer career;
  CareerDayEvents last_career_events;
  NationalTeams international;
  MatchScheduler scheduler;
  GameDateValue currentDate;
  uint8_t current_season = 1;
  uint16_t managed_team_id;
  CareerGuidance guidance;
};
