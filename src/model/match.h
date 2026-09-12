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
#include <functional>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "gamedate.h"
#include "global/stats_config.h"
#include "global/types.h"

class GameData;
class Lineup;
class MatchEngine;
class Player;
struct MatchReport;
struct MatchSimulationInput;
struct MatchSimulationResult;
struct PlayerMatchConsequence;

/**
 * @class Match
 * @brief Represents a football match between two teams.
 */
class Match
{
 public:
  /**
   * @brief Constructs a Match object.
   * @param home_id The ID of the home team.
   * @param away_id The ID of the away team.
   * @param date The date of the match.
   * @param type The type of the match (e.g., league, friendly).
   * @param competition League ID (league matches) or country root league ID
   * (cup matches); 0 for friendlies.
   * @param stage League round or cup round, 1-based; 0 when unknown.
   */
  Match(TeamID home_id, TeamID away_id, GameDateValue date, MatchType type,
        LeagueID competition = 0, uint8_t stage = 0);

  /**
   * @brief Simulates the match using the provided game data.
   *
   * Drawn cup matches are decided by extra time and, if still level, a
   * penalty shootout.
   * Players start with their persistent condition.
   * @param game_data Reference to the GameData used for simulation.
   * @param report Optional report filled with the structured match summary.
   * @param consequences Optional physical outcome of every participant
   * (engine end condition and injuries) for WorldSimulation.
   */
  void simulate(const GameData& game_data, MatchReport* report = nullptr,
                std::vector<PlayerMatchConsequence>* consequences = nullptr);

  /**
   * Captures lineups, strategies, seed and (for cup ties) the extra-time
   * resolution, so the match can be simulated off this thread with
   * MatchSimulation::run(). nullopt when played or a team is missing.
   */
  std::optional<MatchSimulationInput> prepareSimulation(
      const GameData& game_data) const;

  /**
   * Records a simulated result. Returns its report with the identity and
   * final result filled in.
   */
  MatchReport applySimulation(MatchSimulationResult result);

  /**
   * @brief Gets the ID of the home team.
   * @return The home team's ID.
   */
  TeamID getHomeTeamId() const;

  /**
   * @brief Gets the ID of the away team.
   * @return The away team's ID.
   */
  TeamID getAwayTeamId() const;

  /**
   * @brief Gets the home team's score.
   * @return The score of the home team.
   */
  uint8_t getHomeScore() const;

  /**
   * @brief Gets the away team's score.
   * @return The score of the away team.
   */
  uint8_t getAwayScore() const;

  /**
   * @brief Gets the type of the match.
   * @return The match type.
   */
  MatchType getMatchType() const;

  /**
   * @brief Gets the date of the match.
   * @return The date value.
   */
  const GameDateValue& getDate() const;

  /**
   * @brief Checks if the match has been played.
   * @return True if the match has been played, false otherwise.
   */
  bool isPlayed() const;

  void setPlayedResult(uint8_t h, uint8_t a);

  /** Records the final score after extra time plus an optional shootout. */
  void setKnockoutResult(uint8_t h, uint8_t a, bool extra_time,
                         std::optional<std::pair<uint8_t, uint8_t>> penalties);

  LeagueID getCompetitionId() const { return competition_id; }
  void setCompetitionId(LeagueID id) { competition_id = id; }
  uint8_t getStage() const { return stage; }

  /** True for knockout ties that cannot end in a draw. */
  bool isKnockout() const { return match_type == MatchType::CUP; }
  bool wentToExtraTime() const { return extra_time; }
  bool wentToPenalties() const { return penalties; }
  uint8_t getHomePenalties() const { return home_penalties; }
  uint8_t getAwayPenalties() const { return away_penalties; }

  /** Winner of a played match; nullopt for unplayed matches and draws. */
  std::optional<TeamID> getWinnerId() const;

  /** Deterministic per-fixture seed (teams and date). */
  uint32_t getSeed() const;

  /** Copies identity and final result (incl. extra time) into a report. */
  void writeResultTo(MatchReport& report) const;

 private:
  TeamID home_team_id;
  TeamID away_team_id;
  GameDateValue match_date;
  MatchType match_type;
  uint8_t home_score;
  uint8_t away_score;
  LeagueID competition_id;
  uint8_t stage;
  uint8_t home_penalties = 0;
  uint8_t away_penalties = 0;
  bool extra_time = false;
  bool penalties = false;
  bool _played = false;
};

/**
 * Selection rules shared by live and simulated matches: who may take part,
 * how an ineligible selection is replaced, and how the players' persistent
 * physical state enters and leaves the match engine.
 */
namespace MatchdaySquad
{
using Eligibility = std::function<bool(const Player&)>;

/** Selected players (goalkeeper, outfield, then reserves) who may not play. */
std::vector<PlayerID> ineligible(const Lineup& lineup,
                                 const Eligibility& eligible);

/**
 * Replaces ineligible starters with eligible reserves or unselected squad
 * players (same role first, then any keeper for the goalkeeper slot or any
 * outfield player for an outfield slot) and ineligible reserves with the
 * best remaining squad players. Returns the number of players replaced.
 */
std::size_t replaceIneligible(Lineup& lineup,
                              std::span<const Player* const> squad,
                              const Eligibility& eligible,
                              const StatsConfig& config);

/**
 * Who replaced whom between two selections of the same team: starters slot
 * by slot, then reserves that left the squad (replacement 0).
 */
std::vector<std::pair<PlayerID, PlayerID>> replacements(const Lineup& before,
                                                        const Lineup& after);

/** Starts the lineup's players with their persistent condition (0-100). */
void carryCondition(MatchEngine& engine, const Lineup& lineup);

/** Minutes, end condition (0-100) and injuries of every participant. */
std::vector<PlayerMatchConsequence> consequences(const MatchEngine& engine);
}  // namespace MatchdaySquad
