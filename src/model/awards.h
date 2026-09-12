// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "global/types.h"
#include "model/gamedate.h"

class DatabaseConnection;
class GameData;
class Inbox;
struct BoardState;
struct MatchReport;

/**
 * @enum AwardType
 * @brief League honours (values are persisted).
 */
enum class AwardType : std::uint8_t
{
  PlayerOfMonth = 0,
  YoungPlayerOfMonth,
  ManagerOfMonth,
  GoalOfMonth,
  PlayerOfSeason,
  YoungPlayerOfSeason,
  GoldenBoot,
  GoldenGlove,
  TeamOfSeason,
  ManagerOfSeason,
  COUNT
};

/** Language key naming @p type (e.g. "AWARD_PLAYER_OF_MONTH"). */
const char* awardTypeKey(AwardType type);

/**
 * @struct AwardRecord
 * @brief One honour given in a league.
 *
 * Manager awards name the club (player_id 0). The player's name is kept
 * because winners retire and leave the database. value/count depend on the
 * award: average rating and appearances (player awards), goals and minutes
 * (Golden Boot), clean sheets and appearances (Golden Glove), points above
 * expectation and matches (manager awards), goal minute (Goal of the Month).
 */
struct AwardRecord
{
  std::uint16_t season_year = 0; /*!< Calendar year the season started. */
  std::uint8_t month = 0;        /*!< 1-12 for monthly awards, else 0. */
  LeagueID league_id = 0;
  AwardType type = AwardType::PlayerOfMonth;
  PlayerID player_id = 0;
  TeamID team_id = 0;
  TeamID opponent_id = 0; /*!< Goal of the Month: the opponent. */
  std::string name;
  float value = 0.0f;
  std::uint16_t count = 0;
  std::uint8_t slot = 0; /*!< Team of the Season position (0 = GK). */
  GameDateValue date;    /*!< Goal of the Month: match date. */
};

/** @brief League matches of a player in a month or season. */
struct AwardPlayerTally
{
  LeagueID league_id = 0;
  PlayerID player_id = 0;
  TeamID team_id = 0; /*!< Latest club. */
  std::uint16_t appearances = 0;
  std::uint16_t minutes = 0;
  std::uint16_t goals = 0;
  std::uint16_t assists = 0;
  std::uint16_t clean_sheets = 0;
  float rating_total = 0.0f;
  std::uint16_t rated = 0;

  float averageRating() const
  {
    return rated == 0 ? 0.0f : rating_total / static_cast<float>(rated);
  }
};

/** @brief League results of a club against expectation. */
struct AwardClubTally
{
  TeamID team_id = 0;
  LeagueID league_id = 0;
  std::uint16_t matches = 0;
  float points = 0.0f;
  float expected_points = 0.0f;
  std::uint16_t goals_for = 0;
  std::uint16_t goals_against = 0;

  float overPerformance() const { return points - expected_points; }
};

/** @brief Best goal of the month so far in a league. */
struct GoalCandidate
{
  LeagueID league_id = 0;
  GameDateValue date;
  PlayerID scorer = 0;
  TeamID team_id = 0;
  TeamID opponent_id = 0;
  std::uint8_t minute = 0;
  float score = 0.0f;
};

/** @brief A player considered for an award. */
struct AwardCandidate
{
  PlayerRole role = PlayerRole::UNKNOWN;
  int age = 0;
  AwardPlayerTally tally;
};

/**
 * @namespace Awards
 * @brief Selection rules of the league honours.
 *
 * Player awards rank by average match rating plus a small bonus for goals,
 * assists and (goalkeepers) clean sheets per appearance, among players with
 * enough minutes; ties go to the lower player id so the result never
 * depends on iteration order. Manager awards compare league points with the
 * points expected from both line-ups (BoardModel::expectedPoints).
 */
