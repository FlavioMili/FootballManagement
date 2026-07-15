// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "controller/game_controller.h"

#include <SDL3/SDL.h>
#include <sqlite3.h>

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>

#include "database/gamedata.h"
#include "global/global.h"

GameController::GameController() : game(nullptr), gamedata(nullptr) {}

std::string GameController::getSavePath(int slot) const
{
  char* prefPath = SDL_GetPrefPath("FlavioMili", "FootballManagement");
  if (!prefPath)
  {
    return "save_" + std::to_string(slot) + ".db";
  }
  std::filesystem::path p(prefPath);
  SDL_free(prefPath);

  std::filesystem::create_directories(p);
  p /= ("save_" + std::to_string(slot) + ".db");
  return p.string();
}

void GameController::newGame(int slot)
{
  std::string path = getSavePath(slot);
  if (std::filesystem::exists(path))
  {
    std::filesystem::remove(path);
  }
  gamedata = std::make_shared<GameData>();
  game = std::make_unique<Game>(gamedata, path);
}

bool GameController::loadGame(int slot)
{
  std::string path = getSavePath(slot);
  if (!std::filesystem::exists(path))
  {
    return false;
  }
  gamedata = std::make_shared<GameData>();
  game = std::make_unique<Game>(gamedata, path);
  return true;
}

bool GameController::isGameLoaded() const
{
  return game != nullptr && gamedata != nullptr;
}

int GameController::getCurrentSeason() const
{
  return game->getCurrentSeason();
}

GameDateValue GameController::getCurrentDate() const
{
  return game->getCurrentDate();
}

bool GameController::hasSelectedTeam() const
{
  auto managed_id = game->getManagedTeamId();
  return managed_id != FREE_AGENTS_TEAM_ID &&
         (*gamedata).getTeam(managed_id).has_value();
}

std::optional<std::reference_wrapper<Team>> GameController::getManagedTeam()
{
  return (*gamedata).getTeam(game->getManagedTeamId());
}

std::optional<std::reference_wrapper<const Team>>
GameController::getManagedTeam() const
{
  return (*gamedata).getTeam(game->getManagedTeamId());
}

void GameController::selectManagedTeam(uint16_t team_id)
{
  game->setManagedTeamId(team_id);
}

const std::vector<std::reference_wrapper<const League>>&
GameController::getLeagues() const
{
  return (*gamedata).getLeaguesVector();
}

const std::vector<std::reference_wrapper<const Team>>&
GameController::getTeams() const
{
  return (*gamedata).getTeamsVector();
}

const std::vector<std::reference_wrapper<const Player>>&
GameController::getPlayersForTeam(uint16_t team_id) const
{
  return (*gamedata).getPlayersForTeam(team_id);
}

std::vector<std::reference_wrapper<const Team>>
GameController::getTeamsInLeague(uint8_t league_id) const
{
  std::vector<std::reference_wrapper<const Team>> teams;
  for (const auto& team : (*gamedata).getTeamsVector())
  {
    if (team.get().getLeagueId() == league_id)
    {
      teams.push_back(team);
    }
  }
  return teams;
}

std::optional<std::reference_wrapper<const League>>
GameController::getLeagueById(uint8_t league_id) const
{
  return (*gamedata).getLeague(league_id);
}

std::optional<std::reference_wrapper<const Team>> GameController::getTeamById(
    uint16_t team_id) const
{
  return (*gamedata).getTeam(team_id);
}

const StatsConfig& GameController::getStatsConfig() const
{
  return (*gamedata).getStatsConfig();
}

void GameController::advanceDay() { game->advanceDay(); }

void GameController::setMatchResult(GameDateValue date, uint16_t home_id,
                                    uint16_t away_id, uint8_t home_score,
                                    uint8_t away_score)
{
  if (game)
  {
    auto& matches = game->getCalendar().getMatchesForDateMutable(date);
    for (auto& m : matches)
    {
      if (m.getHomeTeamId() == home_id && m.getAwayTeamId() == away_id)
      {
        m.setPlayedResult(home_score, away_score);
        break;
      }
    }
  }
}

void GameController::saveGame() { game->saveGame(); }

GameController::SaveSlotMetadata GameController::getSaveSlotMetadata(
    int slot) const
{
  SaveSlotMetadata metadata;
  std::string path = getSavePath(slot);
  if (!std::filesystem::exists(path))
  {
    metadata.exists = false;
    return metadata;
  }
  metadata.exists = true;

  try
  {
    auto ftime = std::filesystem::last_write_time(path);
    auto sct =
        std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - std::filesystem::file_time_type::clock::now() +
            std::chrono::system_clock::now());
    std::time_t tt = std::chrono::system_clock::to_time_t(sct);
    std::tm* tm = std::localtime(&tt);
    char buf[100];
    if (std::strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M", tm))
    {
      metadata.real_date = buf;
    }
  }
  catch (...)
  {
    metadata.real_date = "";
  }

  sqlite3* db = nullptr;
  if (sqlite3_open_v2(path.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) ==
      SQLITE_OK)
  {
    sqlite3_stmt* stmt = nullptr;
    const char* sql_state =
        "SELECT managed_team_id, game_date FROM GameState WHERE id = 1;";
    if (sqlite3_prepare_v2(db, sql_state, -1, &stmt, nullptr) == SQLITE_OK)
    {
      if (sqlite3_step(stmt) == SQLITE_ROW)
      {
        int team_id = sqlite3_column_int(stmt, 0);
        const unsigned char* date_text = sqlite3_column_text(stmt, 1);
        metadata.game_date =
            date_text ? reinterpret_cast<const char*>(date_text) : "";

        sqlite3_stmt* team_stmt = nullptr;
        const char* sql_team = "SELECT name FROM Teams WHERE id = ?;";
        if (sqlite3_prepare_v2(db, sql_team, -1, &team_stmt, nullptr) ==
            SQLITE_OK)
        {
          sqlite3_bind_int(team_stmt, 1, team_id);
          if (sqlite3_step(team_stmt) == SQLITE_ROW)
          {
            const unsigned char* name_text = sqlite3_column_text(team_stmt, 0);
            metadata.team_name =
                name_text ? reinterpret_cast<const char*>(name_text) : "";
          }
          sqlite3_finalize(team_stmt);
        }
      }
      sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
  }
  return metadata;
}
