// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "model/competition_manager.h"

#include <algorithm>
#include <unordered_map>
#include <utility>

#include "database/database_connection.h"
#include "database/gamedata.h"
#include "database/repositories/competition_repository.h"
#include "database/repositories/fixture_repository.h"
#include "database/repositories/league_repository.h"
#include "model/calendar.h"
#include "model/league.h"
#include "model/player.h"
#include "model/team.h"

namespace
{
void upsertHistory(std::vector<SeasonHistoryEntry>& history,
                   SeasonHistoryEntry entry)
{
  const auto existing = std::ranges::find_if(
      history,
      [&entry](const SeasonHistoryEntry& candidate)
      {
        return candidate.season == entry.season &&
               candidate.competition_type == entry.competition_type &&
               candidate.competition_id == entry.competition_id;
      });
  if (existing != history.end())
    *existing = std::move(entry);
  else
    history.push_back(std::move(entry));
}

void setTopScorer(SeasonHistoryEntry& entry,
                  const std::vector<PlayerSeasonStats>& scorers)
{
  if (scorers.empty()) return;
  entry.top_scorer_id = scorers.front().player_id;
  entry.top_scorer_goals = scorers.front().goals;
}
}  // namespace

CompetitionManager::CompetitionManager(
    std::shared_ptr<GameData> game_data,
    std::shared_ptr<DatabaseConnection> connection)
    : gamedata(std::move(game_data)), db_conn(std::move(connection))
{
}

void CompetitionManager::load(Calendar& calendar, uint16_t season)
{
  current_season = season;
  pending_reports.clear();
  dirty_leagues.clear();
  history_dirty = false;

  // Older saves lack the competition tables and columns.
  FixtureRepository(db_conn).ensureSchema();
  CompetitionRepository(db_conn).ensureSchema();

  // Fixtures of older saves carry no competition: derive it from the teams.
  for (const auto& [date, day_matches] : calendar.getFullCalendar())
  {
    for (Match& match : calendar.getMatchesForDateMutable(date))
    {
      if (match.getMatchType() != MatchType::LEAGUE ||
          match.getCompetitionId() != 0)
        continue;
      const auto home = gamedata->getTeam(match.getHomeTeamId());
      const auto away = gamedata->getTeam(match.getAwayTeamId());
      if (home && away &&
          home->get().getLeagueId() == away->get().getLeagueId())
        match.setCompetitionId(home->get().getLeagueId());
    }
  }

  CompetitionRepository repository(db_conn);
  season_history = repository.loadSeasonHistory();
  player_stats = repository.loadPlayerSeasonStats(season);
  discipline.restore(repository.loadDiscipline());
  for (const auto& [id, league] : gamedata->getLeagues())
    refreshLeaguePoints(calendar, id);
}

void CompetitionManager::save() const
{
  std::vector<MatchReport> reports;
  reports.reserve(pending_reports.size());
  for (const auto& [key, report] : pending_reports) reports.push_back(report);
  FixtureRepository(db_conn).saveMatchReports(reports);

  CompetitionRepository repository(db_conn);
  if (history_dirty) repository.saveSeasonHistory(season_history);
  repository.savePlayerSeasonStats(player_stats);
  repository.saveDiscipline(discipline.records());

  LeagueRepository league_repository(db_conn);
  for (const auto& [id, league] : gamedata->getLeagues())
    league_repository.saveTeamMemberships(league);
}

void CompetitionManager::onSaved()
{
  pending_reports.clear();
  history_dirty = false;
  std::erase_if(player_stats, [this](const auto& entry)
                { return entry.second.season < current_season; });
}

void CompetitionManager::recordResult(const Match& match, MatchReport report)
{
  match.writeResultTo(report);
  report.season = current_season;
  if (report.match_type == MatchType::LEAGUE && report.competition_id != 0)
    dirty_leagues.insert(report.competition_id);
  SeasonStats::accumulate(player_stats, report);
  discipline.processMatch(report, *gamedata);
  const FixtureKey key{report.date, report.home_team_id, report.away_team_id};
  pending_reports.insert_or_assign(key, std::move(report));
}

