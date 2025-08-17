-- Updated schema to match C++ implementation
-- Leagues table
CREATE TABLE IF NOT EXISTS Leagues (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL UNIQUE
);

-- Teams table
CREATE TABLE IF NOT EXISTS Teams (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    league_id INTEGER NOT NULL,
    name TEXT NOT NULL,
    balance INTEGER NOT NULL DEFAULT 0,
    FOREIGN KEY(league_id) REFERENCES Leagues(id)
);

-- Players table
CREATE TABLE IF NOT EXISTS Players (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    team_id INTEGER NOT NULL,
    name TEXT NOT NULL,
    age INTEGER NOT NULL DEFAULT 18,
    role TEXT NOT NULL,
    stats TEXT NOT NULL, -- JSON string containing player stats
    FOREIGN KEY(team_id) REFERENCES Teams(id)
);

-- Calendar table for match scheduling
CREATE TABLE IF NOT EXISTS Calendar (
    season INTEGER NOT NULL,
    week INTEGER NOT NULL,
    home_team_id INTEGER NOT NULL,
    away_team_id INTEGER NOT NULL,
    league_id INTEGER NOT NULL,
    PRIMARY KEY(season, league_id, week, home_team_id, away_team_id),
    FOREIGN KEY(home_team_id) REFERENCES Teams(id),
    FOREIGN KEY(away_team_id) REFERENCES Teams(id),
    FOREIGN KEY(league_id) REFERENCES Leagues(id)
);

-- Game state management
CREATE TABLE IF NOT EXISTS GameState (
    key TEXT PRIMARY KEY,
    value INTEGER NOT NULL
);

-- League points/standings
CREATE TABLE IF NOT EXISTS LeaguePoints (
    league_id INTEGER NOT NULL,
    team_id INTEGER NOT NULL,
    points INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY(league_id, team_id),
    FOREIGN KEY(league_id) REFERENCES Leagues(id),
    FOREIGN KEY(team_id) REFERENCES Teams(id)
);

-- Player name generation tables
CREATE TABLE IF NOT EXISTS FirstNames (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL UNIQUE
);

CREATE TABLE IF NOT EXISTS LastNames (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL UNIQUE
);

-- Enable WAL mode for better concurrency
PRAGMA journal_mode=WAL;
