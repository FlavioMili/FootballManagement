// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "model/game.h"

#include <algorithm>
#include <iostream>
#include <optional>
#include <unordered_map>
#include <vector>

#include "database/database_connection.h"
#include "database/gamedata.h"
#include "database/repositories/fixture_repository.h"
#include "database/repositories/game_state_repository.h"
#include "database/repositories/league_repository.h"
#include "database/repositories/player_repository.h"
#include "database/repositories/team_repository.h"
#include "database/save_manager.h"
#include "global/global.h"
#include "global/logger.h"
#include "global/paths.h"
#include "model/competition.h"
#include "model/league.h"
#include "model/role_utils.h"
#include "model/team.h"
#include "model/world_rng.h"

Game::Game(std::shared_ptr<GameData> gd,
           std::shared_ptr<DatabaseConnection> conn)
    : db_conn(std::move(conn)),
      gamedata(std::move(gd)),
      competitions(gamedata, db_conn),
      world(gamedata),
      transfers(gamedata, world, competitions),
      career(gamedata),
      international(gamedata),
      currentDate(START_DATE)
{
  (*gamedata).loadFromDB(db_conn);
  // Continental news reaches the inbox when it concerns the managed club;
  // results everybody talks about (winners) are filed as read.
  competitions.getContinental().setNewsSink(
      [this](InboxMessage message, const std::vector<TeamID>& clubs,
             bool headline)
      {
        const bool ours = managed_team_id != FREE_AGENTS_TEAM_ID &&
                          std::ranges::contains(clubs, managed_team_id);
        if (!ours && !headline) return;
        message.read = !ours;
        if (ours) message.team_id = managed_team_id;
        world.getInbox().add(std::move(message));
      });
  world.setStandingsProvider(
      [this](LeagueID league_id)
      {
        std::vector<TeamID> order;
        for (const StandingRow& row :
             competitions.getStandings(calendar, league_id))
          order.push_back(row.team_id);
        return order;
      });
  world.setFixtureOutlookProvider(
      [this](const GameDateValue& date, TrainingSystem::FixtureOutlook& outlook)
      {
        const auto& fixtures = calendar.getFullCalendar();
        const GameDateValue horizon = SeasonCalendar::addDays(date, 7);
        for (auto day = fixtures.lower_bound(date);
             day != fixtures.end() && !(horizon < day->first); ++day)
        {
          const auto days = static_cast<std::uint8_t>(
              dayOrdinal(day->first) - dayOrdinal(date));
          for (const Match& match : day->second)
          {
            if (match.isPlayed()) continue;
            outlook.try_emplace(match.getHomeTeamId(), days);
            outlook.try_emplace(match.getAwayTeamId(), days);
          }
        }
      });
  world.getStories().setCareerProvider(
      [this](PlayerID player_id)
      {
        CareerTotals totals;
        for (const PlayerSeasonStats& stats :
             competitions.getPlayerCareer(player_id))
        {
          totals.appearances += stats.appearances;
          totals.goals += stats.goals;
        }
        return totals;
      });
  loadGame();
}

void Game::loadGame()
{
  GameStateRepository gameStateRepo(db_conn);
  FixtureRepository fixtureRepo(db_conn);
  if (std::string game_date_str; gameStateRepo.loadGameState(
          current_season, managed_team_id, game_date_str))
  {
    currentDate = GameDateValue::fromString(game_date_str);
    fixtureRepo.loadCalendar(calendar);
    competitions.load(calendar, current_season);
    world.load(db_conn);
    transfers.load(db_conn);
    career.load(db_conn);
    guidance.load(db_conn);
    international.load(*db_conn);
    // Saves from before the continental competitions join them next
    // season unless the league phase can still be drawn.
    competitions.startContinentalSeason(currentDate);
    Logger::debug("Game loaded. Date: " + game_date_str +
                  ", Season: " + std::to_string(current_season));
  }
  else
  {
    // First run, initialize with defaults
    current_season = 1;
    managed_team_id = FREE_AGENTS_TEAM_ID;  // Or some other default
    currentDate = START_DATE;
    calendar.generate((*gamedata), currentDate);
    competitions.load(calendar, current_season);
    competitions.startContinentalSeason(currentDate);
    Logger::debug("First run, initializing game state.");
    saveGame();
  }
  // Ensure managed team is valid
  if ((*gamedata).getTeams().find(managed_team_id) ==
      (*gamedata).getTeams().end())
  {
    managed_team_id = FREE_AGENTS_TEAM_ID;
  }
  // Saves from before the board existed get it (and the day-one news) now.
  else if (managed_team_id != FREE_AGENTS_TEAM_ID &&
           world.getBoardState().team_id != managed_team_id)
  {
    world.onManagedTeamSelected(currentDate, managed_team_id,
                                upcomingFixtures(managed_team_id));
  }
  // Careers from before the manager market get a manager and AI rivals.
  if (managed_team_id != FREE_AGENTS_TEAM_ID)
    career.ensureProfile(managed_team_id, currentDate);
  career.ensureClubManagers(currentDate, managed_team_id);
}

