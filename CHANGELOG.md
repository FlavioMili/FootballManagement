# Changelog

All notable changes to Football Management are listed here. The format
follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and version
numbers follow [Semantic Versioning](https://semver.org/).

## v1.0.0 (unreleased)

First public release. Everything below is new compared to the earlier
development snapshots, so most of it is listed under *Added*.

### Added

**Management shell**
- Sidebar with seven hubs (Home, Inbox, Squad, Training, Matches,
  Recruitment, Club); the open hub lists its screens with counters (a hover
  menu when the sidebar is collapsed), F1-F7 shortcuts, and a command
  palette (Ctrl+K) that finds players, clubs, screens and pending tasks.
- Browser-style Back and Forward between screens: two-finger touchpad
  swipe, mouse side buttons, Alt+Left/Alt+Right and top-bar arrows.
- Home dashboard with the next fixture, a "Next steps" card and a Continue
  button that advances the calendar on a background worker with a progress
  overlay.
- Holiday mode: your assistant manager runs the club until a date, the next
  match, the next decision or the end of the transfer window, with
  configurable reasons to come back early and a "Welcome back" summary.
- Inbox split into Decisions and Information, with inline actions (accept,
  reject, sign, release, reply), weekly medical digests and routine news filed
  as read.
- Delegation screen: hand lineup fixes, substitutions, training, contract
  renewals, scouting assignments, friendlies and youth contracts to the
  assistant, with three presets. Changing a delegated item by hand takes it
  back, with an undo notice.
- Screen tips and a first-season checklist for new careers.
- Full Italian localization: every screen, the inbox, the match commentary
  and the club names with their Italian articles, plurals and dates.
- Settings for language (English, Italian), resolution, fullscreen, six
  themes, accent colour (club colour or custom), interface scale (automatic
  or manual), table density, reduced motion, autosave frequency and backups.
- Responsive layout that works from 1280x720 up to 4K and at HiDPI scale 2;
  tables adapt their columns instead of scrolling sideways.
- Two-colour club badges and club identity (nickname, founding year,
  stadium) across the UI.

**Match engine and match day**
- Real-time match engine: one simulated second per real second at 1x, a
  10 Hz fixed step with a 40 Hz ball, player kinematics (acceleration,
  braking, fatigue), ball flight with drag, bounce and roll, goalkeeper
  positioning, dribbling, tackles, marking and pressing.
- Laws of the game: offside, fouls with advantage, yellow and red cards
  (including second yellows) with per-referee strictness, free kicks,
  penalties, corners, throw-ins, goal kicks, added time from recorded
  stoppages, and five substitutions in three windows (half-time is free).
- Background fixtures use the same engine at a cheaper step, so watched and
  unwatched matches follow the same model.
- Live match screen with 2D and 3D views of the same simulation, playback
  speeds from 1x to 30x, a Highlights mode, pitch focus, key-moment event
  feed with a timeline, statistics, substitutions and a Quick result button.
- 3D view with six cameras (broadcast, tactical, end, player follow, free
  and a TV director that cuts to goal close-ups, goal-line and reverse
  angles), mouse orbit, pan and zoom, a follow-ball option and a daylight
  look for afternoon kick-offs.
- 2D tactical view with a mown pitch, stands, nets, shirt numbers, facing,
  ball height and trail, pass and shot paths, offside flashes and an
  optional pressure map.
- In-match changes: a substitutions board (drag a substitute onto a player,
  several changes made together at the next stoppage, position swaps and
  assistant suggestions), a tactics panel (style presets, sliders and
  formation presets with their familiarity cost, undo) and eleven touchline
  shouts (Shift + number keys) whose effect fades when repeated.
- Your match pauses at half-time and the other breaks at any speed
  (optional), and Highlights mode opens each highlight on live play.
- Pre-match and half-time team talks. How the squad takes a talk sharpens
  (or unsettles) its decisions, execution and pressing for the first minutes
  of the half, in watched and simulated matches.
- Player roles and duties per position (line or sweeper keeper, inside
  full-back, stopper, cover defender, anchor, playmaker, box-to-box, second
  striker, inside forward, touchline winger, target forward, poacher,
  pressing forward, false nine) with Defend, Support or Attack duties. Each
  role changes how the player positions himself with and without the ball,
  how often he runs in behind, how much width he keeps, how riskily he
  passes, when he presses and how readily he shoots. AI clubs give their
  players the roles that suit them.
- A separate shape with the ball: drag each player to his attacking-phase
  spot on the tactics screen, or start from a ready-made shape (full-backs
  push on, build with three, narrow front line).
- Lineup gate before kick-off: the assistant replaces injured or suspended
  players, or you fix the lineup yourself.
- Knockout matches (cup ties, continental second legs and finals, national
  team finals) are played on through extra time, with a sixth substitution,
  and a live penalty shootout, whether you watch them or not; a second leg
  counts the first leg's goals.
- Structured match commentary in English and Italian with the clubs' names.
- Procedural match sound: a crowd that follows the play and reacts to
  chances, goals and cards, referee whistles and ball contacts, with master,
  crowd and effects volumes and a mute key (M).
- Match report with per-player lines, and half-time and full-time analysis.
- Opposition report for the next opponent, with individual instructions
  the engine plays out: mark a player tightly, close him down, show him onto
  his weaker foot or double up on him.

**Competitions**
- 22 leagues with 440 clubs: a top division and a second division in each
  of eleven countries, with three clubs promoted and relegated every season,
  and a domestic cup per country.
- Staggered matchdays: each league round is spread over a long weekend
  (Friday to Monday, or Tuesday and Wednesday for midweek rounds) with the
  league's own days and kick-off times, so each Continue day plays fewer
  matches.
- Continental club competitions: Old Continent Champions Cup (36-club league
  phase), Old Continent Shield and Pan-American Champions Cup, with Swiss-style
  draws, play-offs, two-legged knockout rounds, a neutral final, prize money
  and club and association coefficients that decide future access.
- National teams: qualifiers, alternating world and continental finals,
  nations series and friendlies in international windows, squad call-ups,
  caps and goals, travel fatigue and club compensation.
- Standings, fixtures and results, a season calendar, season history,
  top scorers and a data hub with team and player trends.

**World simulation**
- Club economy sized on expected income (TV, merit money, gate receipts,
  commercial, continental money), wage and staff budgets, a finance ledger,
  ticket pricing, and a transfer budget limited by the cash the club can
  actually spend.
- Board objectives, board confidence, warnings, dismissal, and a transfer
  embargo when the club stays in the red.
- Facility projects requested from the board, including a medical centre.
- Injuries with a medical centre screen, fatigue, sharpness, morale and form.
- Player development driven by age, training, minutes and staff.
- Squad status, captain, vice-captain and set-piece takers.
- Squad planner (depth, needs, age profile, contract calendar, next-season
  projection) and a player comparison screen.
- Player conversations, promises, dressing-room mood and short story chains
  (debuts, breakouts, captain disputes, poor runs, transfer sagas, comebacks,
  milestones, rivalries).
- Pre-season planning with friendlies against clubs from the same region,
  and mentoring groups for young players.
- Generated players with names that follow each nationality.

**Youth academy**
- Yearly intake cycle: a preview from the head of youth development on
  1 February, trialists arrive on 15 March, then a 30-day decision window.
- Intake quality from facilities, recruitment, coaching, staff, reputation
  and the country's talent pool, with occasional golden generations.
- Under-18 league with minutes, ratings and goals that feed development;
  scholarships, first professional contracts and homegrown status.
- Under-21 squad for every club on its own screen (Squad hub): players move
  between the first team, the U21s and the U18s under age rules (21 and
  younger, plus three outfield players and a goalkeeper over age); U18
  players move up at 19. One U21 league per country plays a cheap weekly
  round whose minutes speed up development, and AI clubs send young players
  outside their plans down and promote them when they are ready.

**Transfers and scouting**
- Transfer market with search, recommendations based on squad needs,
  affordability filters and a fit score.
- Negotiations with counter-offers, instalments, loans, pre-contracts, free
  agents, transfer listing and releases with severance.
- Bids for your players are real negotiations: counter with fee,
  instalments, add-ons and sell-on, name your price or declare a player not
  for sale. Buyers answer on later days with their own structures, walk
  away when pushed too far, compete with rival bids and push harder near
  deadline day; a player who wanted the move can take a rejection badly. Selling clubs ask
  more for key players and long contracts; some refuse to sell key players.
- Contract talks with agents who open high and soften round by round.
- Scouts with nationalities, languages and regional experience; sending a
  scout to a continent, country or league shows the expected effectiveness.
- Players outside your club are shown as scouted estimates (ranges) until
  your knowledge is good enough.
- AI clubs buy, sell, loan and trim their squads.

**Staff and training**
- Staff grouped into coaching, medical, scouting and youth, with hiring,
  extensions and releases.
- Weekly training plans with presets, per-slot intensity and automatic
  adjustment in congested weeks.

**Manager career**
- Manager profile with nationality, reputation and coaching licence.
- Job centre with vacancies, applications, interviews and contract
  negotiation; unsolicited offers, resignation, sacking and severance.
- Option to start unemployed; AI managers for every club with their own
  sackings and appointments.
- National-team jobs: federations look for a coach after failed campaigns
  and tournaments; apply or accept an approach, alone or (with a continental
  reputation) next to your club job. Pick the squad on the Call-ups screen
  in the week before each window; qualifiers, nations series and finals
  results move your reputation, qualifying earns a new contract and missing
  out can cost a big nation's job.

**Awards and records**
- Monthly awards (player, young player, manager, goal of the month) and
  season awards (player, young player, manager, golden boot, golden glove,
  team of the season), player honours, and club and league records
  (biggest wins, attendances, points, goals, record signings and sales).

**Saves**
- Three save slots with autosave and numbered backups that can be restored
  from the main menu.
- Crash-safe saving: the game works on an in-memory copy and replaces the
  slot file only with a verified snapshot.
- Numbered save migrations; saves from earlier development builds are
  upgraded when loaded.

**Tools, tests and packaging**
- `fm_lab`, a headless balance lab for batches of matches, whole seasons and
  tactic round robins, with Markdown and JSON reports.
- Unit, core, GUI, playtest, monkey and adversarial test suites.
- CI builds and release packages for Linux (tar.gz and AppImage), macOS
  (Apple silicon) and Windows (experimental).

### Changed

- The match engine was rewritten as a real-time simulation; results,
  statistics and the live views all come from it.
- The old separate scenes were replaced by one management shell.
- SDL3, SDL3_ttf and Dear ImGui are built from pinned sources by default
  (`FM_USE_SYSTEM_SDL=ON` to use exact-version system packages).
- Read-only game data is found next to the installed executable, and all
  writable files (saves, settings, logs, captures) live in the user data
  directory.

### Fixed

- Blurry or wrongly scaled rendering on HiDPI displays.
- A memory leak in the transfer market.

### Known limitations

- The second divisions exist only in new careers; saves started with the
  earlier twelve-league world keep their leagues.
- A knockout match abandoned while level (a side down to six players) is
  settled by a statistical extra time and shootout instead of being awarded.
- Players who play through an injury because nobody fit can replace them
  carry no extra risk of aggravating it.
- Loan offers for your players can only be accepted or rejected, and a
  buying club does not check its wage budget until the deal is settled.
- The swipe direction and distance for Back/Forward are tuned for common
  touchpads; natural-scrolling setups may need them flipped.
- Italian commentary uses club names without their articles.
- Match sound is synthesised and still being tuned by ear; there are no club
  chants and no commentary voice.
- You do not watch or direct your national team's matches live; they are
  simulated with the squad you pick.
- Each player has one nationality.
- U21 players are not sold or loaned out by AI clubs while in the U21
  squad (they can be when back in the first team).
- Awards are not rebuilt for careers saved before awards existed.
- Engine and world calibration is still being tuned (for example strikers
  score too large a share of the goals, set pieces too few, and possession
  differs too little between strong and weak sides).
- macOS packages are only ad-hoc signed and need macOS 15 on Apple silicon;
  Windows packages are experimental. See
  [docs/development/release.md](docs/development/release.md).
