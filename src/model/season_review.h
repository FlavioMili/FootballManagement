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
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "global/types.h"
#include "model/board.h"
#include "model/gamedate.h"
#include "model/standings.h"

class DatabaseConnection;
class Inbox;

/** @brief The board's verdict on the manager's season (values are persisted). */
enum class SeasonVerdict : std::uint8_t
{
  Delighted = 0, /*!< Well beyond what the board asked for. */
  Pleased,       /*!< Objectives met with something to spare. */
  Satisfied,     /*!< Objectives broadly met, or too early to judge. */
  Warned,        /*!< Objectives missed: next season is on notice. */
  Sacked         /*!< The board ends the manager's contract. */
};

/** @brief What the board weighs at the end of a season. */
struct SeasonVerdictInputs
{
  BoardObjective objective = BoardObjective::MidTable;
  int target_position = 10; /*!< Worst acceptable finish. */
  int position = 10;        /*!< Final league position. */
  int league_size = 20;
  bool champion = false;
  bool promoted = false;
  bool relegated = false;
  /** The other targets, graded with BoardModel::gradeCup() and friends. */
  ObjectiveGrade cup = ObjectiveGrade::Met;
  ObjectiveGrade finances = ObjectiveGrade::Met;
  ObjectiveGrade youth = ObjectiveGrade::Met;
  /** False when the board never set the cup, finance and youth targets
   * this season (a save from before them): the league alone is judged. */
  bool targets_set = true;
  int league_matches = 38; /*!< League matches in charge this season. */
  float confidence = 60.0f; /*!< Before the verdict. */
  /** The previous season at this club ended with a warning. */
  bool warned_last_season = false;
};

/** @brief The verdict and the grades behind it. */
struct SeasonVerdictResult
{
  SeasonVerdict verdict = SeasonVerdict::Satisfied;
  ObjectiveGrade league = ObjectiveGrade::Met;
  ObjectiveGrade cup = ObjectiveGrade::Met;
  ObjectiveGrade finances = ObjectiveGrade::Met;
  ObjectiveGrade youth = ObjectiveGrade::Met;
  float confidence = 60.0f; /*!< After the verdict. */
};

/**
 * @struct SeasonReview
 * @brief The managed club's season as the end-of-season summary shows it
 * (persisted once per season).
 */
struct SeasonReview
{
  std::uint16_t season = 0;
  std::uint16_t start_year = 0;
  TeamID team_id = 0;
  LeagueID league_id = 0;
  LeagueID next_league_id = 0; /*!< After promotion or relegation. */
  std::uint8_t position = 0;
  std::uint8_t league_size = 0;
  std::uint8_t target_position = 0;
  BoardObjective objective = BoardObjective::MidTable;
  CupObjective cup_objective = CupObjective::None;
  FinanceObjective finance_objective = FinanceObjective::WithinWageBudget;
  std::uint8_t youth_target = 0;
  std::uint8_t young_regulars = 0;
  std::uint16_t played = 0;
  std::uint16_t won = 0;
  std::uint16_t drawn = 0;
  std::uint16_t lost = 0;
  std::uint16_t goals_for = 0;
  std::uint16_t goals_against = 0;
  std::uint16_t points = 0;
  bool champion = false;
  bool promoted = false;
  bool relegated = false;
  bool cup_won = false;
  /** Continental competition qualified for (0 = none). */
  LeagueID continental_id = 0;
  std::string top_scorer; /*!< The club's top league scorer, if any. */
  std::uint16_t top_scorer_goals = 0;
  std::int64_t prize_money = 0;
  std::int64_t balance = 0;
  float confidence_before = 0.0f;
  SeasonVerdictResult result;
  /** Next season's objectives (unset after a sacking). */
  std::optional<BoardObjective> next_objective;
  std::uint8_t next_target = 0;
  CupObjective next_cup = CupObjective::None;
  FinanceObjective next_finances = FinanceObjective::WithinWageBudget;
  std::uint8_t next_youth = 0;
  bool seen = false; /*!< The summary has been shown. */
};

