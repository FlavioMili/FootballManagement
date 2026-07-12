// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "model/game.h"

#include <iostream>

#include "database/database_connection.h"
#include "database/gamedata.h"
#include "database/repositories/fixture_repository.h"
#include "database/repositories/game_state_repository.h"
#include "database/repositories/league_repository.h"
#include "global/global.h"
#include "global/logger.h"
#include "global/paths.h"
#include "model/league.h"
#include "model/team.h"

Game::Game() : currentDate(START_DATE)
{
  db_conn = std::make_shared<DatabaseConnection>(DATABASE_PATH);
  GameData::instance().loadFromDB(db_conn);
  loadGame();
}

void Game::loadGame()
{
  GameStateRepository gameStateRepo(db_conn);
  FixtureRepository fixtureRepo(db_conn);
  std::string game_date_str;
  if (gameStateRepo.loadGameState(current_season, managed_team_id,
                                  game_date_str))
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
    calendar.generate(currentDate);
    Logger::debug("First run, initializing game state.");
  }
  // Ensure managed team is valid
  if (GameData::instance().getTeams().find(managed_team_id) ==
      GameData::instance().getTeams().end())
  {
    managed_team_id = FREE_AGENTS_TEAM_ID;
  }
}

void Game::saveGame()
{
  db_conn->beginTransaction();
  try {
    GameStateRepository gameStateRepo(db_conn);
    FixtureRepository fixtureRepo(db_conn);
    LeagueRepository leagueRepo(db_conn);

    gameStateRepo.updateGameState(current_season, managed_team_id,
                                  currentDate.toString());
    fixtureRepo.saveCalendar(calendar);

    for (const auto& pair : GameData::instance().getLeagues())
    {
      leagueRepo.saveLeaguePoints(pair.second);
    }
    db_conn->commitTransaction();
  } catch (const std::exception& e) {
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

  const auto& matches_today = calendar.getMatchesForDate(currentDate);
  if (!matches_today.empty())
  {
    auto matches_to_simulate = matches_today;
    simulateMatches(matches_to_simulate);
  }
}

void Game::simulateMatches(std::vector<Match>& matches)
{
  for (auto& match : matches)
  {
    match.simulate(GameData::instance());

    auto home_team_opt = GameData::instance().getTeam(match.getHomeTeamId());
    auto away_team_opt = GameData::instance().getTeam(match.getAwayTeamId());

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

void Game::updateStandings(const Match& match)
{
  if (match.getMatchType() != MatchType::LEAGUE)
  {
    return;
  }
  Logger::debug("Updating standings for league match.");

  auto home_team_opt = GameData::instance().getTeam(match.getHomeTeamId());
  if (!home_team_opt) return;
  Team& home_team = home_team_opt->get();

  auto away_team_opt = GameData::instance().getTeam(match.getAwayTeamId());
  if (!away_team_opt) return;
  Team& away_team = away_team_opt->get();

  League& league =
      GameData::instance().getLeagues().at(home_team.getLeagueId());

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
  GameData::instance().ageAllPlayers();
  current_season++;
}

void Game::handleSeasonTransition()
{
  endSeason();
  startNewSeason();
}

void Game::startNewSeason()
{
  for (auto& pair : GameData::instance().getLeagues())
  {
    pair.second.resetPoints();
  }
  calendar.generate(currentDate);
}

const GameDateValue& Game::getCurrentDate() const { return currentDate; }

const Calendar& Game::getCalendar() const { return calendar; }

int Game::getCurrentSeason() const { return current_season; }

uint16_t Game::getManagedTeamId() const { return managed_team_id; }

void Game::setManagedTeamId(uint16_t id) { managed_team_id = id; }

void Game::trainPlayers(const std::vector<uint32_t>& player_ids)
{
  auto stats_config = GameData::instance().getStatsConfig();
  for (auto& player_id : player_ids)
  {
    Player& player = GameData::instance().getPlayers().at(player_id);
    const auto& focus_stats =
        stats_config.role_focus.at(player.getRole()).stats;
    player.train(focus_stats);
  }
}