void Game::saveGame()
{
  db_conn->beginTransaction();
  try
  {
    GameStateRepository gameStateRepo(db_conn);
    FixtureRepository fixtureRepo(db_conn);
    LeagueRepository leagueRepo(db_conn);
    PlayerRepository playerRepo(db_conn);
    TeamRepository teamRepo(db_conn);

    gameStateRepo.updateGameState(current_season, managed_team_id,
                                  currentDate.toString());
    fixtureRepo.saveCalendar(calendar);
    competitions.save();
    world.save(db_conn);
    SaveManager::fault(SaveManager::FaultPoint::MidFlush);
    transfers.save(db_conn);
    career.save(db_conn);
    guidance.save(db_conn);
    international.save(*db_conn);

    for (const auto& [id, league] : (*gamedata).getLeagues())
    {
      leagueRepo.saveLeaguePoints(league);
    }
    playerRepo.updatePlayers(gamedata->getPlayersVector());
    teamRepo.updateTeamsState(gamedata->getTeamsVector());
    db_conn->commitTransaction();
  }
  catch (const std::exception& e)
  {
    db_conn->rollbackTransaction();
    Logger::error("Failed to save game: " + std::string(e.what()));
    throw;
  }
  competitions.onSaved();
  world.onSaved();
  transfers.onSaved();
  guidance.onSaved();

  Logger::debug("Game saved.");
}

void Game::advanceDay()
{
  scheduler.resetProgress();
  currentDate.nextDay();
  Logger::debug("Date changed to: " + currentDate.toString());
  world.onDayAdvanced(currentDate, managed_team_id);
  runCareerDay();
  // National-team matches run through the same batch scheduler; windows
  // never overlap competitive club fixtures.
  international.onDay(currentDate, scheduler, world, managed_team_id);

  if (currentDate.month == 7 && currentDate.day == 1)
  {
    handleSeasonTransition();
    // Pre-contracts complete once expired contracts have been released.
    transfers.onDayAdvanced(currentDate, managed_team_id);
    return;
  }

  // A managed fixture left unplayed on its day is simulated so that the
  // competitions (tables, cup draws) never stall.
  const GameDateValue yesterday = SeasonCalendar::addDays(currentDate, -1);
  if (calendar.getFullCalendar().contains(yesterday))
  {
    simulateMatches(calendar.getMatchesForDateMutable(yesterday), true);
  }

  auto& matches_today = calendar.getMatchesForDateMutable(currentDate);
  if (!matches_today.empty())
  {
    simulateMatches(matches_today);
  }
  competitions.afterMatchday(calendar, currentDate);
  transfers.onDayAdvanced(currentDate, managed_team_id);
  keepManagedSelectionEligible();
}

void Game::keepManagedSelectionEligible()
{
  if (!getAssistantFixesLineup() || managed_team_id == FREE_AGENTS_TEAM_ID)
    return;
  // Suspensions depend on the competition of the next managed fixture.
  const auto& schedule = calendar.getFullCalendar();
  for (auto day = schedule.lower_bound(currentDate); day != schedule.end();
       ++day)
  {
    for (const Match& match : day->second)
    {
      if (match.isPlayed() || (match.getHomeTeamId() != managed_team_id &&
                               match.getAwayTeamId() != managed_team_id))
        continue;
      fixMatchdaySquad(managed_team_id, match.getMatchType(), day->first);
      return;
    }
  }
}

