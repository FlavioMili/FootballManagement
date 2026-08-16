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
#include "model/league.h"
#include "model/role_utils.h"
#include "model/team.h"

Game::Game(std::shared_ptr<GameData> gd,
           std::shared_ptr<DatabaseConnection> conn)
    : db_conn(std::move(conn)), gamedata(std::move(gd)), currentDate(START_DATE)
{
  (*gamedata).loadFromDB(db_conn);
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

  Logger::debug("Game saved.");
}

void Game::advanceDay()
{
  currentDate.nextDay();
  Logger::debug("Date changed to: " + currentDate.toString());

  if (currentDate.month == 7 && currentDate.day == 1)
  {
    handleSeasonTransition();
    return;
  }

  auto& matches_today = calendar.getMatchesForDateMutable(currentDate);
  if (!matches_today.empty())
  {
    simulateMatches(matches_today);
  }
}

void Game::simulateMatches(std::vector<Match>& matches)
{
  for (auto& match : matches)
  {
    if (match.isPlayed() || match.getHomeTeamId() == managed_team_id ||
        match.getAwayTeamId() == managed_team_id)
    {
      continue;
    }

    match.simulate((*gamedata));

    auto home_team_opt = (*gamedata).getTeam(match.getHomeTeamId());
    auto away_team_opt = (*gamedata).getTeam(match.getAwayTeamId());

    if (home_team_opt && away_team_opt)
    {
      Team& home_team = home_team_opt->get();
      Team& away_team = away_team_opt->get();

      updateStandings(match);

      trainPlayers(home_team.getPlayerIDs());
      trainPlayers(away_team.getPlayerIDs());

      if (home_team.getId() == managed_team_id ||
          away_team.getId() == managed_team_id)
      {
        std::cout << home_team.getName() << " "
                  << static_cast<int>(match.getHomeScore()) << " - "
                  << static_cast<int>(match.getAwayScore()) << " "
                  << away_team.getName() << "\n";
      }
    }
  }
}

bool Game::setMatchResult(const GameDateValue& date, TeamID home_id,
                          TeamID away_id, uint8_t home_score,
                          uint8_t away_score)
{
  auto& matches = calendar.getMatchesForDateMutable(date);
  const auto match_it = std::find_if(
      matches.begin(), matches.end(),
      [&](const Match& match)
      {
        return !match.isPlayed() && match.getHomeTeamId() == home_id &&
               match.getAwayTeamId() == away_id;
      });
  if (match_it == matches.end())
  {
    return false;
  }

  match_it->setPlayedResult(home_score, away_score);
  updateStandings(*match_it);

  if (const auto home_team = gamedata->getTeam(home_id))
  {
    trainPlayers(home_team->get().getPlayerIDs());
  }
  if (const auto away_team = gamedata->getTeam(away_id))
  {
    trainPlayers(away_team->get().getPlayerIDs());
  }
  return true;
}

void Game::updateStandings(const Match& match)
{
  if (match.getMatchType() != MatchType::LEAGUE)
  {
    return;
  }
  Logger::debug("Updating standings for league match.");

  auto home_team_opt = (*gamedata).getTeam(match.getHomeTeamId());
  if (!home_team_opt) return;
  Team& home_team = home_team_opt->get();

  auto away_team_opt = (*gamedata).getTeam(match.getAwayTeamId());
  if (!away_team_opt) return;
  Team& away_team = away_team_opt->get();

  if (home_team.getLeagueId() != away_team.getLeagueId()) return;
  const auto league_it = gamedata->getLeagues().find(home_team.getLeagueId());
  if (league_it == gamedata->getLeagues().end()) return;
  League& league = league_it->second;

  if (match.getHomeScore() > match.getAwayScore())
  {
    league.addPoints(home_team.getId(), 3);
  }
  else if (match.getHomeScore() < match.getAwayScore())
  {
    league.addPoints(away_team.getId(), 3);
  }
  else
  {
    league.addPoints(home_team.getId(), 1);
    league.addPoints(away_team.getId(), 1);
  }
}

void Game::endSeason()
{
  std::cout << "--- Season " << static_cast<int>(current_season)
            << " has concluded. ---"
            << "\n";
  (*gamedata).ageAllPlayers();
  (*gamedata).advanceContractsAndReleasePlayers();
  current_season++;
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
}

const GameDateValue& Game::getCurrentDate() const { return currentDate; }

const Calendar& Game::getCalendar() const { return calendar; }

Calendar& Game::getCalendar() { return calendar; }

int Game::getCurrentSeason() const { return current_season; }

uint16_t Game::getManagedTeamId() const { return managed_team_id; }

void Game::setManagedTeamId(uint16_t id) { managed_team_id = id; }

void Game::trainPlayers(const std::vector<uint32_t>& player_ids)
{
  const auto& stats_config = (*gamedata).getStatsConfig();
  for (const auto player_id : player_ids)
  {
    const auto player_it = gamedata->getPlayers().find(player_id);
    if (player_it == gamedata->getPlayers().end()) continue;
    Player& player = player_it->second;
    const auto focus = stats_config.role_focus.find(
        RoleUtils::getBroadCategory(player.getRole()));
    if (focus != stats_config.role_focus.end())
      player.train(focus->second.stats);
  }
}
