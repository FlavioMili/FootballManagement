// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "model/stories.h"

#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <span>
#include <string>
#include <utility>

#include "database/database_connection.h"
#include "database/gamedata.h"
#include "model/inbox.h"
#include "model/interactions.h"
#include "model/match_report.h"
#include "model/player.h"
#include "model/team.h"
#include "model/world_rng.h"

namespace
{
constexpr std::array<std::uint32_t, 5> APPEARANCE_MILESTONES = {100, 200, 300,
                                                                400, 500};
constexpr std::array<std::uint32_t, 4> GOAL_MILESTONES = {50, 100, 150, 200};
/** Milestone keys: appearances as is, goals offset by this. */
constexpr std::uint32_t GOAL_KEY_OFFSET = 10'000;
constexpr std::int32_t POOR_RUN_COOLDOWN_DAYS = 60;
constexpr std::int32_t DISPUTE_COOLDOWN_DAYS = 120;
constexpr std::int32_t SAGA_COOLDOWN_DAYS = 120;
constexpr std::int32_t RECORD_RETENTION_DAYS = 3 * 365;
/** A captain this unhappy, or this far below his minutes, disputes. [P] */
constexpr float DISPUTE_MORALE = 35.0f;
constexpr float DISPUTE_SHARE = 0.35f;
/** Rivals are clubs of the same league within this reputation gap. */
constexpr int RIVAL_REPUTATION_GAP = 6;

std::optional<std::uint32_t> crossed(std::span<const std::uint32_t> marks,
                                     std::uint32_t before, std::uint32_t after)
{
  for (const std::uint32_t mark : marks)
    if (before < mark && after >= mark) return mark;
  return std::nullopt;
}

std::uint16_t seasonOf(const GameDateValue& date)
{
  return static_cast<std::uint16_t>(date.month >= 7 ? date.year
                                                    : date.year - 1);
}

template <typename T>
T columnAs(sqlite3_stmt* stmt, int column)
{
  return static_cast<T>(sqlite3_column_int64(stmt, column));
}

template <typename Read>
void forEachRow(const DatabaseConnection& db, const char* sql, Read read)
{
  sqlite3_stmt* stmt = db.prepareStatement(sql);
  while (sqlite3_step(stmt) == SQLITE_ROW) read(stmt);
  sqlite3_finalize(stmt);
}

template <typename Range, typename Bind>
void insertAll(const DatabaseConnection& db, const char* sql,
               const Range& items, Bind bind)
{
  sqlite3_stmt* stmt = db.prepareStatement(sql);
  for (const auto& item : items)
  {
    bind(stmt, item);
    db.executeStep(stmt);
    sqlite3_reset(stmt);
    sqlite3_clear_bindings(stmt);
  }
  sqlite3_finalize(stmt);
}

bool validKind(std::uint8_t kind)
{
  return kind <= static_cast<std::uint8_t>(StoryKind::Rivalry);
}
}  // namespace

namespace Stories
{
std::optional<std::uint32_t> appearanceMilestone(std::uint32_t before,
                                                 std::uint32_t after)
{
  return crossed(APPEARANCE_MILESTONES, before, after);
}

std::optional<std::uint32_t> goalMilestone(std::uint32_t before,
                                           std::uint32_t after)
{
  return crossed(GOAL_MILESTONES, before, after);
}

bool isPoorRun(std::string_view recent_form)
{
  if (recent_form.size() < static_cast<std::size_t>(POOR_RUN_MATCHES))
    return false;
  return recent_form.substr(0, POOR_RUN_MATCHES).find('W') ==
         std::string_view::npos;
}

const char* kindKey(StoryKind kind)
{
  switch (kind)
  {
    case StoryKind::Debut:
      return "STORY_KIND_DEBUT";
    case StoryKind::Breakout:
      return "STORY_KIND_BREAKOUT";
    case StoryKind::CaptainDispute:
      return "STORY_KIND_CAPTAIN";
    case StoryKind::PoorRun:
      return "STORY_KIND_POOR_RUN";
    case StoryKind::TransferSaga:
      return "STORY_KIND_SAGA";
    case StoryKind::Comeback:
      return "STORY_KIND_COMEBACK";
    case StoryKind::Milestone:
      return "STORY_KIND_MILESTONE";
    case StoryKind::Rivalry:
      break;
  }
  return "STORY_KIND_RIVALRY";
}
}  // namespace Stories

