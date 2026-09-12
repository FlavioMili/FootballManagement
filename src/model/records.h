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
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "global/types.h"
#include "model/gamedate.h"

class DatabaseConnection;
class GameData;
struct MatchReport;

/** @brief Kind of a record (values are persisted). */
enum class RecordKind : std::uint8_t
{
  BiggestWin = 0,      /*!< value = margin, value2 = goals scored. */
  BiggestDefeat,       /*!< value = margin, value2 = goals conceded. */
  HighestScoringMatch, /*!< value = total goals. */
  HighestAttendance,   /*!< value = crowd. */
  MostGoalsSeason,     /*!< value = league goals of a club in a season. */
  MostPointsSeason,    /*!< value = league points of a club in a season. */
  TopScorerSeason,     /*!< value = competitive goals of a player. */
  RecordSigning,       /*!< value = fee paid. */
  RecordSale,          /*!< value = fee received. */
  COUNT
};

/** Language key naming @p kind (e.g. "RECORD_BIGGEST_WIN"). */
const char* recordKindKey(RecordKind kind);

/** @brief Whether a record belongs to a club or to a league. */
enum class RecordScope : std::uint8_t
{
  Club = 0,
  League
};

/**
 * @struct RecordEntry
 * @brief The current holder of one record.
 *
 * team_id is the club that set it (for league records: the club whose
 * match, season or player it was), opponent_id the other side of a match.
 * Names are copied because players retire and leave the database.
 */
struct RecordEntry
{
  RecordScope scope = RecordScope::Club;
  std::uint32_t scope_id = 0; /*!< TeamID or LeagueID. */
  RecordKind kind = RecordKind::BiggestWin;
  std::int64_t value = 0;
  std::int32_t value2 = 0;
  TeamID team_id = 0;
  TeamID opponent_id = 0;
  PlayerID player_id = 0;
  std::string name; /*!< Player name (scorer, signing), else empty. */
  GameDateValue date;
  std::uint16_t season_year = 0;
  std::uint8_t goals_for = 0; /*!< Match records: score of team_id. */
  std::uint8_t goals_against = 0;
  bool home = true; /*!< Match records: team_id played at home. */
};

/** @brief Competitive appearances and goals of a player for one club. */
struct ClubPlayerTotal
{
  TeamID team_id = 0;
  PlayerID player_id = 0;
  std::string name;
  std::uint16_t appearances = 0;
  std::uint16_t goals = 0;
  std::uint16_t first_year = 0; /*!< Season of the first appearance. */
  std::uint16_t last_year = 0;
};

/** @brief All-time league table row of a club (league matches only). */
struct AllTimeRow
{
  LeagueID league_id = 0;
  TeamID team_id = 0;
  std::uint32_t played = 0;
  std::uint32_t won = 0;
  std::uint32_t drawn = 0;
  std::uint32_t lost = 0;
  std::uint32_t goals_for = 0;
  std::uint32_t goals_against = 0;
  std::uint32_t points = 0;
};

/** @brief A club legend in the hall of fame. */
struct LegendEntry
{
  ClubPlayerTotal totals;
  int honours = 0; /*!< Individual season awards won at the club. */
  int score = 0;
};

/**
 * @namespace Records
 * @brief Comparison rules of the records book.
 *
 * A challenger must strictly beat the holder: on a tie the earlier holder
 * keeps the record. Biggest wins and defeats compare the margin, then the
 * goals scored (conceded).
 */
namespace Records
{
inline constexpr std::uint16_t LEGEND_APPEARANCES = 150;
inline constexpr std::uint16_t LEGEND_GOALS = 60;
inline constexpr std::uint16_t LEGEND_HONOURS_APPEARANCES = 60;
inline constexpr int LEGEND_HONOURS = 2;
inline constexpr std::size_t HALL_OF_FAME_SIZE = 12;

/** True when @p challenger takes the record from @p holder. */
bool beats(const RecordEntry& challenger, const RecordEntry& holder);

/** Whether a club career qualifies for the hall of fame. */
bool isLegend(const ClubPlayerTotal& totals, int honours);

/** Ranking score of a legend: appearances + 2 x goals + 30 x honours. */
int legendScore(const ClubPlayerTotal& totals, int honours);
}  // namespace Records

/**
 * @class RecordBook
 * @brief Club and league records, club player totals, all-time tables and
 * the hall of fame.
 *
 * Updated incrementally from every competitive match report and completed
 * transfer; season records are settled when the season closes. Friendlies
 * never count. Old saves are rebuilt once from the stored match reports and
 * transfer history.
 */
class RecordBook
{
 public:
  /** Adds a competitive match (attendance already set). */
  void onMatchPlayed(const GameData& gamedata, const MatchReport& report);

  /** A completed move with a fee: record signing / sale. */
  void onTransfer(const GameData& gamedata, const GameDateValue& date,
                  PlayerID player_id, TeamID from_team_id, TeamID to_team_id,
                  std::uint32_t fee);

  /** Settles the season records of @p season_year and starts a new season. */
  void closeSeason(const GameData& gamedata, std::uint16_t season_year);

  /** Records of a club / league in RecordKind order (only those set). */
  std::vector<RecordEntry> clubRecords(TeamID team_id) const;
  std::vector<RecordEntry> leagueRecords(LeagueID league_id) const;

  /** A club's all-time scorers / appearance makers, best first. */
  std::vector<ClubPlayerTotal> topScorers(TeamID team_id,
                                          std::size_t limit) const;
  std::vector<ClubPlayerTotal> mostAppearances(TeamID team_id,
                                               std::size_t limit) const;

  /** All-time league table, most points first. */
  std::vector<AllTimeRow> allTimeTable(LeagueID league_id) const;

  /**
   * Legends of a club, best first. @p honours counts a player's individual
   * season awards at the club.
   */
  std::vector<LegendEntry> hallOfFame(
      TeamID team_id,
      const std::function<int(PlayerID, TeamID)>& honours) const;

  /** Competitive totals of a player at a club (zeros when none). */
  ClubPlayerTotal playerTotals(TeamID team_id, PlayerID player_id) const;

  bool empty() const { return records.empty() && club_players.empty(); }
  void clear();
  /** Loads the book; a save written before it existed is rebuilt from the
   * stored match reports and transfer history. */
  void load(const std::shared_ptr<DatabaseConnection>& db_conn,
            const GameData& gamedata);
  void save(const std::shared_ptr<DatabaseConnection>& db_conn) const;

 private:
  using RecordKey = std::tuple<RecordScope, std::uint32_t, RecordKind>;

  struct SeasonTeam
  {
    LeagueID league_id = 0;
    std::uint16_t played = 0;
    std::uint16_t goals_for = 0;
    std::uint16_t points = 0;
  };

  void offer(RecordEntry entry);
  void rebuild(const std::shared_ptr<DatabaseConnection>& db_conn,
               const GameData& gamedata);

  std::map<RecordKey, RecordEntry> records;
  std::map<std::pair<TeamID, PlayerID>, ClubPlayerTotal> club_players;
  std::map<std::pair<LeagueID, TeamID>, AllTimeRow> all_time;
  std::map<TeamID, SeasonTeam> season_teams;
  std::map<std::pair<TeamID, PlayerID>, std::uint16_t> season_scorers;
  std::uint16_t season_year = 0; /*!< Season of season_teams/scorers. */
};
