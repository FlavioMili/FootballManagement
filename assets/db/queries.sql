-- Each query has a unique identifier for easy lookup

-- LEAGUES
-- @QUERY_ID: INSERT_LEAGUE
INSERT INTO Leagues (name) VALUES (?);

-- @QUERY_ID: SELECT_LEAGUES
SELECT id, name FROM Leagues;

-- TEAMS
-- @QUERY_ID: INSERT_TEAM
INSERT INTO Teams (league_id, name, balance) VALUES (?, ?, ?);

-- @QUERY_ID: SELECT_TEAMS_BY_LEAGUE
SELECT id, name, balance FROM Teams WHERE league_id = ?;

-- @QUERY_ID: UPDATE_TEAM
UPDATE Teams SET name = ?, balance = ? WHERE id = ?;

-- @QUERY_ID: SELECT_ALL_TEAM_IDS
SELECT id FROM Teams;

-- PLAYERS
-- @QUERY_ID: INSERT_PLAYER
INSERT INTO Players (team_id, name, age, role, stats) VALUES (?, ?, ?, ?, ?);

-- @QUERY_ID: SELECT_PLAYERS_BY_TEAM
SELECT id, name, age, role, stats FROM Players WHERE team_id = ?;

-- @QUERY_ID: SELECT_ALL_PLAYERS
SELECT id, team_id, name, age, role, stats FROM Players;

-- @QUERY_ID: UPDATE_PLAYER
UPDATE Players SET name = ?, age = ?, role = ?, stats = ? WHERE id = ?;

-- @QUERY_ID: DELETE_PLAYER
DELETE FROM Players WHERE id = ?;

-- @QUERY_ID: TRANSFER_PLAYER
UPDATE Players SET team_id = ? WHERE id = ?;

-- CALENDAR
-- @QUERY_ID: INSERT_CALENDAR_MATCH
INSERT INTO Calendar (season, week, home_team_id, away_team_id, league_id) VALUES (?, ?, ?, ?, ?);

-- @QUERY_ID: SELECT_CALENDAR
SELECT week, home_team_id, away_team_id FROM Calendar WHERE season = ? AND league_id = ? ORDER BY week;

-- GAME STATE
-- @QUERY_ID: UPSERT_GAME_STATE
INSERT OR REPLACE INTO GameState (key, value) VALUES (?, ?);

-- @QUERY_ID: SELECT_GAME_STATE
SELECT key, value FROM GameState;

-- @QUERY_ID: SELECT_GAME_STATE_BY_KEY
SELECT value FROM GameState WHERE key = ?;

-- LEAGUE POINTS
-- @QUERY_ID: UPSERT_LEAGUE_POINTS
INSERT OR REPLACE INTO LeaguePoints (league_id, team_id, points) VALUES (?, ?, ?);

-- @QUERY_ID: SELECT_LEAGUE_POINTS
SELECT team_id, points FROM LeaguePoints WHERE league_id = ?;

-- @QUERY_ID: RESET_ALL_LEAGUE_POINTS
UPDATE LeaguePoints SET points = 0;

-- NAMES
-- @QUERY_ID: INSERT_FIRST_NAME
INSERT INTO FirstNames (name) VALUES (?);

-- @QUERY_ID: INSERT_LAST_NAME
INSERT INTO LastNames (name) VALUES (?);

-- @QUERY_ID: SELECT_FIRST_NAMES
SELECT name FROM FirstNames;

-- @QUERY_ID: SELECT_LAST_NAMES
SELECT name FROM LastNames;