StoryEngine::StoryEngine(std::shared_ptr<GameData> gd) : gamedata(std::move(gd))
{
}

void StoryEngine::setCareerProvider(
    std::function<CareerTotals(PlayerID)> provider)
{
  career_provider = std::move(provider);
  career_cache.clear();
}

CareerTotals StoryEngine::careerBefore(PlayerID player_id)
{
  const auto cached = career_cache.find(player_id);
  if (cached != career_cache.end()) return cached->second;
  const CareerTotals totals =
      career_provider ? career_provider(player_id) : CareerTotals{};
  career_cache.emplace(player_id, totals);
  return totals;
}

bool StoryEngine::fired(StoryKind kind, std::uint32_t entity,
                        std::uint32_t key) const
{
  return std::ranges::any_of(state.records,
                             [&](const StoryRecord& record)
                             {
                               return record.kind == kind &&
                                      record.entity == entity &&
                                      record.key == key;
                             });
}

bool StoryEngine::firedSince(StoryKind kind, std::uint32_t entity,
                             std::int32_t since_day) const
{
  return std::ranges::any_of(state.records,
                             [&](const StoryRecord& record)
                             {
                               return record.kind == kind &&
                                      record.entity == entity &&
                                      record.day > since_day;
                             });
}

bool StoryEngine::post(const GameDateValue& date, StoryKind kind,
                       std::uint32_t entity, std::uint32_t key,
                       const char* title_key, const char* body_key,
                       std::vector<std::string> args,
                       std::optional<PlayerID> player_id,
                       std::optional<TeamID> team_id, Inbox& inbox,
                       bool with_choice)
{
  const std::int32_t today = dayOrdinal(date);
  // Decisions always get through; the rest shares a weekly allowance.
  const auto recent = std::ranges::count_if(
      state.records, [&](const StoryRecord& record)
      { return record.posted && record.day > today - 7; });
  const bool posted = with_choice || recent < Stories::WEEKLY_CAP;
  state.records.push_back(StoryRecord{kind, entity, key, today, posted});
  if (!posted) return false;
  InboxMessage message;
  message.date = date;
  message.category = kind == StoryKind::TransferSaga ? InboxCategory::Transfer
                                                     : InboxCategory::General;
  message.title_key = title_key;
  message.body_key = body_key;
  message.args = std::move(args);
  message.player_id = player_id;
  message.team_id = team_id;
  // The manager watched these happen: filed as read, like match results.
  message.read = kind == StoryKind::Debut || kind == StoryKind::Rivalry;
  inbox.add(std::move(message));
  if (with_choice && player_id)
  {
    std::erase_if(state.choices, [&](const StoryChoice& choice)
                  { return choice.player_id == *player_id; });
    state.choices.push_back(
        StoryChoice{kind, *player_id, today, today + Stories::CHOICE_DAYS});
  }
  return true;
}

