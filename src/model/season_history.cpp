// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "model/season_history.h"

#include <algorithm>
#include <unordered_map>

#include "model/match_report.h"

namespace
{
PlayerSeasonStats& entryFor(PlayerSeasonTable& table, const MatchReport& report,
                            PlayerID player_id, TeamID team_id)
{
  const PlayerSeasonKey key{report.season, player_id, team_id,
                            report.match_type};
  auto [it, inserted] = table.try_emplace(key);
  if (inserted)
  {
    it->second.season = report.season;
    it->second.player_id = player_id;
    it->second.team_id = team_id;
    it->second.competition_type = report.match_type;
  }
  return it->second;
}

void increment(uint16_t& counter, unsigned amount)
{
  counter = static_cast<uint16_t>(std::min(counter + amount, 65'535U));
}
}  // namespace

void SeasonStats::accumulate(PlayerSeasonTable& table,
                             const MatchReport& report)
{
  if (report.match_type == MatchType::FRIENDLY) return;

  const bool lines_have_events = std::ranges::any_of(
      report.players,
      [](const PlayerMatchLine& line)
      {
        return line.goals > 0 || line.assists > 0 || line.yellow_cards > 0 ||
               line.red_cards > 0;
      });

  for (const PlayerMatchLine& line : report.players)
  {
    PlayerSeasonStats& stats =
        entryFor(table, report, line.player_id, line.team_id);
    increment(stats.appearances, 1);
    if (line.started) increment(stats.starts, 1);
    increment(stats.minutes, line.minutes);
    increment(stats.goals, line.goals);
    increment(stats.assists, line.assists);
    increment(stats.yellow_cards, line.yellow_cards);
    increment(stats.red_cards, line.red_cards);
    if (line.rating > 0.0f)
    {
      stats.rating_total += line.rating;
      increment(stats.rated_matches, 1);
    }
  }
  if (lines_have_events) return;

  // Fall back to attributed events (e.g. engines without per-player lines).
  const auto teamOf = [&report](const MatchReportEvent& event)
  { return event.home ? report.home_team_id : report.away_team_id; };
  for (const MatchReportEvent& event : report.events)
  {
    if (event.player == 0) continue;
    const bool listed = std::ranges::any_of(
        report.players, [&event](const PlayerMatchLine& line)
        { return line.player_id == event.player; });
    PlayerSeasonStats& stats =
        entryFor(table, report, event.player, teamOf(event));
    if (!listed && stats.appearances == 0) increment(stats.appearances, 1);
    switch (event.kind)
    {
      case MatchEventKind::GOAL:
        increment(stats.goals, 1);
        if (event.assist != 0)
          increment(entryFor(table, report, event.assist, teamOf(event)).assists,
                    1);
        break;
      case MatchEventKind::YELLOW_CARD:
        increment(stats.yellow_cards, 1);
        break;
      case MatchEventKind::RED_CARD:
        increment(stats.red_cards, 1);
        break;
      case MatchEventKind::SECOND_YELLOW:
        increment(stats.yellow_cards, 1);
        increment(stats.red_cards, 1);
        break;
      case MatchEventKind::OWN_GOAL:
        break;
    }
  }
}

std::vector<PlayerSeasonStats> SeasonStats::topScorers(
    const PlayerSeasonTable& table, uint16_t season,
    MatchType competition_type, const std::vector<TeamID>& team_ids,
    size_t limit)
{
  std::unordered_map<PlayerID, PlayerSeasonStats> totals;
  for (const auto& [key, stats] : table)
  {
    if (stats.season != season || stats.competition_type != competition_type ||
        !std::ranges::contains(team_ids, stats.team_id))
      continue;
    auto [it, inserted] = totals.try_emplace(stats.player_id, stats);
    if (inserted) continue;
    PlayerSeasonStats& total = it->second;
    total.team_id = stats.team_id;
    increment(total.appearances, stats.appearances);
    increment(total.starts, stats.starts);
    increment(total.minutes, stats.minutes);
    increment(total.goals, stats.goals);
    increment(total.assists, stats.assists);
    increment(total.yellow_cards, stats.yellow_cards);
    increment(total.red_cards, stats.red_cards);
    total.rating_total += stats.rating_total;
    increment(total.rated_matches, stats.rated_matches);
  }

  std::vector<PlayerSeasonStats> scorers;
  scorers.reserve(totals.size());
  for (const auto& [player_id, stats] : totals)
    if (stats.goals > 0) scorers.push_back(stats);
  std::ranges::sort(scorers,
                    [](const PlayerSeasonStats& left,
                       const PlayerSeasonStats& right)
                    {
                      if (left.goals != right.goals)
                        return left.goals > right.goals;
                      if (left.assists != right.assists)
                        return left.assists > right.assists;
                      if (left.minutes != right.minutes)
                        return left.minutes < right.minutes;
                      return left.player_id < right.player_id;
                    });
  if (scorers.size() > limit) scorers.resize(limit);
  return scorers;
}