void CompetitionManager::afterMatchday(Calendar& calendar,
                                       const GameDateValue& today)
{
  for (const LeagueID league_id : dirty_leagues)
    refreshLeaguePoints(calendar, league_id);
  dirty_leagues.clear();
  Competitions::drawPendingCupRounds(
      calendar, *gamedata, SeasonCalendar::seasonStartYear(today), today);
}

void CompetitionManager::closeSeason(const Calendar& calendar, uint16_t season,
                                     uint16_t start_year)
{
  std::unordered_map<LeagueID, std::vector<StandingRow>> tables;
  std::vector<LeagueID> league_ids;
  for (const auto& [id, league] : gamedata->getLeagues())
  {
    tables.emplace(id, Standings::compute(league, calendar, *gamedata));
    league_ids.push_back(id);
  }
  std::ranges::sort(league_ids);
  const auto movements = Competitions::computeLeagueMovements(*gamedata, tables);

  for (const LeagueID league_id : league_ids)
  {
    const League& league = gamedata->getLeagues().at(league_id);
    const auto& rows = tables.at(league_id);
    SeasonHistoryEntry entry;
    entry.season = season;
    entry.start_year = start_year;
    entry.competition_type = MatchType::LEAGUE;
    entry.competition_id = league_id;
    entry.competition_name = league.getName();
    if (!rows.empty() && rows.front().played > 0)
    {
      entry.champion_id = rows.front().team_id;
      if (rows.size() > 1) entry.runner_up_id = rows[1].team_id;
    }
    for (const auto& movement : movements)
    {
      if (movement.from != league_id) continue;
      if (league.getParentLeagueID() == movement.to)
        entry.promoted.push_back(movement.team_id);
      else
        entry.relegated.push_back(movement.team_id);
    }
    setTopScorer(entry, SeasonStats::topScorers(player_stats, season,
                                                MatchType::LEAGUE,
                                                league.getTeamIDs(), 1));
    upsertHistory(season_history, std::move(entry));
  }

  for (const LeagueID root : Competitions::countryRoots(*gamedata))
  {
    const auto status = Competitions::cupStatus(calendar, *gamedata, root);
    if (status.rounds.empty()) continue;
    SeasonHistoryEntry entry;
    entry.season = season;
    entry.start_year = start_year;
    entry.competition_type = MatchType::CUP;
    entry.competition_id = root;
    entry.competition_name = status.name;
    entry.champion_id = status.winner.value_or(0);
    entry.runner_up_id = status.runner_up.value_or(0);
    setTopScorer(entry,
                 SeasonStats::topScorers(player_stats, season, MatchType::CUP,
                                         Competitions::cupEntrants(*gamedata, root),
                                         1));
    upsertHistory(season_history, std::move(entry));
  }

  Competitions::applyLeagueMovements(*gamedata, movements);
  discipline.resetSeason();
  history_dirty = true;
}

std::vector<StandingRow> CompetitionManager::getStandings(
    const Calendar& calendar, LeagueID league_id) const
{
  const auto league = gamedata->getLeague(league_id);
  if (!league) return {};
  return Standings::compute(league->get(), calendar, *gamedata);
}

std::vector<LeagueID> CompetitionManager::getCupIds() const
{
  return Competitions::countryRoots(*gamedata);
}

std::optional<Competitions::CupStatus> CompetitionManager::getCupStatus(
    const Calendar& calendar, LeagueID cup_id) const
{
  if (!gamedata->getLeague(cup_id) ||
      Competitions::rootLeague(*gamedata, cup_id) != cup_id)
    return std::nullopt;
  return Competitions::cupStatus(calendar, *gamedata, cup_id);
}

std::optional<MatchReport> CompetitionManager::getMatchReport(
    const GameDateValue& date, TeamID home_id, TeamID away_id) const
{
  const auto pending = pending_reports.find(FixtureKey{date, home_id, away_id});
  if (pending != pending_reports.end()) return pending->second;
  return FixtureRepository(db_conn).loadMatchReport(date, home_id, away_id);
}

std::vector<TeamID> CompetitionManager::competitionTeams(
    MatchType competition_type, LeagueID competition_id) const
{
  if (competition_type == MatchType::CUP)
    return Competitions::cupEntrants(*gamedata, competition_id);
  const auto league = gamedata->getLeague(competition_id);
  return league ? league->get().getTeamIDs() : std::vector<TeamID>{};
}