void StoryEngine::onDayAdvanced(const GameDateValue& date,
                                TeamID managed_team_id,
                                const InteractionSystem& interactions,
                                Inbox& inbox)
{
  const std::int32_t today = dayOrdinal(date);
  std::erase_if(state.choices, [&](const StoryChoice& choice)
                { return choice.expires_day < today; });
  if (managed_team_id == FREE_AGENTS_TEAM_ID) return;

  // Long injuries of the managed squad: a comeback is due at the player's
  // next appearance.
  for (const auto& player : gamedata->getPlayersForTeam(managed_team_id))
  {
    const PlayerID player_id = player.get().getId();
    const bool injured = player.get().getDynamics().injury_days > 0;
    const auto start = state.injury_start.find(player_id);
    if (injured && start == state.injury_start.end())
      state.injury_start.emplace(player_id, today);
    else if (!injured && start != state.injury_start.end())
    {
      const std::int32_t days_out = today - start->second;
      if (days_out >= Stories::LONG_INJURY_DAYS)
        state.comeback_due[player_id] = days_out;
      state.injury_start.erase(start);
    }
  }
  const auto left_club = [&](const auto& entry)
  {
    const auto player = gamedata->getPlayer(entry.first);
    return !player || player->get().getTeamId() != managed_team_id;
  };
  std::erase_if(state.injury_start, left_club);
  std::erase_if(state.comeback_due, left_club);

  if (today % 7 != 0) return;
  std::erase_if(state.bids, [&](const auto& bid)
                { return today - bid.second > Stories::SAGA_WINDOW_DAYS; });
  std::erase_if(state.records, [&](const StoryRecord& record)
                { return today - record.day > RECORD_RETENTION_DAYS; });

  // Captain dispute: the dressing room's top figure is unhappy or dropped.
  const DressingRoom room = interactions.dressingRoom(managed_team_id, date);
  if (room.leaders.empty()) return;
  const PlayerID captain_id = room.leaders.front().player_id;
  const auto captain = gamedata->getPlayer(captain_id);
  if (!captain) return;
  const PlayerDynamics& dynamics = captain->get().getDynamics();
  const bool dropped = dynamics.injury_days == 0 &&
                       dynamics.season_appearances >= 1 &&
                       dynamics.playing_share < DISPUTE_SHARE;
  if ((dynamics.morale < DISPUTE_MORALE || dropped) &&
      !firedSince(StoryKind::CaptainDispute, captain_id,
                  today - DISPUTE_COOLDOWN_DAYS))
  {
    post(date, StoryKind::CaptainDispute, captain_id, 0, "STORY_CAPTAIN_TITLE",
         dropped ? "STORY_CAPTAIN_BODY_DROPPED" : "STORY_CAPTAIN_BODY_UNHAPPY",
         {captain->get().getName()}, captain_id, managed_team_id, inbox, true);
  }
}