/**
 * @namespace SeasonReviewModel
 * @brief The board's season-end verdict and the season-end news.
 */
namespace SeasonReviewModel
{
/** League matches in charge before the board judges a season at all. */
inline constexpr int MIN_MATCHES_TO_JUDGE = 12;
/** Confidence under which a failed season costs the job. */
inline constexpr float SACK_CONFIDENCE = 25.0f;
/** Places above the drop zone that still count as a relegation fight. */
inline constexpr int SURVIVAL_MARGIN = 3;

/**
 * Judges a season against the board's objectives. The league finish
 * weighs most; cup, finances and youth add or take away (when the board
 * set them for the season). A manager in charge for too few matches is not
 * judged; a failed season (league target missed, or the books failed
 * without a strong league finish) sacks him when confidence is low, when
 * the club went down although it was not expected to fight relegation, or
 * when he had already been warned.
 */
SeasonVerdictResult judge(const SeasonVerdictInputs& inputs);

/** Language keys (e.g. "SEASON_VERDICT_PLEASED", "SEASON_GRADE_MET"). */
const char* verdictKey(SeasonVerdict verdict);
const char* gradeKey(ObjectiveGrade grade);

/** Names used by the season-end news. */
struct EventNames
{
  std::string club;
  std::string league;
  std::string next_league;
  std::string continental; /*!< Name key of the continental competition. */
  std::string cup;
  int relegation_places = 3; /*!< 0 in the lowest division. */
};

/**
 * Posts the season's news for the managed club in order: champions,
 * promotion, relegation or survival, the domestic cup, continental
 * qualification, then the board's verdict.
 */
void postEvents(Inbox& inbox, const GameDateValue& date,
                const SeasonReview& review, const EventNames& names);
}  // namespace SeasonReviewModel

/**
 * @class SeasonArchive
 * @brief Final league tables of past seasons and the managed club's season
 * reviews (tables SeasonTables and SeasonReviews).
 */
class SeasonArchive
{
 public:
  /** Stores the final tables of @p season (before promotion/relegation). */
  void recordTables(std::uint16_t season, std::uint16_t start_year,
                    const std::map<LeagueID, std::vector<StandingRow>>& tables);
  void recordReview(SeasonReview review);

  /** Seasons with a stored final table of @p league_id, newest first. */
  std::vector<std::uint16_t> seasonsOf(LeagueID league_id) const;
  /** Final table of @p league_id in @p season (nullptr if not stored). */
  const std::vector<StandingRow>* table(std::uint16_t season,
                                        LeagueID league_id) const;
  /** Whether final tables of @p season are stored. */
  bool hasSeason(std::uint16_t season) const;
  /** Calendar year @p season started in (0 if not stored). */
  std::uint16_t startYear(std::uint16_t season) const;
  /** League and final position of @p team_id in @p season. */
  std::optional<std::pair<LeagueID, std::uint16_t>> placing(
      std::uint16_t season, TeamID team_id) const;

  const std::vector<SeasonReview>& reviews() const { return season_reviews; }
  SeasonReview* review(std::uint16_t season);
  const SeasonReview* review(std::uint16_t season) const;
  /** The newest review not shown yet, if any. */
  const SeasonReview* unseenReview() const;
  void markSeen(std::uint16_t season);

  void load(const DatabaseConnection& db);
  /** Writes new tables and every review (inside the caller's transaction). */
  void save(const DatabaseConnection& db) const;
  void onSaved();

 private:
  using TableKey = std::pair<std::uint16_t, LeagueID>;
  std::map<TableKey, std::vector<StandingRow>> tables;
  std::map<std::uint16_t, std::uint16_t> start_years;
  std::vector<SeasonReview> season_reviews;
  std::set<std::uint16_t> unsaved_seasons;
  bool reviews_dirty = false;
};
