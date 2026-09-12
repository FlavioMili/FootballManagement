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
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "global/types.h"
#include "model/gamedate.h"

class Game;
class GameData;
struct AwardRecord;
struct ManagerAward;
struct ManagerSeasonLine;
struct ManagerStint;
struct RecordEntry;
struct SeasonHistoryEntry;
struct TransferRecord;

/** @brief What happened at a point of the manager's career. */
enum class TimelineKind : std::uint8_t
{
  Appointed = 0, /*!< Took charge of a club. */
  Departed,      /*!< Sacked, resigned, moved on or out of contract. */
  Trophy,        /*!< League title or cup won. */
  Award,         /*!< Manager of the Month / Season. */
  Promotion,
  Relegation,
  Season,        /*!< League finish against the board's expectation. */
  Record,        /*!< A club record set on his watch. */
  Signing,       /*!< One of the biggest signings of a spell. */
  COUNT
};

/** Language key naming @p kind (e.g. "TIMELINE_KIND_TROPHY"). */
const char* timelineKindKey(TimelineKind kind);

/** @brief One dated entry of the career timeline. */
struct TimelineEntry
{
  GameDateValue date;
  TimelineKind kind = TimelineKind::Appointed;
  std::uint16_t season_year = 0; /*!< Calendar year the season started. */
  TeamID team_id = 0;            /*!< The club he was in charge of. */
  PlayerID player_id = 0;        /*!< Signings: the player. */
  std::string text_key;
  std::vector<std::string> args;

  /** Text in the current language. */
  std::string text() const;
};

/** @brief The saved career facts the timeline is built from. */
struct TimelineSources
{
  const GameData* gamedata = nullptr;
  std::string manager_name;
  std::span<const ManagerStint> stints;
  std::span<const ManagerSeasonLine> seasons;
  std::span<const ManagerAward> honours;
  /** Every league honour (the manager's monthly and season awards). */
  std::span<const AwardRecord> awards;
  std::span<const TransferRecord> transfers;
  /** Club records of the clubs he managed. */
  std::span<const RecordEntry> records;
  std::span<const SeasonHistoryEntry> history;
};

/**
 * @brief The manager's career across every club: spells, trophies,
 * honours, promotions and relegations, season finishes, club records and
 * key signings, oldest first. Pure and deterministic (identical after a
 * reload); also exported as a Markdown journal.
 */
namespace CareerTimeline
{
/** Biggest signings listed per spell. */
inline constexpr std::size_t KEY_SIGNINGS_PER_STINT = 3;

std::vector<TimelineEntry> build(const TimelineSources& sources);

/**
 * The sources of a running game. The club records of the manager's clubs
 * are copied into @p records, which must outlive the returned sources.
 */
TimelineSources sourcesFor(const Game& game, const GameData& gamedata,
                           std::vector<RecordEntry>& records);

/**
 * The career journal as Markdown, in the current language: a summary of
 * the spells, then the timeline grouped by season.
 */
std::string toMarkdown(const TimelineSources& sources,
                       std::span<const TimelineEntry> entries,
                       const GameDateValue& today);

/** File name of a journal: "career-<name>-<date>.md" (ASCII only). */
std::string journalFileName(std::string_view manager_name,
                            const GameDateValue& today);

/**
 * Writes @p markdown to @p folder / @p file_name, creating the folder.
 * Returns the written path, or nullopt when the file could not be written.
 */
std::optional<std::filesystem::path> writeJournal(
    const std::string& markdown, const std::filesystem::path& folder,
    const std::string& file_name);
}  // namespace CareerTimeline