void StoryEngine::onMatchPlayed(const MatchReport& report,
                                TeamID managed_team_id,
                                const std::string& recent_form, Inbox& inbox)
{
  const bool home = report.home_team_id == managed_team_id;
  if (managed_team_id == FREE_AGENTS_TEAM_ID ||
      (!home && report.away_team_id != managed_team_id))
    return;
  const bool competitive = report.match_type != MatchType::FRIENDLY;
  const GameDateValue& date = report.date;
  const std::int32_t today = dayOrdinal(date);
  const TeamID opponent_id = home ? report.away_team_id : report.home_team_id;
  const auto opponent = gamedata->getTeam(opponent_id);
  const std::string opponent_name =
      opponent ? opponent->get().getName() : std::string();

  for (const PlayerMatchLine& line : report.players)
  {
    if (line.team_id != managed_team_id || line.minutes == 0) continue;
    const auto player = gamedata->getPlayer(line.player_id);
    if (!player) continue;
    const Player& who = player->get();
    const std::string name = who.getName();

    if (const auto due = state.comeback_due.find(line.player_id);
        due != state.comeback_due.end())
    {
      post(date, StoryKind::Comeback, line.player_id,
           static_cast<std::uint32_t>(today), "STORY_COMEBACK_TITLE",
           "STORY_COMEBACK_BODY",
           {name, std::to_string(due->second / 7), opponent_name},
           line.player_id, std::nullopt, inbox);
      state.comeback_due.erase(due);
    }

    if (competitive)
    {
      const CareerTotals before = careerBefore(line.player_id);
      const CareerTotals after{before.appearances + 1,
                               before.goals + line.goals};
      career_cache[line.player_id] = after;
      // Only youngsters: a new world has no history for its veterans.
      if (before.appearances == 0 && who.getAge() <= Stories::DEBUT_MAX_AGE &&
          !fired(StoryKind::Debut, line.player_id, 0))
      {
        post(date, StoryKind::Debut, line.player_id, 0, "STORY_DEBUT_TITLE",
             line.goals > 0 ? "STORY_DEBUT_BODY_GOAL" : "STORY_DEBUT_BODY",
             {name, std::to_string(who.getAge()), opponent_name},
             line.player_id, std::nullopt, inbox);
      }
      if (const auto mark = Stories::appearanceMilestone(before.appearances,
                                                         after.appearances);
          mark && !fired(StoryKind::Milestone, line.player_id, *mark))
      {
        post(date, StoryKind::Milestone, line.player_id, *mark,
             "STORY_MILESTONE_TITLE", "STORY_MILESTONE_APPS_BODY",
             {name, std::to_string(*mark)}, line.player_id, std::nullopt,
             inbox);
      }
      if (const auto mark = Stories::goalMilestone(before.goals, after.goals);
          mark &&
          !fired(StoryKind::Milestone, line.player_id, GOAL_KEY_OFFSET + *mark))
      {
        post(date, StoryKind::Milestone, line.player_id,
             GOAL_KEY_OFFSET + *mark, "STORY_MILESTONE_TITLE",
             "STORY_MILESTONE_GOALS_BODY", {name, std::to_string(*mark)},
             line.player_id, std::nullopt, inbox);
      }
    }

    // Breakout: a young player whose recent ratings stand out.
    const std::uint16_t season = seasonOf(date);
    if (who.getAge() <= Stories::BREAKOUT_MAX_AGE &&
        who.getDynamics().rating_count >= 3 &&
        who.getForm() >= Stories::BREAKOUT_RATING &&
        !fired(StoryKind::Breakout, line.player_id, season))
    {
      post(date, StoryKind::Breakout, line.player_id, season,
           "STORY_BREAKOUT_TITLE", "STORY_BREAKOUT_BODY",
           {name, std::to_string(who.getAge())}, line.player_id, std::nullopt,
           inbox);
    }
  }

  if (!competitive) return;
  const auto team = gamedata->getTeam(managed_team_id);
  if (Stories::isPoorRun(recent_form) &&
      !firedSince(StoryKind::PoorRun, managed_team_id,
                  today - POOR_RUN_COOLDOWN_DAYS))
  {
    post(date, StoryKind::PoorRun, managed_team_id,
         static_cast<std::uint32_t>(today), "STORY_POOR_RUN_TITLE",
         "STORY_POOR_RUN_BODY",
         {team ? team->get().getName() : std::string(),
          std::to_string(Stories::POOR_RUN_MATCHES)},
         std::nullopt, managed_team_id, inbox);
  }

  if (report.match_type == MatchType::LEAGUE && rivalOf(managed_team_id) &&
      *rivalOf(managed_team_id) == opponent_id &&
      !fired(StoryKind::Rivalry, opponent_id,
             static_cast<std::uint32_t>(today)))
  {
    const int own = home ? report.home_goals : report.away_goals;
    const int other = home ? report.away_goals : report.home_goals;
    const char* body = own > other   ? "STORY_RIVALRY_BODY_WIN"
                       : own < other ? "STORY_RIVALRY_BODY_LOSS"
                                     : "STORY_RIVALRY_BODY_DRAW";
    post(date, StoryKind::Rivalry, opponent_id,
         static_cast<std::uint32_t>(today), "STORY_RIVALRY_TITLE", body,
         {opponent_name, std::to_string(own), std::to_string(other)},
         std::nullopt, opponent_id, inbox);
  }
}

