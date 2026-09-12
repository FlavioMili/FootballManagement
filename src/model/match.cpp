// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "match.h"

#include <algorithm>
#include <cstdint>

#include "database/gamedata.h"
#include "lineup.h"
#include "model/competition.h"
#include "model/match_engine.h"
#include "model/match_report.h"
#include "player.h"

Match::Match(TeamID home_id, TeamID away_id, GameDateValue date, MatchType type,
             LeagueID competition, uint8_t round_stage)
    : home_team_id(home_id),
      away_team_id(away_id),
      match_date(date),
      match_type(type),
      home_score(0),
      away_score(0),
      competition_id(competition),
      stage(round_stage)
{
}

uint32_t Match::getSeed() const
{
  return (static_cast<uint32_t>(home_team_id) << 16U) ^
         static_cast<uint32_t>(away_team_id) ^
         (static_cast<uint32_t>(match_date.year) << 8U) ^
         (static_cast<uint32_t>(match_date.month) << 4U) ^ match_date.day;
}

void Match::simulate(const GameData& game_data, MatchReport* report)
{
  if (_played)
  {
    return;
  }

  auto home_team_opt = game_data.getTeam(home_team_id);
  auto away_team_opt = game_data.getTeam(away_team_id);
  if (!home_team_opt || !away_team_opt)
  {
    return;  // Or handle error appropriately
  }
  const Team& home_team = home_team_opt->get();
  const Team& away_team = away_team_opt->get();
  MatchEngine engine(home_team.getLineup(), away_team.getLineup(),
                     home_team.getStrategy(), away_team.getStrategy(),
                     game_data.getStatsConfig(), getSeed());
  while (engine.getState() != MatchState::FULL_TIME)
  {
    engine.update(0.25f);
  }

  home_score = static_cast<uint8_t>(std::clamp(engine.getHomeScore(), 0, 255));
  away_score = static_cast<uint8_t>(std::clamp(engine.getAwayScore(), 0, 255));
  _played = true;
  if (isKnockout() && home_score == away_score)
  {
    const Competitions::KnockoutResolution resolution =
        Competitions::resolveDrawnKnockout(
            home_team, away_team, game_data.getStatsConfig(), getSeed());
    applyKnockoutResolution(resolution);
  }

  if (report)
  {
    report->fillFromEngine(engine, home_team_id, away_team_id);
    if (report->players.empty())
    {
      report->addLineupAppearances(home_team.getLineup(), home_team_id);
      report->addLineupAppearances(away_team.getLineup(), away_team_id);
    }
    writeResultTo(*report);
  }
}

void Match::applyKnockoutResolution(
    const Competitions::KnockoutResolution& resolution)
{
  std::optional<std::pair<uint8_t, uint8_t>> shootout;
  if (resolution.penalties)
    shootout.emplace(resolution.home_penalties, resolution.away_penalties);
  setKnockoutResult(
      static_cast<uint8_t>(home_score + resolution.home_extra_goals),
      static_cast<uint8_t>(away_score + resolution.away_extra_goals), true,
      shootout);
}

void Match::setPlayedResult(uint8_t h, uint8_t a)
{
  home_score = h;
  away_score = a;
  _played = true;
}

void Match::setKnockoutResult(
    uint8_t h, uint8_t a, bool went_to_extra_time,
    std::optional<std::pair<uint8_t, uint8_t>> shootout)
{
  setPlayedResult(h, a);
  extra_time = went_to_extra_time;
  penalties = shootout.has_value();
  home_penalties = shootout ? shootout->first : 0;
  away_penalties = shootout ? shootout->second : 0;
}

std::optional<TeamID> Match::getWinnerId() const
{
  if (!_played) return std::nullopt;
  if (home_score != away_score)
    return home_score > away_score ? home_team_id : away_team_id;
  if (penalties && home_penalties != away_penalties)
    return home_penalties > away_penalties ? home_team_id : away_team_id;
  return std::nullopt;
}

void Match::writeResultTo(MatchReport& report) const
{
  constexpr uint8_t EXTRA_TIME_MINUTES = 120;
  report.date = match_date;
  report.home_team_id = home_team_id;
  report.away_team_id = away_team_id;
  report.match_type = match_type;
  report.competition_id = competition_id;
  report.stage = stage;
  report.home_goals = home_score;
  report.away_goals = away_score;
  report.extra_time = extra_time;
  report.penalties = penalties;
  report.home_penalties = home_penalties;
  report.away_penalties = away_penalties;
  if (extra_time)
  {
    for (PlayerMatchLine& line : report.players)
      if (line.started) line.minutes = EXTRA_TIME_MINUTES;
  }
}

uint16_t Match::getHomeTeamId() const { return home_team_id; }
uint16_t Match::getAwayTeamId() const { return away_team_id; }
uint8_t Match::getHomeScore() const { return home_score; }
uint8_t Match::getAwayScore() const { return away_score; }
MatchType Match::getMatchType() const { return match_type; }
const GameDateValue& Match::getDate() const { return match_date; }

bool Match::isPlayed() const { return _played; }