namespace Awards
{
/** Three full matches in the month. */
inline constexpr std::uint16_t MONTH_MIN_MINUTES = 270;
/** Share of the league's most-used player's minutes for season awards. */
inline constexpr float SEASON_MIN_SHARE = 0.5f;
inline constexpr int YOUNG_MAX_AGE = 21;
inline constexpr std::uint16_t MONTH_MIN_MATCHES = 3;
inline constexpr std::uint16_t SEASON_MIN_MATCHES = 10;
/** A clean sheet needs this many minutes from the start without conceding. */
inline constexpr std::uint8_t CLEAN_SHEET_MINUTES = 60;
inline constexpr std::size_t TEAM_OF_SEASON_SIZE = 11;
/** Market value premium caps (see valueMultiplier). */
inline constexpr float MAX_VALUE_PREMIUM = 0.25f;

/** Rating plus contribution bonus of a tally. */
float playerScore(const AwardPlayerTally& tally, bool goalkeeper);

/** Best candidate with enough minutes and at most @p max_age (0 = any). */
std::optional<std::size_t> bestPlayer(std::span<const AwardCandidate> candidates,
                                      std::uint16_t min_minutes,
                                      int max_age = 0);

/** Most goals; ties: fewer minutes, more assists, lower id. */
std::optional<std::size_t> goldenBoot(
    std::span<const AwardCandidate> candidates);

/** Goalkeeper with most clean sheets and enough minutes; ties: fewer
 * minutes per clean sheet, lower id. */
std::optional<std::size_t> goldenGlove(
    std::span<const AwardCandidate> candidates, std::uint16_t min_minutes);

/**
 * Best XI in a 4-3-3: GK, RB, CB, CB, LB, three midfielders, RW, ST, LW.
 * Slots are filled from natural positions first, then from neighbouring
 * ones; a slot stays empty when nobody qualifies.
 */
std::array<std::optional<std::size_t>, TEAM_OF_SEASON_SIZE> teamOfSeason(
    std::span<const AwardCandidate> candidates, std::uint16_t min_minutes);

/** Language key of a Team of the Season slot (e.g. "AWARD_SLOT_GK"). */
const char* slotKey(std::uint8_t slot);

/** Largest points-over-expectation with enough matches; ties: points,
 * lower team id. */
std::optional<std::size_t> bestManager(std::span<const AwardClubTally> clubs,
                                       std::uint16_t min_matches);

/** Minimum minutes for season awards among @p candidates. */
std::uint16_t seasonMinMinutes(std::span<const AwardCandidate> candidates);

/**
 * Importance of a goal for the Goal of the Month panel (no shot data is
 * stored): late goals, winners and equalisers, goals against stronger
 * opponents and goals without an assist score higher.
 * @param own_before,other_before Score of the scorer's side before it.
 * @param decisive The game-winning goal (the winner's goal after which the
 * opponent never drew level again).
 * @param reputation_gap Opponent reputation minus the scorer's club's.
 */
float goalScore(std::uint8_t minute, int own_before, int other_before,
                bool decisive, int reputation_gap, bool assisted);

/**
 * Market value multiplier from recent honours: +3% per monthly player
 * award of the last 12 months (at most three), +8% per individual season
 * award and +4% for the Team of the Season of the current or previous
 * season, capped at +25%.
 */
float valueMultiplier(std::span<const AwardRecord> honours,
                      const GameDateValue& today);
}  // namespace Awards

/**
 * @class AwardSystem
 * @brief Monthly and season honours of every league.
 *
 * Tallies league matches as they are played and hands out the monthly
 * awards on the first day of the next month and the season awards on 1
 * June, when every league has finished. The history is kept for the whole
 * career. Winners gain morale; the managed club's manager awards please the
 * board; the Manager of the Season's club gains a reputation point.
 */
class AwardSystem
{
 public:
  /** Adds a league match (ratings already set). */
  void onMatchPlayed(const GameData& gamedata, const MatchReport& report,
                     float home_expected, float away_expected);

  /**
   * First day of a month: awards the previous month (and on 1 June the
   * season), applies the effects and posts the managed league's news.
   * @return The honours given today.
   */
  std::vector<AwardRecord> onMonthStart(GameData& gamedata,
                                        const GameDateValue& date,
                                        TeamID managed_team_id, Inbox& inbox,
                                        BoardState& board);

  /** Monthly awards of the tallies collected so far (clears them). */
  std::vector<AwardRecord> awardMonth(const GameData& gamedata,
                                      std::uint16_t season_year,
                                      std::uint8_t month);

  /** Season awards of the tallies collected so far (clears them). */
  std::vector<AwardRecord> awardSeason(const GameData& gamedata,
                                       std::uint16_t season_year);

  /** Every honour, oldest first. */
  const std::vector<AwardRecord>& history() const { return records; }
  /** Honours of a player, newest first. */
  std::vector<AwardRecord> honoursFor(PlayerID player_id) const;
  /** Honours given in a league in a season (all months), in award order. */
  std::vector<AwardRecord> forLeague(LeagueID league_id,
                                     std::uint16_t season_year) const;
  /** Individual season honours won by a player at a club. */
  int seasonHonours(PlayerID player_id, TeamID team_id) const;
  /** Market value multiplier of a player (see Awards::valueMultiplier). */
  float valueMultiplier(PlayerID player_id, const GameDateValue& today) const;

  const std::map<std::pair<LeagueID, PlayerID>, AwardPlayerTally>&
  seasonTallies() const
  {
    return season_players;
  }
  const std::map<std::pair<LeagueID, PlayerID>, AwardPlayerTally>&
  monthTallies() const
  {
    return month_players;
  }

  void clear();
  void load(const std::shared_ptr<DatabaseConnection>& db_conn);
  void save(const std::shared_ptr<DatabaseConnection>& db_conn) const;

 private:
  using PlayerTallies = std::map<std::pair<LeagueID, PlayerID>, AwardPlayerTally>;
  using ClubTallies = std::map<TeamID, AwardClubTally>;

  std::vector<AwardCandidate> candidates(const GameData& gamedata,
                                         const PlayerTallies& tallies,
                                         LeagueID league_id) const;
  void addRecord(const GameData& gamedata, AwardRecord record,
                 std::vector<AwardRecord>& given);

  std::vector<AwardRecord> records;
  PlayerTallies month_players;
  PlayerTallies season_players;
  ClubTallies month_clubs;
  ClubTallies season_clubs;
  std::map<LeagueID, GoalCandidate> month_goals;
};