void Game::simulateMatches(std::vector<Match>& matches, bool include_managed)
{
  // Inputs are captured in fixture order, simulated in parallel and applied
  // back in the same order. A club plays once per batch, so this equals
  // simulating and applying the matches one by one, which is what a single
  // simulation thread does.
  std::vector<Match*> batch;
  std::vector<MatchSimulationInput> inputs;
  std::vector<TeamID> batch_teams;
  const auto flush = [&]
  {
    if (inputs.empty()) return;
    std::vector<MatchSimulationResult> results =
        scheduler.run(inputs, gamedata->getStatsConfig());
    for (std::size_t i = 0; i < batch.size(); ++i)
    {
      Match& match = *batch[i];
      const std::vector<PlayerMatchConsequence> consequences =
          std::move(results[i].consequences);
      MatchReport report = match.applySimulation(std::move(results[i]));
      for (const PlayerMatchConsequence& consequence : consequences)
        world.applyMatchConsequences(match.getDate(), consequence,
                                     managed_team_id);
      world.onMatchPlayed(match, report, managed_team_id);
      recordCareerMatch(match);
      competitions.recordResult(match, std::move(report));
    }
    batch.clear();
    inputs.clear();
    batch_teams.clear();
  };
  const bool one_by_one = scheduler.getThreadCount() <= 1;

  for (auto& match : matches)
  {
    const bool managed = match.getHomeTeamId() == managed_team_id ||
                         match.getAwayTeamId() == managed_team_id;
    if (match.isPlayed() || (managed && !include_managed))
    {
      continue;
    }
    if (std::ranges::contains(batch_teams, match.getHomeTeamId()) ||
        std::ranges::contains(batch_teams, match.getAwayTeamId()))
      flush();

    // A missed managed fixture is played by the assistant with an eligible
    // squad; the manager's own selection is restored afterwards.
    std::optional<Lineup> managed_selection;
    if (managed)
    {
      if (auto team = gamedata->getTeam(managed_team_id))
      {
        managed_selection = team->get().getLineup();
        fixMatchdaySquad(managed_team_id, match.getMatchType());
      }
    }
    const auto swaps = competitions.benchSuspendedPlayers(match);
    std::optional<MatchSimulationInput> input =
        match.prepareSimulation(*gamedata);
    competitions.restoreLineups(swaps);
    if (managed_selection)
      gamedata->getTeam(managed_team_id)->get().getLineup() =
          *managed_selection;
    if (!input) continue;
    batch.push_back(&match);
    inputs.push_back(std::move(*input));
    batch_teams.push_back(match.getHomeTeamId());
    batch_teams.push_back(match.getAwayTeamId());
    if (one_by_one) flush();
  }
  flush();
}

bool Game::setMatchResult(const GameDateValue& date, TeamID home_id,
                          TeamID away_id, uint8_t home_score,
                          uint8_t away_score)
{
  MatchReport report;
  report.home_goals = home_score;
  report.away_goals = away_score;
  return setMatchResult(date, home_id, away_id, std::move(report));
}

bool Game::setMatchResult(
    const GameDateValue& date, TeamID home_id, TeamID away_id,
    MatchReport report, std::span<const PlayerMatchConsequence> consequences)
{
  Match* match = calendar.findMatch(date, home_id, away_id);
  if (!match || match->isPlayed())
  {
    return false;
  }
  const auto home_team = gamedata->getTeam(home_id);
  const auto away_team = gamedata->getTeam(away_id);

  uint8_t home_goals = report.home_goals;
  uint8_t away_goals = report.away_goals;
  bool extra_time = report.extra_time;
  std::optional<std::pair<uint8_t, uint8_t>> shootout;
  if (report.penalties)
    shootout.emplace(report.home_penalties, report.away_penalties);
  if (match->isKnockout() && home_goals == away_goals && !shootout &&
      home_team && away_team)
  {
    const auto resolution = Competitions::resolveDrawnKnockout(
        home_team->get(), away_team->get(), gamedata->getStatsConfig(),
        match->getSeed());
    extra_time = true;
    home_goals = static_cast<uint8_t>(home_goals + resolution.home_extra_goals);
    away_goals = static_cast<uint8_t>(away_goals + resolution.away_extra_goals);
    if (resolution.penalties)
      shootout.emplace(resolution.home_penalties, resolution.away_penalties);
  }
  if (extra_time || shootout)
    match->setKnockoutResult(home_goals, away_goals, extra_time, shootout);
  else
    match->setPlayedResult(home_goals, away_goals);
  match->writeResultTo(report);

  if (report.players.empty())
  {
    if (home_team)
      report.addLineupAppearances(home_team->get().getLineup(), home_id);
    if (away_team)
      report.addLineupAppearances(away_team->get().getLineup(), away_id);
  }
  for (const PlayerMatchConsequence& consequence : consequences)
    world.applyMatchConsequences(date, consequence, managed_team_id);
  world.onMatchPlayed(*match, report, managed_team_id);
  recordCareerMatch(*match);
  competitions.recordResult(*match, std::move(report));
  competitions.afterMatchday(calendar, currentDate);
  return true;
}

