-- Leagues table
CREATE TABLE IF NOT EXISTS Leagues (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  name TEXT NOT NULL UNIQUE,
  parent_league_id INTEGER NULL,
  tiebreak INTEGER NOT NULL DEFAULT 0, -- 0: goal difference first, 1: head-to-head first
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
  reputation INTEGER NOT NULL DEFAULT 0,          -- 1-100 (0: legacy save)
  stadium_capacity INTEGER NOT NULL DEFAULT 0,
  ticket_price INTEGER NOT NULL DEFAULT 0,        -- average yield per seat
  training_facilities INTEGER NOT NULL DEFAULT 0, -- 1-100
  youth_facilities INTEGER NOT NULL DEFAULT 0,    -- 1-100
  transfer_budget INTEGER NOT NULL DEFAULT 0,
  wage_budget INTEGER NOT NULL DEFAULT 0,         -- weekly
  recent_form TEXT NOT NULL DEFAULT '',           -- e.g. 'WDLWW', newest first
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
  potential REAL NOT NULL DEFAULT 0,  -- hidden, overall scale (0: legacy)
  traits TEXT NOT NULL DEFAULT '',    -- professionalism,ambition,temperament,loyalty,injury_proneness
  dynamics TEXT NOT NULL DEFAULT '',  -- condition, sharpness, morale, injury, form (CSV)
  FOREIGN KEY(team_id) REFERENCES Teams(id)
);

-- Fixtures table for match scheduling
CREATE TABLE IF NOT EXISTS Fixtures (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    game_date TEXT NOT NULL,
    home_team_id INTEGER NOT NULL,
    away_team_id INTEGER NOT NULL,
    match_type INTEGER NOT NULL DEFAULT 0, -- 0: LEAGUE, 1: FRIENDLY, 2: CUP 
    home_goals INTEGER,
    away_goals INTEGER,
    played INTEGER NOT NULL DEFAULT 0,
    competition_id INTEGER NOT NULL DEFAULT 0, -- league ID, or country root league ID for cups
    stage INTEGER NOT NULL DEFAULT 0,          -- league round / cup round (1-based)
    extra_time INTEGER NOT NULL DEFAULT 0,
    home_penalties INTEGER,                    -- NULL unless decided on penalties
    away_penalties INTEGER,
    FOREIGN KEY(home_team_id) REFERENCES Teams(id),
    FOREIGN KEY(away_team_id) REFERENCES Teams(id),
    UNIQUE(game_date, home_team_id, away_team_id)
);

