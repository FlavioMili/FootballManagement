// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <tuple>
#include <vector>

#include "model/competition.h"
#include "model/discipline.h"
#include "model/match_report.h"
#include "model/season_history.h"
#include "model/standings.h"

class Calendar;
class DatabaseConnection;
class GameData;

/**
 * @class CompetitionManager
 * @brief Owns the result pipeline of all competitions.
 *
 * Stores the structured report of every played fixture, aggregates player
 * season statistics, keeps league points in sync with the fixture-derived
 * tables, draws cup rounds and closes seasons (history, promotion and
 * relegation). Game forwards its lifecycle events here.
 */
class CompetitionManager
{
 public:
  CompetitionManager(std::shared_ptr<GameData> gamedata,
                     std::shared_ptr<DatabaseConnection> db_conn);

  /** Migrates older saves and restores the state of the loaded season. */
  void load(Calendar& calendar, uint16_t season);

  /** Persists pending reports, stats, history and memberships. Must run
   * inside the caller's transaction. */
  void save() const;

  /** Drops buffers that are now persisted (call after a commit). */
  void onSaved();

  /** Stores the report of a played match and aggregates its statistics. */
  void recordResult(const Match& match, MatchReport report);

  /** Refreshes league points and draws cup rounds that became due. */
  void afterMatchday(Calendar& calendar, const GameDateValue& today);

  /**
   * @brief Records the season history and applies promotion/relegation.
   * @param season Number of the season that ends.
   * @param start_year Calendar year that season started in.
   */
  void closeSeason(const Calendar& calendar, uint16_t season,
                   uint16_t start_year);

  /** A starter of a team whose suspension was covered for one match. */
  struct LineupSwap
  {
    TeamID team_id = 0;
    PlayerID suspended_id = 0;
    PlayerID replacement_id = 0;
  };

  /**
   * Temporarily replaces suspended starters of both teams with eligible
   * reserves for a simulated match; undo with restoreLineups().
   */
  std::vector<LineupSwap> benchSuspendedPlayers(const Match& match);
  void restoreLineups(const std::vector<LineupSwap>& swaps);

  /** Sets the season number used for new reports and statistics. */
  void setCurrentSeason(uint16_t season) { current_season = season; }

  // ---------------- Queries ----------------
  std::vector<StandingRow> getStandings(const Calendar& calendar,
                                        LeagueID league_id) const;
  /** One domestic cup per country, identified by its root league ID. */
  std::vector<LeagueID> getCupIds() const;
  std::optional<Competitions::CupStatus> getCupStatus(const Calendar& calendar,
                                                      LeagueID cup_id) const;
  std::optional<MatchReport> getMatchReport(const GameDateValue& date,
                                            TeamID home_id,
                                            TeamID away_id) const;
  const std::vector<SeasonHistoryEntry>& getSeasonHistory() const
  {
    return season_history;
  }
  /** League (or cup, with MatchType::CUP and a root league ID) scorers. */
  std::vector<PlayerSeasonStats> getTopScorers(MatchType competition_type,
                                               LeagueID competition_id,
                                               size_t limit) const;
  /** Current-season rows of a player (one per team and competition). */
  std::vector<PlayerSeasonStats> getPlayerSeasonStats(PlayerID player_id) const;
  /** Every stored season row of a player, oldest first. */
  std::vector<PlayerSeasonStats> getPlayerCareer(PlayerID player_id) const;
  const Discipline& getDiscipline() const { return discipline; }

 private:
  using FixtureKey = std::tuple<GameDateValue, TeamID, TeamID>;

  void refreshLeaguePoints(const Calendar& calendar, LeagueID league_id);
  std::vector<TeamID> competitionTeams(MatchType competition_type,
                                       LeagueID competition_id) const;

  std::shared_ptr<GameData> gamedata;
  std::shared_ptr<DatabaseConnection> db_conn;
  uint16_t current_season = 1;
  std::map<FixtureKey, MatchReport> pending_reports;
  PlayerSeasonTable player_stats;
  std::set<LeagueID> dirty_leagues;
  std::vector<SeasonHistoryEntry> season_history;
  Discipline discipline;
  bool history_dirty = false;
};
