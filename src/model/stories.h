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
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "global/types.h"
#include "model/gamedate.h"

class DatabaseConnection;
class GameData;
class Inbox;
class InteractionSystem;
struct MatchReport;

/**
 * @brief Story chains that grow out of what actually happens in the save.
 *
 * Each story is triggered by a real event of the managed club (a debut, a
 * string of high ratings, a long injury ending, repeated bids for a key
 * player...) and produces at most one inbox message per instance. Stories
 * that call for a decision carry a StoryChoice: the inbox offers to talk to
 * the player with the conversation options that answer it. A weekly cap
 * keeps busy periods from flooding the inbox.
 */
enum class StoryKind : std::uint8_t
{
  Debut = 0,
  Breakout,
  CaptainDispute,
  PoorRun,
  TransferSaga,
  Comeback,
  Milestone,
  Rivalry
};

/** Competitive career totals of a player (all clubs). */
struct CareerTotals
{
  std::uint32_t appearances = 0;
  std::uint32_t goals = 0;
};

/** A story waiting for the manager's answer. */
struct StoryChoice
{
  StoryKind kind = StoryKind::TransferSaga;
  PlayerID player_id = 0;
  std::int32_t day = 0;
  std::int32_t expires_day = 0;
};

/** Dedupe record: a story fired for (kind, entity, key) on day. */
struct StoryRecord
{
  StoryKind kind = StoryKind::Debut;
  std::uint32_t entity = 0;
  std::uint32_t key = 0;
  std::int32_t day = 0;
};

/** @brief Persisted state of the story engine. */
struct StoryState
{
  std::vector<StoryRecord> records;
  std::vector<StoryChoice> choices;
  /** Managed players currently injured: first day out. */
  std::unordered_map<PlayerID, std::int32_t> injury_start;
  /** Returned from a long injury, story due at the next appearance: days
   * out. */
  std::unordered_map<PlayerID, std::int32_t> comeback_due;
  /** Recent bids for managed players: (player, day). */
  std::vector<std::pair<PlayerID, std::int32_t>> bids;
};

namespace Stories
{
/** Stories posted per rolling week at most. */
constexpr int WEEKLY_CAP = 2;
/** Injuries at least this long make a comeback story. */
constexpr std::int32_t LONG_INJURY_DAYS = 60;
/** Mean recent rating of a young player that makes a breakout. */
constexpr float BREAKOUT_RATING = 7.3f;
constexpr int BREAKOUT_MAX_AGE = 21;
/** Competitive matches without a win that make a poor run. */
constexpr int POOR_RUN_MATCHES = 5;
/** Bids within this many days that make a transfer saga. */
constexpr std::int32_t SAGA_WINDOW_DAYS = 30;
constexpr std::int32_t CHOICE_DAYS = 21;

/** Appearance milestones crossed from @p before to @p after (e.g. 100). */
std::optional<std::uint32_t> appearanceMilestone(std::uint32_t before,
                                                 std::uint32_t after);
/** Goal milestones crossed from @p before to @p after (e.g. 50). */
std::optional<std::uint32_t> goalMilestone(std::uint32_t before,
                                           std::uint32_t after);
/** True when the latest results (newest first, 'W'/'D'/'L') hold a poor
 * run: POOR_RUN_MATCHES or more without a win. */
bool isPoorRun(std::string_view recent_form);
/** Language key of a story kind. */
const char* kindKey(StoryKind kind);
}  // namespace Stories

/**
 * @class StoryEngine
 * @brief Detects story triggers for the managed club and posts them.
 */
class StoryEngine
{
 public:
  explicit StoryEngine(std::shared_ptr<GameData> gamedata);

  /**
   * Competitive career totals before the match being processed (the
   * competitions record a result after the world simulation). Queried once
   * per player and session, then kept up to date from match reports.
   */
  void setCareerProvider(std::function<CareerTotals(PlayerID)> provider);

  /** Daily: long-injury tracking, captain disputes (weekly), expiries. */
  void onDayAdvanced(const GameDateValue& date, TeamID managed_team_id,
                     const InteractionSystem& interactions, Inbox& inbox);
  /** Debut, breakout, milestones, comeback, poor run and rivalry. */
  void onMatchPlayed(const MatchReport& report, TeamID managed_team_id,
                     const std::string& recent_form, Inbox& inbox);
  /** Transfer saga: a second bid within a month for a key player. */
  void onTransferBid(const GameDateValue& date, PlayerID player_id,
                     TeamID bidder_id, bool key_player,
                     TeamID managed_team_id, Inbox& inbox);
  /** A player left: his open choices lapse. */
  void onPlayerLeft(PlayerID player_id);
  /** Rivals are recomputed at the start of each season. */
  void onSeasonStart();

  /** Same-league club of the nearest reputation (lower id on ties). */
  std::optional<TeamID> rivalOf(TeamID team_id) const;

  /** Open decision for a player, if any. */
  std::optional<StoryChoice> choiceFor(PlayerID player_id,
                                       std::int32_t today) const;
  /** The manager answered (or dismissed) the player's story. */
  void resolveChoice(PlayerID player_id);

  const StoryState& getState() const { return state; }
  void restore(StoryState restored);
  void load(const std::shared_ptr<DatabaseConnection>& db_conn);
  void save(const std::shared_ptr<DatabaseConnection>& db_conn) const;

 private:
  bool fired(StoryKind kind, std::uint32_t entity, std::uint32_t key) const;
  bool firedSince(StoryKind kind, std::uint32_t entity,
                  std::int32_t since_day) const;
  /** Posts when the weekly cap allows; records the story either way. */
  bool post(const GameDateValue& date, StoryKind kind, std::uint32_t entity,
            std::uint32_t key, const char* title_key, const char* body_key,
            std::vector<std::string> args, std::optional<PlayerID> player_id,
            std::optional<TeamID> team_id, Inbox& inbox,
            bool with_choice = false);
  CareerTotals careerBefore(PlayerID player_id);

  std::shared_ptr<GameData> gamedata;
  StoryState state;
  std::function<CareerTotals(PlayerID)> career_provider;
  /** Transient cache of career totals, advanced from match reports. */
  std::unordered_map<PlayerID, CareerTotals> career_cache;
  mutable std::unordered_map<TeamID, std::optional<TeamID>> rivals;
};