void StoryEngine::onTransferBid(const GameDateValue& date, PlayerID player_id,
                                TeamID bidder_id, bool key_player,
                                TeamID managed_team_id, Inbox& inbox)
{
  const auto player = gamedata->getPlayer(player_id);
  if (!player || managed_team_id == FREE_AGENTS_TEAM_ID ||
      player->get().getTeamId() != managed_team_id)
    return;
  const std::int32_t today = dayOrdinal(date);
  state.bids.emplace_back(player_id, today);
  const auto bids = std::ranges::count_if(
      state.bids,
      [&](const auto& bid)
      {
        return bid.first == player_id &&
               today - bid.second <= Stories::SAGA_WINDOW_DAYS;
      });
  if (!key_player || bids < 2 ||
      firedSince(StoryKind::TransferSaga, player_id,
                 today - SAGA_COOLDOWN_DAYS))
    return;
  const auto bidder = gamedata->getTeam(bidder_id);
  post(date, StoryKind::TransferSaga, player_id, 0, "STORY_SAGA_TITLE",
       "STORY_SAGA_BODY",
       {player->get().getName(),
        bidder ? bidder->get().getName() : std::string(), std::to_string(bids)},
       player_id, bidder_id, inbox, true);
}

void StoryEngine::onPlayerLeft(PlayerID player_id)
{
  std::erase_if(state.choices, [&](const StoryChoice& choice)
                { return choice.player_id == player_id; });
  state.injury_start.erase(player_id);
  state.comeback_due.erase(player_id);
  std::erase_if(state.bids,
                [&](const auto& bid) { return bid.first == player_id; });
}

void StoryEngine::onSeasonStart() { rivals.clear(); }

std::optional<TeamID> StoryEngine::rivalOf(TeamID team_id) const
{
  if (const auto cached = rivals.find(team_id); cached != rivals.end())
    return cached->second;
  std::optional<TeamID> rival;
  const auto team = gamedata->getTeam(team_id);
  if (team)
  {
    int best_gap = RIVAL_REPUTATION_GAP + 1;
    const int reputation = team->get().getReputation();
    for (const auto& [other_id, other] : gamedata->getTeams())
    {
      if (other_id == team_id || other_id == FREE_AGENTS_TEAM_ID ||
          other.getLeagueId() != team->get().getLeagueId())
        continue;
      const int gap = std::abs(other.getReputation() - reputation);
      if (gap < best_gap || (gap == best_gap && rival && other_id < *rival))
      {
        best_gap = gap;
        rival = other_id;
      }
    }
  }
  rivals.emplace(team_id, rival);
  return rival;
}

std::optional<StoryChoice> StoryEngine::choiceFor(PlayerID player_id,
                                                  std::int32_t today) const
{
  for (const StoryChoice& choice : state.choices)
    if (choice.player_id == player_id && choice.expires_day >= today)
      return choice;
  return std::nullopt;
}

void StoryEngine::resolveChoice(PlayerID player_id)
{
  std::erase_if(state.choices, [&](const StoryChoice& choice)
                { return choice.player_id == player_id; });
}

// ---------------------------------------------------------------------------
// Persistence (tables StoryRecords, StoryChoices, StoryInjuries, StoryBids)
// ---------------------------------------------------------------------------

void StoryEngine::restore(StoryState restored)
{
  state = std::move(restored);
  career_cache.clear();
  rivals.clear();
}