-- Game state management
CREATE TABLE IF NOT EXISTS GameState (
  id INTEGER PRIMARY KEY,
  managed_team_id INTEGER,
  game_date TEXT,
  current_season INTEGER
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

-- Structured summary of every played fixture (natural key: date + teams)
CREATE TABLE IF NOT EXISTS MatchReports (
  game_date TEXT NOT NULL,
  home_team_id INTEGER NOT NULL,
  away_team_id INTEGER NOT NULL,
  season INTEGER NOT NULL,
  match_type INTEGER NOT NULL DEFAULT 0,
  competition_id INTEGER NOT NULL DEFAULT 0,
  stage INTEGER NOT NULL DEFAULT 0,
  home_goals INTEGER NOT NULL DEFAULT 0,
  away_goals INTEGER NOT NULL DEFAULT 0,
  extra_time INTEGER NOT NULL DEFAULT 0,
  home_penalties INTEGER,
  away_penalties INTEGER,
  attendance INTEGER NOT NULL DEFAULT 0,
  stats TEXT NOT NULL DEFAULT '{}',   -- JSON team totals
  events TEXT NOT NULL DEFAULT '[]',  -- JSON scorers/cards with minutes
  players TEXT NOT NULL DEFAULT '[]', -- JSON per-player lines
  PRIMARY KEY(game_date, home_team_id, away_team_id)
);
CREATE INDEX IF NOT EXISTS idx_match_reports_season ON MatchReports(season);

-- Competitive per-player totals per season, team and competition type
CREATE TABLE IF NOT EXISTS PlayerSeasonStats (
  season INTEGER NOT NULL,
  player_id INTEGER NOT NULL,
  team_id INTEGER NOT NULL,
  competition_type INTEGER NOT NULL,
  appearances INTEGER NOT NULL DEFAULT 0,
  starts INTEGER NOT NULL DEFAULT 0,
  minutes INTEGER NOT NULL DEFAULT 0,
  goals INTEGER NOT NULL DEFAULT 0,
  assists INTEGER NOT NULL DEFAULT 0,
  yellow_cards INTEGER NOT NULL DEFAULT 0,
  red_cards INTEGER NOT NULL DEFAULT 0,
  rating_total REAL NOT NULL DEFAULT 0,
  rated_matches INTEGER NOT NULL DEFAULT 0,
  PRIMARY KEY(season, player_id, team_id, competition_type)
);

-- Card counts and outstanding suspensions per competition type
CREATE TABLE IF NOT EXISTS PlayerDiscipline (
  player_id INTEGER NOT NULL,
  competition_type INTEGER NOT NULL, -- 0: LEAGUE, 2: CUP
  season_yellows INTEGER NOT NULL DEFAULT 0,
  season_reds INTEGER NOT NULL DEFAULT 0,
  ban_matches INTEGER NOT NULL DEFAULT 0,
  PRIMARY KEY(player_id, competition_type)
);

-- Final outcome of every competition per season
CREATE TABLE IF NOT EXISTS SeasonHistory (
  season INTEGER NOT NULL,
  start_year INTEGER NOT NULL,
  competition_type INTEGER NOT NULL, -- 0: LEAGUE, 2: CUP
  competition_id INTEGER NOT NULL,
  competition_name TEXT NOT NULL,
  champion_id INTEGER,
  runner_up_id INTEGER,
  promoted TEXT NOT NULL DEFAULT '[]',  -- JSON team IDs promoted out of the league
  relegated TEXT NOT NULL DEFAULT '[]', -- JSON team IDs relegated out of the league
  top_scorer_id INTEGER,
  top_scorer_goals INTEGER NOT NULL DEFAULT 0,
  PRIMARY KEY(season, competition_type, competition_id)
);

-- Free agent team
INSERT OR IGNORE INTO Teams (id, league_id, name, balance)
VALUES (0, -1, 'Free agents', -1);

-- Transfer list for the transfer market
CREATE TABLE IF NOT EXISTS TransferList (
    player_id INTEGER PRIMARY KEY,
    asking_price INTEGER NOT NULL DEFAULT 0,
    listing_date TEXT NOT NULL,
    highest_bidder_id INTEGER,
    highest_bid INTEGER NOT NULL DEFAULT 0,
    FOREIGN KEY(player_id) REFERENCES Players(id)
);

-- Club ledgers: every balance change as a dated transaction
CREATE TABLE IF NOT EXISTS FinanceLedger (
  team_id INTEGER NOT NULL,
  seq INTEGER NOT NULL,          -- position in the club's ledger
  game_date INTEGER NOT NULL,    -- YYYYMMDD
  category INTEGER NOT NULL,     -- FinanceCategory
  amount INTEGER NOT NULL,       -- positive = income
  PRIMARY KEY(team_id, seq)
) WITHOUT ROWID;

-- Managed club inbox (text stored as language keys + JSON arguments)
CREATE TABLE IF NOT EXISTS InboxMessages (
  id INTEGER PRIMARY KEY,
  game_date INTEGER NOT NULL,    -- YYYYMMDD
  category INTEGER NOT NULL,     -- InboxCategory
  title_key TEXT NOT NULL,
  body_key TEXT NOT NULL,
  args TEXT NOT NULL DEFAULT '[]',
  is_read INTEGER NOT NULL DEFAULT 0,
  player_id INTEGER,
  team_id INTEGER
);

-- Board of the managed club (single row)
CREATE TABLE IF NOT EXISTS BoardState (
  id INTEGER PRIMARY KEY CHECK (id = 1),
  team_id INTEGER NOT NULL,
  season_year INTEGER NOT NULL,
  objective INTEGER NOT NULL,
  expected_position INTEGER NOT NULL,
  target_position INTEGER NOT NULL,
  confidence REAL NOT NULL,
  low_reviews INTEGER NOT NULL DEFAULT 0,
  league_matches INTEGER NOT NULL DEFAULT 0,
  dismissed INTEGER NOT NULL DEFAULT 0,
  recent_deltas TEXT NOT NULL DEFAULT '[]'
);

-- World simulation state (single row): RNG seed and player id counter
CREATE TABLE IF NOT EXISTS WorldState (
  id INTEGER PRIMARY KEY CHECK (id = 1),
  seed INTEGER NOT NULL,
  next_player_id INTEGER NOT NULL
);

-- Scouting department of the managed club (single row)
CREATE TABLE IF NOT EXISTS ScoutingState (
  id INTEGER PRIMARY KEY CHECK (id = 1),
  team_id INTEGER NOT NULL,
  next_assignment_id INTEGER NOT NULL,
  next_report_id INTEGER NOT NULL,
  next_focus_id INTEGER NOT NULL
);

-- Knowledge beyond the relation baseline (own club, league, country)
CREATE TABLE IF NOT EXISTS ScoutingKnowledge (
  player_id INTEGER PRIMARY KEY,
  knowledge REAL NOT NULL,       -- 0-100
  judging_ability INTEGER NOT NULL,
  judging_potential INTEGER NOT NULL,
  last_report_day INTEGER NOT NULL DEFAULT 0  -- day ordinal, 0 = none
) WITHOUT ROWID;

CREATE TABLE IF NOT EXISTS ScoutAssignments (
  id INTEGER PRIMARY KEY,
  scout_id INTEGER NOT NULL,
  kind INTEGER NOT NULL,         -- ScoutTargetKind
  target_id INTEGER NOT NULL,
  start_date INTEGER NOT NULL,   -- YYYYMMDD
  duration_days INTEGER NOT NULL,
  days_done INTEGER NOT NULL,
  cost INTEGER NOT NULL,
  players_observed INTEGER NOT NULL,
  reports_filed INTEGER NOT NULL,
  finished INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS ScoutReports (
  id INTEGER PRIMARY KEY,
  game_date INTEGER NOT NULL,    -- YYYYMMDD
  player_id INTEGER NOT NULL,
  assignment_id INTEGER NOT NULL,
  scout_id INTEGER NOT NULL,
  scout_name TEXT NOT NULL,
  knowledge INTEGER NOT NULL,
  confidence INTEGER NOT NULL,
  overall REAL NOT NULL,
  potential_low REAL NOT NULL,
  potential_high REAL NOT NULL,
  estimated_fee INTEGER NOT NULL,
  grade INTEGER NOT NULL,        -- ScoutGrade
  reasons INTEGER NOT NULL       -- bits: 1 fits need, 2 affordable, 4 available
);

CREATE TABLE IF NOT EXISTS RecruitmentFocus (
  id INTEGER PRIMARY KEY,
  role INTEGER NOT NULL,         -- PlayerRole, UNKNOWN = any
  min_age INTEGER NOT NULL,
  max_age INTEGER NOT NULL,
  max_fee INTEGER NOT NULL,      -- 0 = no limit
  max_wage INTEGER NOT NULL,     -- 0 = no limit
  min_ability INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS ScoutShortlist (
  player_id INTEGER PRIMARY KEY,
  added_date INTEGER NOT NULL,   -- YYYYMMDD
  last_team INTEGER NOT NULL,
  last_flags INTEGER NOT NULL    -- ShortlistFlag bits of the last check
);

-- Football staff of every club (team_id 0: available on the staff market)
CREATE TABLE IF NOT EXISTS Staff (
  id INTEGER PRIMARY KEY,
  team_id INTEGER NOT NULL,
  first_name TEXT NOT NULL,
  last_name TEXT NOT NULL,
  nationality INTEGER NOT NULL,     -- Language
  age INTEGER NOT NULL,
  role INTEGER NOT NULL,            -- StaffRole
  attributes TEXT NOT NULL,         -- CSV of StaffAttribute values (1-100)
  wage INTEGER NOT NULL DEFAULT 0,  -- weekly
  contract_years INTEGER NOT NULL DEFAULT 0
);

-- Training plan, tactical familiarity and training load of every club
CREATE TABLE IF NOT EXISTS TeamTraining (
  team_id INTEGER PRIMARY KEY,
  preset INTEGER NOT NULL,          -- TrainingPreset
  intensity INTEGER NOT NULL,       -- TrainingIntensity (squad-wide)
  auto_congestion INTEGER NOT NULL,
  schedule TEXT NOT NULL,           -- CSV session,intensity for MD+1..MD-1
  familiarity REAL NOT NULL,        -- 0-100
  tactic TEXT NOT NULL DEFAULT '',  -- CSV snapshot of the drilled tactic
  week_load REAL NOT NULL DEFAULT 0,
  last_week_load REAL NOT NULL DEFAULT 0,
  last_match_day INTEGER NOT NULL DEFAULT 0 -- day ordinal
);

-- Individual training focus and workload of every player
CREATE TABLE IF NOT EXISTS PlayerTraining (
  player_id INTEGER PRIMARY KEY,
  focus INTEGER NOT NULL DEFAULT 0, -- TrainingFocus
  acute REAL NOT NULL DEFAULT 0,    -- 7-day load
  chronic REAL NOT NULL DEFAULT 0,  -- 28-day load
  pending REAL NOT NULL DEFAULT 0,  -- load of the current day
  trend REAL NOT NULL DEFAULT 0     -- smoothed weekly overall change
);

-- Transfer market: every completed move, oldest first (append-only)
CREATE TABLE IF NOT EXISTS TransferHistory (
  seq INTEGER PRIMARY KEY,
  player_id INTEGER NOT NULL,
  game_date INTEGER NOT NULL,     -- YYYYMMDD
  from_team_id INTEGER NOT NULL,
  to_team_id INTEGER NOT NULL,
  fee INTEGER NOT NULL DEFAULT 0, -- fee, loan fee or release compensation
  kind INTEGER NOT NULL           -- TransferKind
);
CREATE INDEX IF NOT EXISTS idx_transfer_history_player ON TransferHistory(player_id);

-- Instalments, add-ons and sell-on clauses still to be paid
CREATE TABLE IF NOT EXISTS TransferObligations (
  id INTEGER PRIMARY KEY,
  kind INTEGER NOT NULL,          -- ObligationKind
  player_id INTEGER NOT NULL,
  payer_id INTEGER NOT NULL,
  payee_id INTEGER NOT NULL,
  amount INTEGER NOT NULL,        -- money, or percent for sell-on clauses
  due_date INTEGER NOT NULL,      -- YYYYMMDD (signing date for conditions)
  target INTEGER NOT NULL DEFAULT 0,
  baseline INTEGER NOT NULL DEFAULT 0
);

-- Active loans (the player sits in the borrower's squad)
CREATE TABLE IF NOT EXISTS Loans (
  player_id INTEGER PRIMARY KEY,
  parent_id INTEGER NOT NULL,
  borrower_id INTEGER NOT NULL,
  start_date INTEGER NOT NULL,
  end_date INTEGER NOT NULL,
  wage_share INTEGER NOT NULL,    -- percent paid by the borrower
  full_wage INTEGER NOT NULL,
  option_fee INTEGER NOT NULL DEFAULT 0,
  obligation INTEGER NOT NULL DEFAULT 0,
  recall_clause INTEGER NOT NULL DEFAULT 0
);

-- Pre-contracts executed on 1 July
CREATE TABLE IF NOT EXISTS PreContracts (
  player_id INTEGER PRIMARY KEY,
  from_team_id INTEGER NOT NULL,
  to_team_id INTEGER NOT NULL,
  agreed_date INTEGER NOT NULL,
  weekly_wage INTEGER NOT NULL,
  years INTEGER NOT NULL,
  signing_bonus INTEGER NOT NULL DEFAULT 0,
  release_clause INTEGER NOT NULL DEFAULT 0,
  promised_role INTEGER           -- SquadRole, NULL = no promise
);

-- Release clauses, playing-time promises and the loan list
CREATE TABLE IF NOT EXISTS PlayerMarketFlags (
  player_id INTEGER PRIMARY KEY,
  release_clause INTEGER NOT NULL DEFAULT 0,
  promised_role INTEGER,
  promise_date INTEGER NOT NULL DEFAULT 0,
  loan_listed INTEGER NOT NULL DEFAULT 0
);

-- AI offers for the managed club's players and its own open talks
CREATE TABLE IF NOT EXISTS TransferOffers (
  id INTEGER NOT NULL,
  kind INTEGER NOT NULL,          -- 0 transfer offer, 1 loan offer, 2 talks
  player_id INTEGER NOT NULL,
  club_id INTEGER NOT NULL,
  created INTEGER NOT NULL,
  expires INTEGER NOT NULL,
  rounds INTEGER NOT NULL DEFAULT 0,
  terms TEXT NOT NULL DEFAULT '{}' -- JSON offer / loan terms
);

-- Enable WAL mode
PRAGMA journal_mode=WAL;