bool Game::isEligible(const Player& player, MatchType type,
                      std::optional<GameDateValue> date) const
{
  return player.isAvailable() &&
         (type == MatchType::FRIENDLY ||
          !competitions.getDiscipline().isSuspended(player.getId(), type)) &&
         !international.isOnDuty(player.getId(), date.value_or(currentDate));
}

std::vector<PlayerID> Game::ineligibleSelections(TeamID team_id,
                                                 MatchType type) const
{
  const auto team = gamedata->getTeam(team_id);
  if (!team) return {};
  return MatchdaySquad::ineligible(
      team->get().getLineup(),
      [this, type](const Player& player) { return isEligible(player, type); });
}

std::size_t Game::fixMatchdaySquad(TeamID team_id, MatchType type,
                                   std::optional<GameDateValue> date)
{
  const auto team = gamedata->getTeam(team_id);
  if (!team) return 0;
  std::vector<const Player*> squad;
  for (const PlayerID player_id : team->get().getPlayerIDs())
  {
    if (const auto player = gamedata->getPlayer(player_id))
      squad.push_back(&player->get());
  }
  return MatchdaySquad::replaceIneligible(
      team->get().getLineup(), squad,
      [this, type, date](const Player& player)
      { return isEligible(player, type, date); },
      gamedata->getStatsConfig());
}

std::vector<std::pair<PlayerID, PlayerID>> Game::previewMatchdaySquadFix(
    TeamID team_id, MatchType type) const
{
  const auto team = gamedata->getTeam(team_id);
  if (!team) return {};
  std::vector<const Player*> squad;
  for (const PlayerID player_id : team->get().getPlayerIDs())
  {
    if (const auto player = gamedata->getPlayer(player_id))
      squad.push_back(&player->get());
  }
  const Lineup& current = team->get().getLineup();
  Lineup fixed = current;
  MatchdaySquad::replaceIneligible(
      fixed, squad,
      [this, type](const Player& player) { return isEligible(player, type); },
      gamedata->getStatsConfig());
  return MatchdaySquad::replacements(current, fixed);
}

void Game::endSeason()
{
  std::cout << "--- Season " << static_cast<int>(current_season)
            << " has concluded. ---"
            << "\n";
  world.onSeasonEnd(currentDate, managed_team_id);
  // Final tables are read before promotion and relegation move the clubs.
  std::unordered_map<TeamID, int> final_positions;
  for (const auto& [league_id, league] : gamedata->getLeagues())
  {
    int position = 0;
    for (const StandingRow& row :
         competitions.getStandings(calendar, league_id))
      final_positions[row.team_id] = ++position;
  }
  const BoardState board = world.getBoardState();
  competitions.closeSeason(
      calendar, current_season,
      SeasonCalendar::seasonStartYear(SeasonCalendar::addDays(currentDate, -1)));
  std::vector<SeasonHistoryEntry> finished;
  for (const SeasonHistoryEntry& entry : competitions.getSeasonHistory())
    if (entry.season == current_season) finished.push_back(entry);
  const bool managing = managed_team_id != FREE_AGENTS_TEAM_ID &&
                        board.team_id == managed_team_id;
  const bool contract_ran_out = career.onSeasonEnd(
      currentDate, managed_team_id, managing ? board.expected_position : 0,
      managing ? board.confidence : 50.0f,
      [&final_positions](TeamID team_id)
      {
        const auto found = final_positions.find(team_id);
        return found == final_positions.end() ? 0 : found->second;
      },
      finished, world.getInbox());
  if (contract_ran_out) leaveManagedTeam(DepartureReason::ContractExpired);
  (*gamedata).ageAllPlayers();
  (*gamedata).advanceContractsAndReleasePlayers();
  current_season++;
  competitions.setCurrentSeason(current_season);
}