void StoryEngine::load(const std::shared_ptr<DatabaseConnection>& db_conn)
{
  StoryState loaded;
  forEachRow(
      *db_conn,
      "SELECT kind, entity, story_key, day, posted FROM StoryRecords "
      "ORDER BY rowid;",
      [&](sqlite3_stmt* stmt)
      {
        const auto kind = columnAs<std::uint8_t>(stmt, 0);
        if (!validKind(kind)) return;
        loaded.records.push_back(StoryRecord{
            static_cast<StoryKind>(kind), columnAs<std::uint32_t>(stmt, 1),
            columnAs<std::uint32_t>(stmt, 2), columnAs<std::int32_t>(stmt, 3),
            sqlite3_column_int(stmt, 4) != 0});
      });
  forEachRow(
      *db_conn,
      "SELECT player_id, kind, day, expires_day FROM StoryChoices "
      "ORDER BY rowid;",
      [&](sqlite3_stmt* stmt)
      {
        const auto kind = columnAs<std::uint8_t>(stmt, 1);
        if (!validKind(kind)) return;
        loaded.choices.push_back(StoryChoice{
            static_cast<StoryKind>(kind), columnAs<PlayerID>(stmt, 0),
            columnAs<std::int32_t>(stmt, 2), columnAs<std::int32_t>(stmt, 3)});
      });
  forEachRow(*db_conn,
             "SELECT player_id, start_day, comeback_days FROM StoryInjuries;",
             [&](sqlite3_stmt* stmt)
             {
               const auto player_id = columnAs<PlayerID>(stmt, 0);
               const auto start = columnAs<std::int32_t>(stmt, 1);
               const auto comeback = columnAs<std::int32_t>(stmt, 2);
               if (start > 0) loaded.injury_start.emplace(player_id, start);
               if (comeback > 0)
                 loaded.comeback_due.emplace(player_id, comeback);
             });
  forEachRow(*db_conn, "SELECT player_id, day FROM StoryBids ORDER BY rowid;",
             [&](sqlite3_stmt* stmt)
             {
               loaded.bids.emplace_back(columnAs<PlayerID>(stmt, 0),
                                        columnAs<std::int32_t>(stmt, 1));
             });
  restore(std::move(loaded));
}

void StoryEngine::save(const std::shared_ptr<DatabaseConnection>& db_conn) const
{
  for (const char* table :
       {"StoryRecords", "StoryChoices", "StoryInjuries", "StoryBids"})
  {
    sqlite3_exec(db_conn->getRaw(),
                 (std::string("DELETE FROM ") + table + ";").c_str(), nullptr,
                 nullptr, nullptr);
  }
  insertAll(*db_conn,
            "INSERT INTO StoryRecords (kind, entity, story_key, day, posted) "
            "VALUES (?, ?, ?, ?, ?);",
            state.records,
            [](sqlite3_stmt* row, const StoryRecord& record)
            {
              sqlite3_bind_int(row, 1, static_cast<int>(record.kind));
              sqlite3_bind_int64(row, 2, record.entity);
              sqlite3_bind_int64(row, 3, record.key);
              sqlite3_bind_int(row, 4, record.day);
              sqlite3_bind_int(row, 5, record.posted ? 1 : 0);
            });
  insertAll(*db_conn,
            "INSERT INTO StoryChoices (player_id, kind, day, expires_day) "
            "VALUES (?, ?, ?, ?);",
            state.choices,
            [](sqlite3_stmt* row, const StoryChoice& choice)
            {
              sqlite3_bind_int(row, 1, static_cast<int>(choice.player_id));
              sqlite3_bind_int(row, 2, static_cast<int>(choice.kind));
              sqlite3_bind_int(row, 3, choice.day);
              sqlite3_bind_int(row, 4, choice.expires_day);
            });
  // One row per player: injury start and/or comeback due.
  std::unordered_map<PlayerID, std::pair<std::int32_t, std::int32_t>> injuries;
  for (const auto& [player_id, start] : state.injury_start)
    injuries[player_id].first = start;
  for (const auto& [player_id, days] : state.comeback_due)
    injuries[player_id].second = days;
  insertAll(*db_conn,
            "INSERT INTO StoryInjuries (player_id, start_day, comeback_days) "
            "VALUES (?, ?, ?);",
            injuries,
            [](sqlite3_stmt* row, const auto& entry)
            {
              sqlite3_bind_int(row, 1, static_cast<int>(entry.first));
              sqlite3_bind_int(row, 2, entry.second.first);
              sqlite3_bind_int(row, 3, entry.second.second);
            });
  insertAll(*db_conn, "INSERT INTO StoryBids (player_id, day) VALUES (?, ?);",
            state.bids,
            [](sqlite3_stmt* row, const auto& bid)
            {
              sqlite3_bind_int(row, 1, static_cast<int>(bid.first));
              sqlite3_bind_int(row, 2, bid.second);
            });
}
