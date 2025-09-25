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
SET name = ?, age = ?, role = ?, stats = ?
WHERE id = ?;

-- @QUERY_ID: DELETE_PLAYER
DELETE FROM Players WHERE id = ?;

-- @QUERY_ID: TRANSFER_PLAYER
UPDATE Players SET team_id = ? WHERE id = ?;

-- ==========================================
-- CALENDAR
-- ==========================================

-- @QUERY_ID: INSERT_CALENDAR_MATCH
INSERT INTO Calendar (season, week, date, home_team_id, away_team_id,
  home_score, away_score, league_id)
VALUES (?, ?, ?, ?, ?, NULL, NULL, ?);

-- @QUERY_ID: UPDATE_CALENDAR_RESULT
UPDATE Calendar
SET home_score = ?, away_score = ?
WHERE id = ?;

-- @QUERY_ID: SELECT_CALENDAR
SELECT id, season, week, date, home_team_id, away_team_id,
  home_score, away_score
FROM Calendar
WHERE season = ? AND league_id = ?
ORDER BY week, date;

-- ==========================================
-- GAME STATE
-- ==========================================

-- @QUERY_ID: UPSERT_GAME_STATE
INSERT OR REPLACE INTO GameState (id, managed_team, game_date)
VALUES (1, ?, ?);

-- @QUERY_ID: SELECT_GAME_STATE
SELECT managed_team, game_date FROM GameState WHERE id = 1;

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

-- ==========================================
-- NAMES (Optional for generation)
-- TODO remove
-- ==========================================

-- @QUERY_ID: INSERT_FIRST_NAME
INSERT INTO FirstNames (name) VALUES (?);

-- @QUERY_ID: INSERT_LAST_NAME
INSERT INTO LastNames (name) VALUES (?);

-- @QUERY_ID: SELECT_FIRST_NAMES
SELECT name FROM FirstNames;

-- @QUERY_ID: SELECT_LAST_NAMES
SELECT name FROM LastNames;
