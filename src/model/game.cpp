// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "model/game.h"

#include <algorithm>
#include <iostream>

#include "database/database_connection.h"
#include "database/gamedata.h"
#include "database/repositories/fixture_repository.h"
#include "database/repositories/game_state_repository.h"
#include "database/repositories/league_repository.h"
#include "database/repositories/player_repository.h"
#include "database/repositories/team_repository.h"
#include "global/global.h"
#include "global/logger.h"
#include "global/paths.h"
#include "model/competition.h"
#include "model/league.h"
#include "model/role_utils.h"
#include "model/team.h"
#include "model/world_rng.h"

Game::Game(std::shared_ptr<GameData> gd,
           std::shared_ptr<DatabaseConnection> conn)
    : db_conn(std::move(conn)),
      gamedata(std::move(gd)),
      competitions(gamedata, db_conn),
      world(gamedata),
      transfers(gamedata, world, competitions),
      currentDate(START_DATE)
{
  (*gamedata).loadFromDB(db_conn);
  world.setStandingsProvider(
      [this](LeagueID league_id)
      {
        std::vector<TeamID> order;
        for (const StandingRow& row :
             competitions.getStandings(calendar, league_id))
          order.push_back(row.team_id);
        return order;
      });
  world.setFixtureOutlookProvider(
      [this](const GameDateValue& date, TrainingSystem::FixtureOutlook& outlook)
      {
        const auto& fixtures = calendar.getFullCalendar();
        const GameDateValue horizon = SeasonCalendar::addDays(date, 7);
        for (auto day = fixtures.lower_bound(date);
             day != fixtures.end() && !(horizon < day->first); ++day)
        {
          const auto days = static_cast<std::uint8_t>(
              dayOrdinal(day->first) - dayOrdinal(date));
          for (const Match& match : day->second)
          {
            if (match.isPlayed()) continue;
            outlook.try_emplace(match.getHomeTeamId(), days);
            outlook.try_emplace(match.getAwayTeamId(), days);
          }
        }
      });
  loadGame();
}

void Game::loadGame()
{
  GameStateRepository gameStateRepo(db_conn);
  FixtureRepository fixtureRepo(db_conn);
  if (std::string game_date_str; gameStateRepo.loadGameState(
          current_season, managed_team_id, game_date_str))
  {
    currentDate = GameDateValue::fromString(game_date_str);
    fixtureRepo.loadCalendar(calendar);
    competitions.load(calendar, current_season);
    world.load(db_conn);
    transfers.load(db_conn);
    Logger::debug("Game loaded. Date: " + game_date_str +
                  ", Season: " + std::to_string(current_season));
  }
  else
  {
    // First run, initialize with defaults
    current_season = 1;
    managed_team_id = FREE_AGENTS_TEAM_ID;  // Or some other default
    currentDate = START_DATE;
    calendar.generate((*gamedata), currentDate);
    competitions.load(calendar, current_season);
    Logger::debug("First run, initializing game state.");
    saveGame();
  }
  // Ensure managed team is valid
  if ((*gamedata).getTeams().find(managed_team_id) ==
      (*gamedata).getTeams().end())
  {
    managed_team_id = FREE_AGENTS_TEAM_ID;
  }
}

void Game::saveGame()
{
  db_conn->beginTransaction();
  try
  {
    GameStateRepository gameStateRepo(db_conn);
    FixtureRepository fixtureRepo(db_conn);
    LeagueRepository leagueRepo(db_conn);
    PlayerRepository playerRepo(db_conn);
    TeamRepository teamRepo(db_conn);

    gameStateRepo.updateGameState(current_season, managed_team_id,
                                  currentDate.toString());
    fixtureRepo.saveCalendar(calendar);
    competitions.save();
    world.save(db_conn);
    transfers.save(db_conn);

    for (const auto& [id, league] : (*gamedata).getLeagues())
    {
      leagueRepo.saveLeaguePoints(league);
    }
    playerRepo.updatePlayers(gamedata->getPlayersVector());
    teamRepo.updateTeamsState(gamedata->getTeamsVector());
    db_conn->commitTransaction();
  }
  catch (const std::exception& e)
  {
    db_conn->rollbackTransaction();
    Logger::error("Failed to save game: " + std::string(e.what()));
    throw;
  }
  competitions.onSaved();
  world.onSaved();
  transfers.onSaved();

  Logger::debug("Game saved.");
}