void Game::handleSeasonTransition()
{
  endSeason();
  startNewSeason();
}

void Game::startNewSeason()
{
  for (auto& [id, league] : (*gamedata).getLeagues())
  {
    league.resetPoints();
  }
  calendar.generate((*gamedata), currentDate);
  competitions.startContinentalSeason(currentDate);
  world.onSeasonStart(currentDate, managed_team_id);
}

const GameDateValue& Game::getCurrentDate() const { return currentDate; }

const Calendar& Game::getCalendar() const { return calendar; }

Calendar& Game::getCalendar() { return calendar; }

int Game::getCurrentSeason() const { return current_season; }

uint16_t Game::getManagedTeamId() const { return managed_team_id; }

void Game::setManagedTeamId(uint16_t id)
{
  managed_team_id = id;
  if (id != FREE_AGENTS_TEAM_ID) career.ensureProfile(id, currentDate);
  world.onManagedTeamSelected(currentDate, id, upcomingFixtures(id));
}

void Game::leaveManagedTeam(DepartureReason reason)
{
  if (managed_team_id == FREE_AGENTS_TEAM_ID) return;
  career.leaveJob(reason, currentDate, world.getInbox());
  world.onManagerLeft();
  // Talks and offers were the manager's business at that club.
  std::vector<std::uint32_t> offer_ids;
  for (const IncomingOffer& offer : transfers.incomingOffers())
    offer_ids.push_back(offer.id);
  for (const std::uint32_t offer_id : offer_ids)
    transfers.removeIncomingOffer(offer_id);
  std::vector<PlayerID> talks;
  for (const auto& [player_id, talk] : transfers.talks())
    talks.push_back(player_id);
  for (const PlayerID player_id : talks) transfers.removeNegotiation(player_id);
  managed_team_id = FREE_AGENTS_TEAM_ID;
}

void Game::takeJob(TeamID team_id, const ManagerContract& contract)
{
  if (team_id == FREE_AGENTS_TEAM_ID || !gamedata->getTeam(team_id)) return;
  if (managed_team_id != FREE_AGENTS_TEAM_ID)
    leaveManagedTeam(DepartureReason::Moved);
  career.startJob(team_id, contract, currentDate);
  managed_team_id = team_id;
  world.onManagedTeamSelected(currentDate, team_id, upcomingFixtures(team_id));
}

void Game::runCareerDay()
{
  const BoardState& board = world.getBoardState();
  const bool managing = managed_team_id != FREE_AGENTS_TEAM_ID &&
                        board.team_id == managed_team_id;
  last_career_events = career.onDayAdvanced(
      currentDate, managed_team_id, managing ? board.confidence : 50.0f,
      managing ? board.expected_position : 0,
      [this](TeamID team_id) { return world.leaguePosition(team_id); },
      world.getInbox());
  if (managing && board.dismissed) leaveManagedTeam(DepartureReason::Sacked);
}

void Game::recordCareerMatch(const Match& match)
{
  if (match.getMatchType() == MatchType::FRIENDLY) return;
  const TeamID home_id = match.getHomeTeamId();
  const TeamID away_id = match.getAwayTeamId();
  const float home_strength = world.lineupStrength(home_id);
  const float away_strength = world.lineupStrength(away_id);
  career.onMatchPlayed(
      home_id, away_id, match.getHomeScore(), match.getAwayScore(),
      match.getMatchType() == MatchType::LEAGUE,
      BoardModel::expectedPoints(home_strength, away_strength, true),
      BoardModel::expectedPoints(away_strength, home_strength, false),
      managed_team_id);
}

std::vector<UpcomingFixture> Game::upcomingFixtures(TeamID team_id) const
{
  std::vector<UpcomingFixture> fixtures;
  const auto& schedule = calendar.getFullCalendar();
  for (auto day = schedule.lower_bound(currentDate); day != schedule.end();
       ++day)
  {
    for (const Match& match : day->second)
    {
      const bool home = match.getHomeTeamId() == team_id;
      if (match.isPlayed() || (!home && match.getAwayTeamId() != team_id))
        continue;
      fixtures.push_back(
          {day->first,
           home ? match.getAwayTeamId() : match.getHomeTeamId(), home,
           match.getMatchType()});
      if (match.getMatchType() != MatchType::FRIENDLY) return fixtures;
    }
  }
  return fixtures;
}
