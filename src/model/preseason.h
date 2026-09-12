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
class Inbox;
struct MatchReport;

/** @brief Pre-season training camp (values are persisted). */
enum class TrainingCamp : std::uint8_t
{
  None = 0,
  Domestic,
  Abroad,
  COUNT
};

/** @brief Opponent strength relative to the managed club. */
enum class OpponentLevel : std::uint8_t
{
  Weaker = 0,
  Similar,
  Stronger,
  COUNT
};

/** @brief A pre-season friendly of the managed club. */
struct FriendlySlot
{
  GameDateValue date;
  TeamID opponent_id = 0;
  bool home = true;
  bool tour = false;     /*!< Away match abroad booked as a tour date. */
  bool editable = false; /*!< Not played and still in the future. */
};

/** @brief A club that can be invited to a friendly. */
struct OpponentOption
{
  TeamID team_id = 0;
  std::uint8_t reputation = 0;
  bool abroad = false;
};

/** @brief Cost, dates and effect of a training camp. */
struct CampQuote
{
  TrainingCamp camp = TrainingCamp::None;
  std::int64_t cost = 0;
  float sharpness = 0.0f;   /*!< Added to every fit squad member. */
  float familiarity = 0.0f; /*!< Tactical familiarity points. */
  float morale = 0.0f;
  GameDateValue start;
  GameDateValue end; /*!< The effect applies on this day. */
  bool available = false;
};

/** @brief A suggested friendly (the assistant's plan). */
struct FriendlySuggestion
{
  GameDateValue date;
  TeamID opponent_id = 0;
  bool home = true;
};

/** @brief The season's pre-season choices (persisted). */
struct PreseasonState
{
  std::uint16_t season_year = 0;
  TrainingCamp camp = TrainingCamp::None;
  std::int64_t camp_cost = 0;
  GameDateValue camp_end; /*!< The camp's effect applies on this day. */
  bool camp_applied = false;
  std::vector<GameDateValue> tour_dates;
  std::uint8_t tour_matches = 0; /*!< Tour matches played. */
  bool tour_reputation = false;  /*!< The tour's reputation point given. */
};

/**
 * @namespace Preseason
 * @brief Pricing of camps and tours.
 *
 * A week-long camp between the first and second friendly sharpens the squad
 * (match fitness), drills the tactic and bonds the group; abroad it costs
 * more and does more. Tour matches abroad earn an appearance fee that grows
 * with the club's income and the opponent's stature, minus travel; two tour
 * matches in a season add a reputation point. [P]
 */
namespace Preseason
{
inline constexpr int TOUR_MATCHES_FOR_REPUTATION = 2;
/** Reputation gap separating weaker / similar / stronger opponents. */
inline constexpr int LEVEL_GAP = 8;
inline constexpr int CAMP_DAYS = 6;

/** Camp quote from the club's season income and the first friendly. */
CampQuote campQuote(TrainingCamp camp, double season_income,
                    const GameDateValue& first_friendly);

/** Net appearance fee of one tour match (fee minus travel). */
std::int64_t tourFee(double season_income, std::uint8_t opponent_reputation);

/** Whether @p other fits @p level for a club of reputation @p own. */
bool levelMatches(OpponentLevel level, std::uint8_t own, std::uint8_t other);
}  // namespace Preseason

/**
 * @class PreseasonPlanner
 * @brief Friendlies, tour and training camp of the managed club.
 *
 * Every club plays the generated pre-season friendlies, one a week (spread
 * from Tuesday to Sunday). Choosing an opponent swaps fixtures within that
 * week (the managed club's old opponent meets the new opponent's old one on
 * the day the new opponent was to play), so nobody plays twice a week or
 * loses a match. Away friendlies against clubs from another country can be booked
 * as tour dates.
 */
class PreseasonPlanner
{
 public:
  /** The managed club's friendlies before its first competitive match. */
  std::vector<FriendlySlot> friendlies(const Calendar& calendar,
                                       TeamID managed_team_id,
                                       const GameDateValue& today) const;

  /** Clubs free to be swapped in on @p date (their only match that week is
   * an unplayed friendly, or none), best fit first. */
  std::vector<OpponentOption> opponents(const GameData& gamedata,
                                        const Calendar& calendar,
                                        TeamID managed_team_id,
                                        const GameDateValue& date,
                                        OpponentLevel level,
                                        bool abroad) const;

  /**
   * Sets the opponent and venue of the friendly on @p date; @p tour books it
   * as a tour date (only away against a club from another country).
   */
  bool setFriendly(Calendar& calendar, const GameData& gamedata,
                   TeamID managed_team_id, const GameDateValue& date,
                   TeamID opponent_id, bool home, bool tour,
                   const GameDateValue& today);

  /** The assistant's plan: weaker and similar sides at home, a stronger
   * one away (abroad for clubs with a following). */
  std::vector<FriendlySuggestion> suggest(const GameData& gamedata,
                                          const Calendar& calendar,
                                          TeamID managed_team_id,
                                          const GameDateValue& today) const;

  /** The camp the assistant recommends for this club's finances. */
  TrainingCamp suggestCamp(const GameData& gamedata, const Calendar& calendar,
                           TeamID managed_team_id,
                           const GameDateValue& today) const;

  CampQuote campQuote(const GameData& gamedata, const Calendar& calendar,
                      TeamID managed_team_id, TrainingCamp camp) const;

  /**
   * Books (or with None cancels, refunded) the camp before it starts; the
   * cost is paid today.
   */
  bool bookCamp(GameData& gamedata, const Calendar& calendar,
                TeamID managed_team_id, TrainingCamp camp,
                const GameDateValue& today);

  /** Applies the camp on its last day. */
  void onDay(GameData& gamedata, const GameDateValue& date,
             TeamID managed_team_id, Inbox& inbox);

  /** Tour fees and the tour's reputation point. */
  void onMatchPlayed(GameData& gamedata, const MatchReport& report,
                     TeamID managed_team_id);

  /** A new season: last season's choices are archived. */
  void onSeasonStart(std::uint16_t season_year);

  const PreseasonState& getState() const { return state; }

  void clear();
  void load(const std::shared_ptr<DatabaseConnection>& db_conn);
  void save(const std::shared_ptr<DatabaseConnection>& db_conn) const;

 private:
  std::optional<GameDateValue> firstFriendly(const Calendar& calendar,
                                             TeamID managed_team_id) const;

  PreseasonState state;
};
