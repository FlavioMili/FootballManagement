// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <stdexcept>
#include <string_view>
#include <unordered_map>

enum class Query {
  INSERT_LEAGUE,
  INSERT_LEAGUE_WITH_ID,
  SELECT_LEAGUES,
  INSERT_TEAM,
  INSERT_TEAM_WITH_ID,
  SELECT_TEAMS_BY_LEAGUE,
  UPDATE_TEAM,
  SELECT_ALL_TEAM_IDS,
  INSERT_PLAYER,
  INSERT_PLAYER_WITH_ID,
  SELECT_PLAYERS_BY_TEAM,
  SELECT_ALL_PLAYERS,
  UPDATE_PLAYER,
  DELETE_PLAYER,
  TRANSFER_PLAYER,
  INSERT_FIXTURE,
  UPDATE_FIXTURE_RESULT,
  UPSERT_GAME_STATE,
  SELECT_GAME_STATE,
  UPSERT_LEAGUE_POINTS,
  SELECT_LEAGUE_POINTS,
  RESET_ALL_LEAGUE_POINTS,
  COUNT
};

struct QueryMapEntry {
  std::string_view id;
  Query query;
};

const std::unordered_map<std::string, Query> string_to_query_map = {
    // Leagues
    {"INSERT_LEAGUE", Query::INSERT_LEAGUE},
    {"INSERT_LEAGUE_WITH_ID", Query::INSERT_LEAGUE_WITH_ID},
    {"SELECT_LEAGUES", Query::SELECT_LEAGUES},

    // Teams
    {"INSERT_TEAM", Query::INSERT_TEAM},
    {"INSERT_TEAM_WITH_ID", Query::INSERT_TEAM_WITH_ID},
    {"SELECT_TEAMS_BY_LEAGUE", Query::SELECT_TEAMS_BY_LEAGUE},
    {"UPDATE_TEAM", Query::UPDATE_TEAM},
    {"SELECT_ALL_TEAM_IDS", Query::SELECT_ALL_TEAM_IDS},

    // Players
    {"INSERT_PLAYER", Query::INSERT_PLAYER},
    {"INSERT_PLAYER_WITH_ID", Query::INSERT_PLAYER_WITH_ID},
    {"SELECT_PLAYERS_BY_TEAM", Query::SELECT_PLAYERS_BY_TEAM},
    {"SELECT_ALL_PLAYERS", Query::SELECT_ALL_PLAYERS},
    {"UPDATE_PLAYER", Query::UPDATE_PLAYER},
    {"DELETE_PLAYER", Query::DELETE_PLAYER},
    {"TRANSFER_PLAYER", Query::TRANSFER_PLAYER},

    // Calendar
    {"INSERT_FIXTURE", Query::INSERT_FIXTURE},
    {"UPDATE_FIXTURE_RESULT", Query::UPDATE_FIXTURE_RESULT},

    // Game State
    {"UPSERT_GAME_STATE", Query::UPSERT_GAME_STATE},
    {"SELECT_GAME_STATE", Query::SELECT_GAME_STATE},

    // League Points
    {"UPSERT_LEAGUE_POINTS", Query::UPSERT_LEAGUE_POINTS},
    {"SELECT_LEAGUE_POINTS", Query::SELECT_LEAGUE_POINTS},
    {"RESET_ALL_LEAGUE_POINTS", Query::RESET_ALL_LEAGUE_POINTS},
};