std::vector<PlayerSeasonStats> CompetitionManager::getTopScorers(
    MatchType competition_type, LeagueID competition_id, size_t limit) const
{
  return SeasonStats::topScorers(player_stats, current_season, competition_type,
                                 competitionTeams(competition_type,
                                                  competition_id),
                                 limit);
}

std::vector<PlayerSeasonStats> CompetitionManager::getPlayerSeasonStats(
    PlayerID player_id) const
{
  std::vector<PlayerSeasonStats> rows;
  for (const auto& [key, stats] : player_stats)
    if (stats.player_id == player_id && stats.season == current_season)
      rows.push_back(stats);
  return rows;
}

std::vector<PlayerSeasonStats> CompetitionManager::getPlayerCareer(
    PlayerID player_id) const
{
  PlayerSeasonTable merged;
  for (const PlayerSeasonStats& stats :
       CompetitionRepository(db_conn).loadPlayerCareer(player_id))
    merged.insert_or_assign(PlayerSeasonKey{stats.season, stats.player_id,
                                            stats.team_id,
                                            stats.competition_type},
                            stats);
  for (const auto& [key, stats] : player_stats)
    if (stats.player_id == player_id) merged.insert_or_assign(key, stats);
  std::vector<PlayerSeasonStats> career;
  career.reserve(merged.size());
  for (const auto& [key, stats] : merged) career.push_back(stats);
  return career;
}

void CompetitionManager::refreshLeaguePoints(const Calendar& calendar,
                                             LeagueID league_id)
{
  auto& leagues = gamedata->getLeagues();
  const auto league = leagues.find(league_id);
  if (league == leagues.end()) return;
  for (const StandingRow& row :
       Standings::compute(league->second, calendar, *gamedata))
    league->second.setPoints(row.team_id, row.points);
}

std::vector<CompetitionManager::LineupSwap>
CompetitionManager::benchSuspendedPlayers(const Match& match)
{
  std::vector<LineupSwap> swaps;
  if (match.getMatchType() == MatchType::FRIENDLY) return swaps;
  const MatchType scope = match.getMatchType();
  const auto eligible = [&](const Player* player)
  {
    return player && player->isAvailable() &&
           !discipline.isSuspended(player->getId(), scope);
  };

  for (const TeamID team_id : {match.getHomeTeamId(), match.getAwayTeamId()})
  {
    const auto team = gamedata->getTeam(team_id);
    if (!team) continue;
    Lineup& lineup = team->get().getLineup();
    std::vector<const Player*> starters;
    if (lineup.getGoalkeeper()) starters.push_back(lineup.getGoalkeeper());
    for (const auto& positioned : lineup.getOutfieldPlayers())
      if (positioned.player) starters.push_back(positioned.player);

    for (const Player* starter : starters)
    {
      if (!discipline.isSuspended(starter->getId(), scope)) continue;
      const bool keeper = starter->getRole() == PlayerRole::GK;
      const auto& reserves = lineup.getReserves();
      auto replacement = std::ranges::find_if(
          reserves, [&](const Player* reserve)
          {
            return eligible(reserve) && reserve->getRole() == starter->getRole();
          });
      if (replacement == reserves.end())
        replacement = std::ranges::find_if(
            reserves, [&](const Player* reserve)
            {
              return eligible(reserve) &&
                     (reserve->getRole() == PlayerRole::GK) == keeper;
            });
      if (replacement == reserves.end()) continue;
      const PlayerID replacement_id = (*replacement)->getId();
      if (lineup.swapPlayers(replacement_id, starter->getId()))
        swaps.push_back({team_id, starter->getId(), replacement_id});
    }
  }
  return swaps;
}

void CompetitionManager::restoreLineups(const std::vector<LineupSwap>& swaps)
{
  for (auto swap = swaps.rbegin(); swap != swaps.rend(); ++swap)
  {
    if (const auto team = gamedata->getTeam(swap->team_id))
      team->get().getLineup().swapPlayers(swap->suspended_id,
                                          swap->replacement_id);
  }
}
