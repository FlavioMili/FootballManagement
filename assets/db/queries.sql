-- Each query has a unique identifier for easy lookup
-- Make sure to change the src/global/queries.h when changing this file

-- ==========================================
-- LEAGUES
-- ==========================================

-- @QUERY_ID: INSERT_LEAGUE
INSERT OR IGNORE INTO Leagues (name, parent_league_id) VALUES (?, ?);

-- @QUERY_ID: INSERT_LEAGUE_WITH_ID
INSERT OR IGNORE INTO Leagues (id, name, parent_league_id) VALUES (?, ?, ?);


-- @QUERY_ID: SELECT_LEAGUES
SELECT id, name, parent_league_id FROM Leagues;

-- ==========================================
-- TEAMS
-- ==========================================

-- @QUERY_ID: INSERT_TEAM
INSERT OR IGNORE INTO Teams (league_id, name, balance, strategy, lineup)
VALUES (?, ?, ?, ?, ?);

-- @QUERY_ID: INSERT_TEAM_WITH_ID
INSERT OR IGNORE INTO Teams (id, league_id, name, balance, strategy, lineup)
VALUES (?, ?, ?, ?, ?, ?);

-- @QUERY_ID: SELECT_TEAMS_BY_LEAGUE
SELECT id, name, balance, strategy, lineup
FROM Teams WHERE league_id = ?;

-- @QUERY_ID: UPDATE_TEAM
UPDATE Teams
SET name = ?, balance = ?, strategy = ?, lineup = ?
WHERE id = ?;

-- @QUERY_ID: SELECT_ALL_TEAM_IDS
SELECT id FROM Teams;

-- ==========================================
-- PLAYERS
-- ==========================================

-- @QUERY_ID: INSERT_PLAYER
INSERT INTO Players (
  team_id,
  first_name,
  last_name,
  age,
  role,
  nationality,
  wage,
  contract_years,
  height,
  foot,
  stats,
  status
  ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);

-- @QUERY_ID: INSERT_PLAYER_WITH_ID
INSERT OR IGNORE INTO Players (
  id,
  team_id,
  first_name,
  last_name,
  age,
  role,
  nationality,
  wage,
  contract_years,
  height,
  foot,
  stats,
  status
  ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);

-- @QUERY_ID: SELECT_PLAYERS_BY_TEAM
SELECT id,
  first_name,
  last_name,
  age,
  role,
  nationality,
  wage,
  contract_years,
  height,
  foot,
  stats,
  status
FROM Players
WHERE team_id = ?;

-- @QUERY_ID: SELECT_ALL_PLAYERS
SELECT id,
  team_id,
  first_name,
  last_name,
  age,
  role,
  nationality,
  wage,
  contract_years,
  height,
  foot,
  stats,
  status
FROM Players;

-- @QUERY_ID: UPDATE_PLAYER
UPDATE Players
SET first_name = ?, last_name = ?, age = ?, role = ?, stats = ?
WHERE id = ?;

-- @QUERY_ID: DELETE_PLAYER
DELETE FROM Players WHERE id = ?;

-- @QUERY_ID: TRANSFER_PLAYER
UPDATE Players SET team_id = ? WHERE id = ?;

-- ==========================================
-- FIXTURES
-- ==========================================

-- @QUERY_ID: DELETE_FIXTURES_BY_LEAGUE
DELETE FROM Fixtures WHERE league_id = ?;

-- @QUERY_ID: INSERT_FIXTURE
INSERT INTO Fixtures (game_date, home_team_id, away_team_id, league_id) VALUES (?, ?, ?, ?);

-- @QUERY_ID: SELECT_FIXTURES_BY_LEAGUE
SELECT game_date, home_team_id, away_team_id, home_goals, away_goals, played FROM Fixtures WHERE league_id = ?;

-- @QUERY_ID: UPDATE_FIXTURE_RESULT
UPDATE Fixtures SET home_goals = ?, away_goals = ?, played = 1 WHERE game_date = ? AND home_team_id = ? AND away_team_id = ?;

-- ==========================================
-- GAME STATE
-- ==========================================

-- @QUERY_ID: UPSERT_GAME_STATE
INSERT OR REPLACE INTO GameState (id, managed_team_id, game_date, current_season)
VALUES (1, ?, ?, ?);

-- @QUERY_ID: SELECT_GAME_STATE
SELECT managed_team_id, game_date, current_season FROM GameState WHERE id = 1;

-- ==========================================
-- LEAGUE POINTS
-- ==========================================

-- @QUERY_ID: UPSERT_LEAGUE_POINTS
INSERT OR REPLACE INTO LeaguePoints (league_id, team_id, points)
VALUES (?, ?, ?);

-- @QUERY_ID: SELECT_LEAGUE_POINTS
SELECT team_id, points FROM LeaguePoints WHERE league_id = ?;

-- @QUERY_ID: RESET_ALL_LEAGUE_POINTS
UPDATE LeaguePoints SET points = 0;