void Game::advanceDay()
{
  currentDate.nextDay();
  Logger::debug("Date changed to: " + currentDate.toString());
  world.onDayAdvanced(currentDate, managed_team_id);

  if (currentDate.month == 7 && currentDate.day == 1)
  {
    handleSeasonTransition();
    // Pre-contracts complete once expired contracts have been released.
    transfers.onDayAdvanced(currentDate, managed_team_id);
    return;
  }

  // A managed fixture left unplayed on its day is simulated so that the
  // competitions (tables, cup draws) never stall.
  const GameDateValue yesterday = SeasonCalendar::addDays(currentDate, -1);
  if (calendar.getFullCalendar().contains(yesterday))
  {
    simulateMatches(calendar.getMatchesForDateMutable(yesterday), true);
  }

  auto& matches_today = calendar.getMatchesForDateMutable(currentDate);
  if (!matches_today.empty())
  {
    simulateMatches(matches_today);
  }
  competitions.afterMatchday(calendar, currentDate);
  transfers.onDayAdvanced(currentDate, managed_team_id);
}

void Game::simulateMatches(std::vector<Match>& matches, bool include_managed)
{
  for (auto& match : matches)
  {
    const bool managed = match.getHomeTeamId() == managed_team_id ||
                         match.getAwayTeamId() == managed_team_id;
    if (match.isPlayed() || (managed && !include_managed))
    {
      continue;
    }

    MatchReport report;
    const auto swaps = competitions.benchSuspendedPlayers(match);
    match.simulate((*gamedata), &report);
    competitions.restoreLineups(swaps);
    if (!match.isPlayed()) continue;
    world.onMatchPlayed(match, report, managed_team_id);
    competitions.recordResult(match, std::move(report));
  }
}

bool Game::setMatchResult(const GameDateValue& date, TeamID home_id,
                          TeamID away_id, uint8_t home_score,
                          uint8_t away_score)
{
  MatchReport report;
  report.home_goals = home_score;
  report.away_goals = away_score;
  return setMatchResult(date, home_id, away_id, std::move(report));
}

bool Game::setMatchResult(const GameDateValue& date, TeamID home_id,
                          TeamID away_id, MatchReport report)
{
  Match* match = calendar.findMatch(date, home_id, away_id);
  if (!match || match->isPlayed())
  {
    return false;
  }
  const auto home_team = gamedata->getTeam(home_id);
  const auto away_team = gamedata->getTeam(away_id);

  uint8_t home_goals = report.home_goals;
  uint8_t away_goals = report.away_goals;
  bool extra_time = report.extra_time;
  std::optional<std::pair<uint8_t, uint8_t>> shootout;
  if (report.penalties)
    shootout.emplace(report.home_penalties, report.away_penalties);
  if (match->isKnockout() && home_goals == away_goals && !shootout &&
      home_team && away_team)
  {
    const auto resolution = Competitions::resolveDrawnKnockout(
        home_team->get(), away_team->get(), gamedata->getStatsConfig(),
        match->getSeed());
    extra_time = true;
    home_goals = static_cast<uint8_t>(home_goals + resolution.home_extra_goals);
    away_goals = static_cast<uint8_t>(away_goals + resolution.away_extra_goals);
    if (resolution.penalties)
      shootout.emplace(resolution.home_penalties, resolution.away_penalties);
  }
  if (extra_time || shootout)
    match->setKnockoutResult(home_goals, away_goals, extra_time, shootout);
  else
    match->setPlayedResult(home_goals, away_goals);
  match->writeResultTo(report);

  if (report.players.empty())
  {
    if (home_team)
      report.addLineupAppearances(home_team->get().getLineup(), home_id);
    if (away_team)
      report.addLineupAppearances(away_team->get().getLineup(), away_id);
  }
  world.onMatchPlayed(*match, report, managed_team_id);
  competitions.recordResult(*match, std::move(report));
  competitions.afterMatchday(calendar, currentDate);
  return true;
}

void Game::endSeason()
{
  std::cout << "--- Season " << static_cast<int>(current_season)
            << " has concluded. ---"
            << "\n";
  world.onSeasonEnd(currentDate, managed_team_id);
  competitions.closeSeason(
      calendar, current_season,
      SeasonCalendar::seasonStartYear(SeasonCalendar::addDays(currentDate, -1)));
  (*gamedata).ageAllPlayers();
  (*gamedata).advanceContractsAndReleasePlayers();
  current_season++;
  competitions.setCurrentSeason(current_season);
}

void Game::handleSeasonTransition()
{
  endSeason();
  startNewSeason();
}

void Game::startNewSeason()
{
  for (auto& [id, league] : (*gamedata).getLeagues())
  {
    league.resetPoints();
  }
  calendar.generate((*gamedata), currentDate);
  world.onSeasonStart(currentDate, managed_team_id);
}

const GameDateValue& Game::getCurrentDate() const { return currentDate; }

const Calendar& Game::getCalendar() const { return calendar; }

Calendar& Game::getCalendar() { return calendar; }

int Game::getCurrentSeason() const { return current_season; }

uint16_t Game::getManagedTeamId() const { return managed_team_id; }

void Game::setManagedTeamId(uint16_t id) { managed_team_id = id; }
