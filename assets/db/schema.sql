-- Leagues table
CREATE TABLE IF NOT EXISTS Leagues (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  name TEXT NOT NULL UNIQUE,
  parent_league_id INTEGER NULL,
  FOREIGN KEY(parent_league_id) REFERENCES Leagues(id)
);

-- Teams table
CREATE TABLE IF NOT EXISTS Teams (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  league_id INTEGER NOT NULL,
  name TEXT NOT NULL UNIQUE,
  balance INTEGER NOT NULL DEFAULT 0,
  strategy TEXT DEFAULT '{}',   -- JSON
  lineup TEXT DEFAULT '{}',     -- JSON
  FOREIGN KEY(league_id) REFERENCES Leagues(id)
);

-- Players table
CREATE TABLE IF NOT EXISTS Players (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  team_id INTEGER NOT NULL,
  first_name TEXT NOT NULL,
  last_name TEXT NOT NULL,
  age INTEGER NOT NULL DEFAULT 18,
  role TEXT NOT NULL,
  nationality TEXT NOT NULL,
  wage INTEGER NOT NULL DEFAULT 0,
  contract_years INTEGER NOT NULL DEFAULT 1,
  height INTEGER NOT NULL DEFAULT 175,
  foot TEXT NOT NULL DEFAULT 'Right', -- 'Left' or 'Right'
  stats TEXT NOT NULL,                -- JSON string
  status INTEGER DEFAULT 0,           -- bitmask: injured, transfer, etc.
  FOREIGN KEY(team_id) REFERENCES Teams(id)
);

-- Calendar table for match scheduling
CREATE TABLE IF NOT EXISTS Calendar (
  matchup_id INTEGER PRIMARY KEY AUTOINCREMENT,
  season INTEGER NOT NULL DEFAULT 0,
  week INTEGER NOT NULL,
  home_team_id INTEGER NOT NULL,
  away_team_id INTEGER NOT NULL,
  league_id INTEGER NOT NULL,
  match_date DATE,
  played BOOLEAN DEFAULT 0,
  home_goals INTEGER DEFAULT -1,
  away_goals INTEGER DEFAULT -1,
  FOREIGN KEY(home_team_id) REFERENCES Teams(id),
  FOREIGN KEY(away_team_id) REFERENCES Teams(id),
  FOREIGN KEY(league_id) REFERENCES Leagues(id)
);

-- Game state management
CREATE TABLE IF NOT EXISTS GameState (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  managed_team INTEGER DEFAULT 0,
  game_date DATE NOT NULL,
  season INTEGER NOT NULL DEFAULT 0,
  week INTEGER NOT NULL DEFAULT 0
);

-- League points/standings
CREATE TABLE IF NOT EXISTS LeaguePoints (
  league_id INTEGER NOT NULL,
  team_id INTEGER NOT NULL,
  points INTEGER NOT NULL DEFAULT 0,
  goal_difference INTEGER NOT NULL DEFAULT 0,
  PRIMARY KEY(league_id, team_id),
  FOREIGN KEY(league_id) REFERENCES Leagues(id),
  FOREIGN KEY(team_id) REFERENCES Teams(id)
);

-- Free agent team
INSERT OR IGNORE INTO Teams (id, league_id, name, balance)
VALUES (0, -1, 'Free agents', -1);

-- Enable WAL mode
PRAGMA journal_mode=WAL;
