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
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "global/types.h"
#include "model/awards.h"
#include "model/gamedate.h"
#include "model/inbox.h"
#include "model/manager_career.h"
#include "model/match.h"
#include "model/records.h"
#include "model/season_history.h"
#include "model/transfer_market.h"

class Calendar;
class Game;
class GameData;

/** @brief What a news story is about (filter of the news screen). */
enum class NewsKind : std::uint8_t
{
  Transfer = 0, /*!< Big-money moves and the managed club's deals. */
  Manager,      /*!< Appointments, sackings and departures. */
  Race,         /*!< Title races, new leaders, champions, promotions. */
  Upset,        /*!< Results against the odds. */
  Record,       /*!< Records set this season and later. */
  Award,        /*!< Monthly and season honours. */
  COUNT
};

/** Language key naming @p kind (e.g. "NEWS_KIND_TRANSFER"). */
const char* newsKindKey(NewsKind kind);

/** @brief A competition a story belongs to (id 0: none). */
struct NewsCompetition
{
  MatchType type = MatchType::LEAGUE;
  LeagueID id = 0;

  bool operator==(const NewsCompetition& other) const = default;
};

/**
 * @brief One story of the world news feed.
 *
 * Every story is built from saved facts and cites the entities it talks
 * about (a player, one or two clubs, a match). The headline is a language
 * key plus arguments (formatLocalized()), so it follows the UI language.
 */
struct NewsItem
{
  GameDateValue date;
  NewsKind kind = NewsKind::Transfer;
  LeagueID country = 0; /*!< Top league of the country; 0 = continental. */
  NewsCompetition competition;
  std::string headline_key;
  std::vector<std::string> args;
  PlayerID player_id = 0;
  TeamID team_id = 0;       /*!< The club the story is about. */
  TeamID other_team_id = 0; /*!< The other club (seller, opponent). */
  /** The story is a result: the report of this fixture. */
  bool has_match = false;
  GameDateValue match_date;
  TeamID home_id = 0;
  TeamID away_id = 0;
  std::uint32_t weight = 0; /*!< Bigger stories first within a day. */

  /** Headline in the current language. */
  std::string headline() const;
};

/** @brief The saved facts the feed is built from. */
struct NewsSources
{
  const GameData* gamedata = nullptr;
  /** This season's fixtures (results, upsets, title races); optional. */
  const Calendar* calendar = nullptr;
  std::span<const TransferRecord> transfers;
  /** The human manager's spells in charge and his name. */
  std::span<const ManagerStint> stints;
  std::string manager_name;
  /** Computer managers (their appointment dates) and open vacancies. */
  std::span<const AiManager> managers;
  std::span<const Vacancy> vacancies;
  /** Inbox messages: dismissal reports name the departed manager. */
  std::span<const InboxMessage> inbox;
  std::span<const AwardRecord> awards;
  /** League records, plus the managed club's own records. */
  std::span<const RecordEntry> records;
  std::span<const SeasonHistoryEntry> history;
  TeamID managed_team = 0;
  /** First day of the save: nothing earlier is news. */
  GameDateValue world_start;
};

/** @brief What the news screen shows (unset fields: everything). */
struct NewsFilter
{
  std::optional<LeagueID> country; /*!< 0 selects continental stories. */
  std::optional<NewsCompetition> competition;
  std::optional<NewsKind> kind;

  bool matches(const NewsItem& item) const;
};

/**
 * @brief The world news feed: dated stories derived from saved data.
 *
 * Pure and deterministic: the same saved facts give the same stories in
 * the same order (newest first), so the feed is identical after a reload.
 * It never writes anything (no inbox messages, no saved state).
 */
namespace NewsFeed
{
/** Fees from this amount make the world news (euros). */
inline constexpr std::uint32_t MAJOR_FEE = 12'000'000;
/** Reputation gap between loser and winner that makes a result an upset. */
inline constexpr int UPSET_REPUTATION_GAP = 18;
/** Matches a new league leader must have played to be news. */
inline constexpr std::uint16_t LEADER_MIN_PLAYED = 6;
/** Title race: at most this many games left and points between the top two. */
inline constexpr int RACE_GAMES_LEFT = 8;
inline constexpr int RACE_POINTS_GAP = 3;

/** Stories dated from @p from to @p to (inclusive), newest first. */
std::vector<NewsItem> build(const NewsSources& sources,
                            const GameDateValue& from, const GameDateValue& to);

/**
 * The feed of a running game: this season and the last one up to today,
 * from every saved source (transfers, managers, inbox dismissal reports,
 * awards, league records and the managed club's, season history and this
 * season's fixtures).
 */
std::vector<NewsItem> forGame(const Game& game, const GameData& gamedata);

/** Indices of the stories @p filter keeps, in feed order. */
std::vector<std::size_t> filter(std::span<const NewsItem> items,
                                const NewsFilter& filter);

/** Headline variant for a story: a hash of its facts, never a random draw. */
std::size_t variant(NewsKind kind, std::uint32_t first, std::uint32_t second,
                    const GameDateValue& date, std::size_t count);
}  // namespace NewsFeed
