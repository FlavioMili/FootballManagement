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

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <span>
#include <sstream>

#include "database/gamedata.h"
#include "model/club_economy.h"
#include "database/repositories/player_repository.h"
#include "database/repositories/team_repository.h"
#include "global/global.h"
#include "global/logger.h"
#include "global/runtime_paths.h"
#include "model/transfer_tuning.h"
#include "model/world_rng.h"

namespace
{
// The AI transfer market draws from a stream reseeded from the world seed
// every day, so a reloaded save makes the same moves.
// Key of the daily AI market stream (other transfer streams use player ids).
constexpr std::uint64_t AI_MARKET_STREAM = 0xA1AA'4E7ULL;

std::uint32_t transferSeed(const GameData& gamedata, const GameDateValue& date)
{
  return static_cast<std::uint32_t>(
      mixHash(mixHash(gamedata.getWorldSeed(),
                      static_cast<std::uint64_t>(RngDomain::Transfers)),
              static_cast<std::uint64_t>(dayOrdinal(date))));
}
}  // namespace

GameController::GameController() : game(nullptr), gamedata(nullptr) {}

std::string GameController::getSavePath(int slot) const
{
  return RuntimePaths::savePath(slot).string();
}

void GameController::newGame(int slot, std::optional<std::uint64_t> world_seed)
{
  const auto startedAt = std::chrono::steady_clock::now();
  std::string path = getSavePath(slot);
  RuntimePaths::removeSave(slot);
  gamedata = std::make_shared<GameData>();
  if (world_seed) gamedata->setWorldSeed(*world_seed);
  db_conn = std::make_shared<DatabaseConnection>(path);
  game = std::make_unique<Game>(gamedata, db_conn);
  transfer_listings.clear();
  transfer_rng.seed(transferSeed(*gamedata, game->getCurrentDate()));

  // Seed the market: every club lists its surplus, bids for its needs and
  // offers prospects for loan. Deals start once the first day is played.
  for (const auto& team : gamedata->getTeamsVector())
  {
    const TeamID team_id = team.get().getId();
    evaluateAndActForTeam(team_id);
    game->getTransfers().listLoanProspects(team_id);
  }
  last_initialization_milliseconds =
      std::chrono::duration<float, std::milli>(
          std::chrono::steady_clock::now() - startedAt)
          .count();
  Logger::info(std::format("New game initialized in {:.2f} ms",
                           last_initialization_milliseconds));
}

bool GameController::loadGame(int slot)
{
  const auto startedAt = std::chrono::steady_clock::now();
  std::string path = getSavePath(slot);
  if (!std::filesystem::exists(path))
  {
    return false;
  }
  gamedata = std::make_shared<GameData>();
  db_conn = std::make_shared<DatabaseConnection>(path);
  game = std::make_unique<Game>(gamedata, db_conn);
  transfer_rng.seed(transferSeed(*gamedata, game->getCurrentDate()));

  // Load transfer listings
  transfer_listings.clear();
  auto loaded = gamedata->loadAllTransferListings();
  for (auto& [pid, listing] : loaded)
  {
    auto player_opt = gamedata->getPlayer(pid);
    if (player_opt.has_value())
    {
      listing.seller_team_id = player_opt->get().getTeamId();
      gamedata->getPlayers().at(pid).setTransferStatus(TransferStatus::Listed);
      listing.attention_score = calculateAttentionScore(pid);
      transfer_listings[pid] = listing;
    }
  }
  purgeStaleListings();
  if (game->getManagedTeamId() != FREE_AGENTS_TEAM_ID)
    game->getWorld().getScouting().setManagedTeam(game->getManagedTeamId());

  last_initialization_milliseconds =
      std::chrono::duration<float, std::milli>(
          std::chrono::steady_clock::now() - startedAt)
          .count();
  Logger::info(std::format("Save loaded in {:.2f} ms",
                           last_initialization_milliseconds));

  return true;
}

bool GameController::isGameLoaded() const
{
  return game != nullptr && gamedata != nullptr;
}

int GameController::getCurrentSeason() const
{
  return game ? game->getCurrentSeason() : 0;
}

GameDateValue GameController::getCurrentDate() const
{
  return game ? game->getCurrentDate() : GameDateValue{};
}

bool GameController::hasSelectedTeam() const
{
  if (!game || !gamedata) return false;
  auto managed_id = game->getManagedTeamId();
  return managed_id != FREE_AGENTS_TEAM_ID &&
         (*gamedata).getTeam(managed_id).has_value();
}

std::optional<std::reference_wrapper<Team>> GameController::getManagedTeam()
{
  if (!game || !gamedata) return std::nullopt;
  return (*gamedata).getTeam(game->getManagedTeamId());
}

std::optional<std::reference_wrapper<const Team>>
GameController::getManagedTeam() const
{
  if (!game || !gamedata) return std::nullopt;
  return (*gamedata).getTeam(game->getManagedTeamId());
}

void GameController::selectManagedTeam(uint16_t team_id)
{
  if (game && gamedata && team_id != FREE_AGENTS_TEAM_ID &&
      gamedata->getTeam(team_id).has_value())
  {
    game->setManagedTeamId(team_id);
    game->getWorld().getScouting().setManagedTeam(team_id);
    // The market was seeded before the club had a manager: whether its
    // players are transfer- or loan-listed is now the manager's call.
    TransferMarket& market = game->getTransfers();
    std::vector<PlayerID> squad;
    for (const auto& player : gamedata->getPlayersForTeam(team_id))
      squad.push_back(player.get().getId());
    for (const PlayerID player_id : squad)
    {
      market.setLoanListed(player_id, false);
      removePlayerFromTransfer(player_id);
    }
  }
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

void GameController::advanceDay()
{
  if (!game) return;
  game->resetSimulationProgress();
  continue_days_started = 0;
  continue_days_total = 1;
  simulateDay();
}

void GameController::simulateDay()
{
  // Counted just before Game::advanceDay() resets the match progress.
  ++continue_days_started;
  game->advanceDay();
  transfer_rng.seed(transferSeed(*gamedata, game->getCurrentDate()));
  purgeStaleListings();
  processAITransferActivity();
}

int GameController::advanceToNextManagedFixture(int max_days)
{
  if (!game || max_days <= 0 || !hasSelectedTeam()) return 0;

  std::optional<GameDateValue> targetDate;
  const TeamID managedTeamId = game->getManagedTeamId();
  for (const auto& [date, matches] : game->getCalendar().getFullCalendar())
  {
    if (date < game->getCurrentDate()) continue;
    const bool hasManagedFixture = std::ranges::any_of(
        matches,
        [managedTeamId](const Match& match)
        {
          return !match.isPlayed() && (match.getHomeTeamId() == managedTeamId ||
                                       match.getAwayTeamId() == managedTeamId);
        });
    if (hasManagedFixture)
    {
      targetDate = date;
      break;
    }
  }

  int advancedDays = 0;
  game->resetSimulationProgress();
  continue_days_started = 0;
  continue_days_total =
      targetDate ? std::clamp(dayOrdinal(*targetDate) -
                                  dayOrdinal(game->getCurrentDate()),
                              0, max_days)
                 : 0;
  while (targetDate && game->getCurrentDate() < *targetDate &&
         advancedDays < max_days)
  {
    simulateDay();
    ++advancedDays;
  }
  return advancedDays;
}

float GameController::ContinueProgress::fraction() const
{
  if (days_total <= 0) return 0.0f;
  float days = static_cast<float>(days_done);
  if (matches_total > 0)
    days += static_cast<float>(matches_done) / static_cast<float>(matches_total);
  return std::clamp(days / static_cast<float>(days_total), 0.0f, 1.0f);
}

GameController::ContinueProgress GameController::getContinueProgress() const
{
  ContinueProgress progress;
  progress.days_done = std::max(continue_days_started.load() - 1, 0);
  progress.days_total = continue_days_total.load();
  if (game)
  {
    const SimulationProgress matches = game->getSimulationProgress();
    progress.matches_done = matches.completed;
    progress.matches_total = matches.total;
  }
  return progress;
}

void GameController::setSimulationThreads(unsigned threads)
{
  if (game) game->setSimulationThreads(threads);
}

void GameController::purgeStaleListings()
{
  const TransferMarket& market = game->getTransfers();
  std::vector<PlayerID> staleListings;
  for (const auto& [playerId, listing] : transfer_listings)
  {
    const auto player = gamedata->getPlayer(playerId);
    if (!player || player->get().getTeamId() == FREE_AGENTS_TEAM_ID ||
        player->get().getTeamId() != listing.seller_team_id ||
        !market.canBeTraded(playerId))
      staleListings.push_back(playerId);
  }
  for (PlayerID playerId : staleListings)
  {
    transfer_listings.erase(playerId);
    gamedata->deleteTransferListing(playerId);
    if (auto player = gamedata->getPlayers().find(playerId);
        player != gamedata->getPlayers().end())
      player->second.setTransferStatus(TransferStatus::NotListed);
  }
}

bool GameController::setMatchResult(GameDateValue date, uint16_t home_id,
                                    uint16_t away_id, uint8_t home_score,
                                    uint8_t away_score)
{
  return game &&
         game->setMatchResult(date, home_id, away_id, home_score, away_score);
}

bool GameController::setMatchResult(GameDateValue date, uint16_t home_id,
                                    uint16_t away_id, const MatchEngine& engine)
{
  if (!game) return false;
  MatchReport report;
  report.fillFromEngine(engine, home_id, away_id);
  return game->setMatchResult(date, home_id, away_id, std::move(report),
                              MatchdaySquad::consequences(engine));
}

std::vector<PlayerID> GameController::getIneligibleSelections(
    TeamID team_id, MatchType type) const
{
  return game ? game->ineligibleSelections(team_id, type)
              : std::vector<PlayerID>{};
}

size_t GameController::autoFixLineup(TeamID team_id, MatchType type)
{
  return game ? game->fixMatchdaySquad(team_id, type) : 0;
}

void GameController::setAssistantFixesLineup(bool enabled)
{
  if (game) game->setAssistantFixesLineup(enabled);
}

bool GameController::getAssistantFixesLineup() const
{
  return !game || game->getAssistantFixesLineup();
}

std::vector<std::pair<PlayerID, PlayerID>> GameController::previewLineupFix(
    TeamID team_id, MatchType type) const
{
  return game ? game->previewMatchdaySquadFix(team_id, type)
              : std::vector<std::pair<PlayerID, PlayerID>>{};
}

std::vector<StandingRow> GameController::getStandings(LeagueID league_id) const
{
  if (!game) return {};
  return game->getCompetitions().getStandings(game->getCalendar(), league_id);
}

std::vector<LeagueID> GameController::getCupIds() const
{
  return game ? game->getCompetitions().getCupIds() : std::vector<LeagueID>{};
}

std::optional<Competitions::CupStatus> GameController::getCupStatus(
    LeagueID cup_id) const
{
  if (!game) return std::nullopt;
  return game->getCompetitions().getCupStatus(game->getCalendar(), cup_id);
}

std::string GameController::getCupName(LeagueID cup_id) const
{
  return gamedata ? Competitions::cupName(*gamedata, cup_id) : std::string();
}

uint8_t GameController::getLeagueTier(LeagueID league_id) const
{
  return gamedata ? Competitions::leagueTier(*gamedata, league_id) : 0;
}

std::vector<Match> GameController::getTeamFixtures(TeamID team_id) const
{
  return game ? game->getCalendar().getTeamFixtures(team_id)
              : std::vector<Match>{};
}

std::optional<MatchReport> GameController::getMatchReport(GameDateValue date,
                                                          TeamID home_id,
                                                          TeamID away_id) const
{
  if (!game) return std::nullopt;
  return game->getCompetitions().getMatchReport(date, home_id, away_id);
}

const std::vector<SeasonHistoryEntry>& GameController::getSeasonHistory() const
{
  static const std::vector<SeasonHistoryEntry> no_history;
  return game ? game->getCompetitions().getSeasonHistory() : no_history;
}

std::vector<PlayerSeasonStats> GameController::getTopScorers(
    MatchType competition_type, LeagueID competition_id, size_t limit) const
{
  if (!game) return {};
  return game->getCompetitions().getTopScorers(competition_type,
                                               competition_id, limit);
}

std::vector<PlayerSeasonStats> GameController::getPlayerSeasonStats(
    PlayerID player_id) const
{
  if (!game) return {};
  return game->getCompetitions().getPlayerSeasonStats(player_id);
}

std::vector<PlayerSeasonStats> GameController::getPlayerCareer(
    PlayerID player_id) const
{
  if (!game) return {};
  return game->getCompetitions().getPlayerCareer(player_id);
}

uint8_t GameController::getSuspensionMatches(PlayerID player_id,
                                             MatchType scope) const
{
  return game ? game->getCompetitions().getDiscipline().banMatches(player_id,
                                                                   scope)
              : 0;
}

std::vector<DisciplinaryRecord> GameController::getSuspendedPlayers(
    TeamID team_id) const
{
  if (!game) return {};
  return game->getCompetitions().getDiscipline().suspendedPlayers(team_id,
                                                                  *gamedata);
}

void GameController::saveGame()
{
  if (game) game->saveGame();
}

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
    std::tm tm_buf;
    if (const std::tm* tm = localtime_r(&tt, &tm_buf))
    {
      char buf[100];
      if (std::strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M", tm))
      {
        metadata.real_date = buf;
      }
    }
  }
  catch (const std::exception&)
  {
    metadata.real_date = "";
  }

  if (sqlite3* db = nullptr;
      sqlite3_open_v2(path.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) ==
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

        if (team_id != FREE_AGENTS_TEAM_ID)
        {
          sqlite3_stmt* team_stmt = nullptr;
          const char* sql_team = "SELECT name FROM Teams WHERE id = ?;";
          if (sqlite3_prepare_v2(db, sql_team, -1, &team_stmt, nullptr) ==
              SQLITE_OK)
          {
            sqlite3_bind_int(team_stmt, 1, team_id);
            if (sqlite3_step(team_stmt) == SQLITE_ROW)
            {
              const unsigned char* name_text =
                  sqlite3_column_text(team_stmt, 0);
              metadata.team_name =
                  name_text ? reinterpret_cast<const char*>(name_text) : "";
            }
            sqlite3_finalize(team_stmt);
          }
        }
      }
      sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
  }
  return metadata;
}

// ========== Transfer Market: Listing ==========

void GameController::listPlayerForTransfer(PlayerID pid, uint32_t asking_price)
{
  if (!isGameLoaded() || !isTransferWindowOpen() || asking_price == 0) return;
  auto player_opt = gamedata->getPlayer(pid);
  if (!player_opt.has_value()) return;

  // Don't list free agents, loanees or players committed elsewhere
  if (player_opt->get().getTeamId() == FREE_AGENTS_TEAM_ID ||
      !game->getTransfers().canBeTraded(pid))
    return;

  float attention = calculateAttentionScore(pid);
  TransferListing listing(pid, player_opt->get().getTeamId(), asking_price,
                          game->getCurrentDate(), attention);
  const auto existing_listing = transfer_listings.find(pid);
  const std::optional<TransferListing> old_listing =
      existing_listing == transfer_listings.end()
          ? std::nullopt
          : std::optional<TransferListing>(existing_listing->second);
  transfer_listings[pid] = listing;

  // Update player status bitmask
  Player& player = gamedata->getPlayers().at(pid);
  const TransferStatus old_status = player.getTransferStatus();
  player.setTransferStatus(TransferStatus::Listed);

  try
  {
    db_conn->beginTransaction();
    gamedata->saveTransferListing(listing);
    PlayerRepository(db_conn).updatePlayer(player);
    db_conn->commitTransaction();
  }
  catch (...)
  {
    db_conn->rollbackTransaction();
    player.setTransferStatus(old_status);
    if (old_listing)
      transfer_listings[pid] = *old_listing;
    else
      transfer_listings.erase(pid);
    throw;
  }
}

void GameController::removePlayerFromTransfer(PlayerID pid)
{
  auto it = transfer_listings.find(pid);
  if (it == transfer_listings.end()) return;

  const TransferListing removed_listing = it->second;
  transfer_listings.erase(it);

  // Update player status bitmask
  auto player_opt = gamedata->getPlayer(pid);
  if (!player_opt.has_value())
  {
    transfer_listings[pid] = removed_listing;
    return;
  }

  Player& player = gamedata->getPlayers().at(pid);
  const TransferStatus old_status = player.getTransferStatus();
  player.setTransferStatus(TransferStatus::NotListed);

  try
  {
    db_conn->beginTransaction();
    PlayerRepository(db_conn).updatePlayer(player);
    gamedata->deleteTransferListing(pid);
    db_conn->commitTransaction();
  }
  catch (...)
  {
    db_conn->rollbackTransaction();
    player.setTransferStatus(old_status);
    transfer_listings[pid] = removed_listing;
    throw;
  }
}

bool GameController::isPlayerListed(PlayerID pid) const
{
  return transfer_listings.contains(pid);
}

// ========== Transfer Market: Queries ==========

const std::unordered_map<PlayerID, TransferListing>&
GameController::getAllListings() const
{
  return transfer_listings;
}

std::vector<const TransferListing*> GameController::getListingsExcludingTeam(
    TeamID team_id) const
{
  std::vector<const TransferListing*> result;
  for (const auto& [pid, listing] : transfer_listings)
  {
    if (listing.seller_team_id != team_id)
    {
      result.push_back(&listing);
    }
  }
  return result;
}

std::vector<const TransferListing*> GameController::getListingsByRole(
    PlayerRole role) const
{
  std::vector<const TransferListing*> result;
  for (const auto& [pid, listing] : transfer_listings)
  {
    auto player_opt = gamedata->getPlayer(pid);
    if (player_opt.has_value() &&
        getRoleCategory(player_opt->get().getRole()) == getRoleCategory(role))
    {
      result.push_back(&listing);
    }
  }
  return result;
}

// ========== Budget Check ==========

bool GameController::canAffordPlayer(TeamID buyer_id, PlayerID pid,
                                     uint32_t target_price) const
{
  auto player_opt = gamedata->getPlayer(pid);
  if (!player_opt.has_value()) return false;
  return canAffordPlayer(buyer_id, pid, target_price,
                         player_opt->get().getWage());
}

bool GameController::canAffordPlayer(TeamID buyer_id, PlayerID pid,
                                     uint32_t target_price,
                                     uint32_t offered_weekly_wage) const
{
  auto buyer_opt = gamedata->getTeam(buyer_id);
  auto player_opt = gamedata->getPlayer(pid);
  if (!buyer_opt.has_value() || !player_opt.has_value()) return false;

  const Player& player = player_opt->get();
  const Finances& finances = buyer_opt->get().getFinances();

  if (buyer_id == player.getTeamId() ||
      transferBudgetForTeam(buyer_id) < target_price)
  {
    return false;
  }

  int64_t current_wages =
      finances.getCurrentWageSpending(*gamedata, buyer_opt->get());
  int64_t new_wage_total =
      current_wages + static_cast<int64_t>(offered_weekly_wage);

  return new_wage_total <= finances.getWageBudget();
}

GameController::ContractTerms GameController::getContractDemand(
    PlayerID pid, bool free_agent) const
{
  const auto player = gamedata->getPlayer(pid);
  if (!player) return {};

  const float wageRaise = free_agent
                              ? TransferTuning::Contract::FREE_AGENT_WAGE_RAISE
                              : TransferTuning::Contract::TRANSFER_WAGE_RAISE;
  const auto requestedWage = static_cast<uint32_t>(
      std::ceil(static_cast<float>(player->get().getWage()) * wageRaise));

  uint8_t requestedYears = TransferTuning::Contract::VETERAN_MINIMUM_YEARS;
  if (player->get().getAge() <=
      TransferTuning::Contract::YOUNG_PLAYER_MAXIMUM_AGE)
  {
    requestedYears = TransferTuning::Contract::YOUNG_PLAYER_MINIMUM_YEARS;
  }
  else if (player->get().getAge() <=
           TransferTuning::Contract::PRIME_PLAYER_MAXIMUM_AGE)
  {
    requestedYears = TransferTuning::Contract::PRIME_PLAYER_MINIMUM_YEARS;
  }

  return {
      std::max(requestedWage, TransferTuning::Contract::MINIMUM_WEEKLY_WAGE),
      requestedYears};
}

bool GameController::isContractOfferAcceptable(PlayerID pid, bool free_agent,
                                               ContractTerms terms) const
{
  if (terms.years < TransferTuning::Contract::MINIMUM_YEARS ||
      terms.years > TransferTuning::Contract::MAXIMUM_YEARS)
  {
    return false;
  }
  const ContractTerms demand = getContractDemand(pid, free_agent);
  return demand.weekly_wage > 0 && terms.weekly_wage >= demand.weekly_wage &&
         terms.years >= demand.years;
}

// ========== Core Transfer Execution ==========

bool GameController::executeTransfer(PlayerID pid, TeamID buyer_id,
                                     TeamID seller_id, uint32_t price,
                                     std::optional<ContractTerms> contract)
{
  auto buyer_opt = gamedata->getTeam(buyer_id);
  auto seller_opt = gamedata->getTeam(seller_id);
  auto player_it = gamedata->getPlayers().find(pid);
  if (!buyer_opt || !seller_opt || player_it == gamedata->getPlayers().end() ||
      buyer_id == seller_id || player_it->second.getTeamId() != seller_id ||
      !canAffordPlayer(
          buyer_id, pid, price,
          contract ? contract->weekly_wage : player_it->second.getWage()))
  {
    return false;
  }

  Team& buyer = buyer_opt->get();
  Team& seller = seller_opt->get();
  Player& player = player_it->second;
  const TransferStatus old_status = player.getTransferStatus();
  const uint32_t old_wage = player.getWage();
  const uint8_t old_contract_years = player.getContractYears();
  const bool moves_money = seller_id != FREE_AGENTS_TEAM_ID && price > 0;
  const GameDateValue today = game->getCurrentDate();

  db_conn->beginTransaction();
  try
  {
    if (moves_money)
    {
      seller.getFinances().record(today, FinanceCategory::TransferFeeIn,
                                  static_cast<int64_t>(price));
      buyer.getFinances().record(today, FinanceCategory::TransferFeeOut,
                                 -static_cast<int64_t>(price));
    }
    seller.removePlayerID(pid);
    buyer.addPlayerID(pid);
    gamedata->transferPlayer(pid, buyer_id);
    player.setTransferStatus(TransferStatus::NotListed);
    if (contract)
    {
      player.setWage(contract->weekly_wage);
      player.setContractYears(contract->years);
    }

    PlayerRepository playerRepo(db_conn);
    playerRepo.updatePlayer(player);
    TeamRepository team_repo(db_conn);
    team_repo.updateTeamState(buyer);
    if (moves_money) team_repo.updateTeamState(seller);
    gamedata->deleteTransferListing(pid);
    db_conn->commitTransaction();
  }
  catch (const std::exception& exception)
  {
    db_conn->rollbackTransaction();
    gamedata->transferPlayer(pid, seller_id);
    buyer.removePlayerID(pid);
    seller.addPlayerID(pid);
    if (moves_money)
    {
      buyer.getFinances().revertLastTransaction();
      seller.getFinances().revertLastTransaction();
    }
    player.setTransferStatus(old_status);
    player.setWage(old_wage);
    player.setContractYears(old_contract_years);
    Logger::error("Transfer failed: " + std::string(exception.what()));
    return false;
  }

  buyer.generateStartingXI(*gamedata, gamedata->getStatsConfig());
  seller.generateStartingXI(*gamedata, gamedata->getStatsConfig());
  transfer_listings.erase(pid);
  game->getWorld().onTransferCompleted(today, pid, seller_id, buyer_id, price,
                                       game->getManagedTeamId());
  game->getTransfers().recordLegacyTransfer(pid, seller_id, buyer_id, price,
                                            today);
  return true;
}

// ========== Buy + Sign + Market Value ==========

bool GameController::buyPlayer(PlayerID pid, TeamID buyer_id, uint32_t price)
{
  return buyPlayerWithContract(pid, buyer_id, price,
                               getContractDemand(pid, false));
}

bool GameController::buyPlayerWithContract(PlayerID pid, TeamID buyer_id,
                                           uint32_t price, ContractTerms terms)
{
  auto player_opt = gamedata->getPlayer(pid);
  if (!player_opt.has_value()) return false;

  TeamID seller_id = player_opt->get().getTeamId();
  if (seller_id == buyer_id) return false;
  if (!isTransferWindowOpen() || !game->getTransfers().canBeTraded(pid))
    return false;

  auto it = transfer_listings.find(pid);
  if (it == transfer_listings.end() || it->second.seller_team_id != seller_id)
  {
    return false;
  }
  else
  {
    const TransferListing& listing = it->second;
    const bool paysAskingPrice = price == listing.asking_price;
    const bool ownsHighestBid =
        listing.highest_bidder_id == buyer_id && price == listing.highest_bid;
    if (!paysAskingPrice && !ownsHighestBid)
    {
      return false;
    }
  }

  if (!isContractOfferAcceptable(pid, false, terms) ||
      !canAffordPlayer(buyer_id, pid, price, terms.weekly_wage))
  {
    return false;
  }

  return executeTransfer(pid, buyer_id, seller_id, price, terms);
}

bool GameController::signFreeAgent(PlayerID pid, TeamID buyer_id)
{
  return signFreeAgentWithContract(pid, buyer_id, getContractDemand(pid, true));
}

bool GameController::signFreeAgentWithContract(PlayerID pid, TeamID buyer_id,
                                               ContractTerms terms)
{
  auto player_opt = gamedata->getPlayer(pid);
  if (!player_opt.has_value()) return false;

  if (player_opt->get().getTeamId() != FREE_AGENTS_TEAM_ID) return false;
  if (!isContractOfferAcceptable(pid, true, terms) ||
      !canAffordPlayer(buyer_id, pid, 0, terms.weekly_wage))
  {
    return false;
  }

  return executeTransfer(pid, buyer_id, FREE_AGENTS_TEAM_ID, 0, terms);
}

uint32_t GameController::getPlayerMarketValue(PlayerID pid) const
{
  auto player_opt = gamedata->getPlayer(pid);
  if (!player_opt.has_value()) return 0;

  const Player& player = player_opt->get();
  player.updateMarketValue(gamedata->getStatsConfig());
  return player.getMarketValue();
}

// ========== Negotiation ==========

bool GameController::submitBid(PlayerID pid, TeamID bidder_id,
                               uint32_t bid_amount)
{
  auto it = transfer_listings.find(pid);
  if (it == transfer_listings.end()) return false;

  if (bidder_id == it->second.seller_team_id) return false;

  if (!isTransferWindowOpen() || !game->getTransfers().canBeTraded(pid))
    return false;

  if (!canAffordPlayer(bidder_id, pid, bid_amount)) return false;

  if (bid_amount > it->second.highest_bid)
  {
    it->second.highest_bid = bid_amount;
    it->second.highest_bidder_id = bidder_id;
    gamedata->saveTransferListing(it->second);
    game->getWorld().onTransferBid(game->getCurrentDate(), pid, bidder_id,
                                   bid_amount, game->getManagedTeamId());
    return true;
  }

  return false;
}

bool GameController::acceptBid(PlayerID pid)
{
  auto it = transfer_listings.find(pid);
  if (it == transfer_listings.end()) return false;
  if (!it->second.highest_bidder_id.has_value()) return false;
  const ContractTerms contract = getContractDemand(pid, false);
  if (!isTransferWindowOpen() ||
      !canAffordPlayer(*it->second.highest_bidder_id, pid,
                       it->second.highest_bid, contract.weekly_wage))
  {
    return false;
  }

  return executeTransfer(pid, *it->second.highest_bidder_id,
                         it->second.seller_team_id, it->second.highest_bid,
                         contract);
}

bool GameController::rejectBid(PlayerID pid)
{
  auto it = transfer_listings.find(pid);
  if (it == transfer_listings.end()) return false;

  it->second.highest_bid = 0;
  it->second.highest_bidder_id = std::nullopt;
  gamedata->saveTransferListing(it->second);
  return true;
}

bool GameController::counterOffer(PlayerID pid, uint32_t new_price)
{
  auto it = transfer_listings.find(pid);
  if (it == transfer_listings.end()) return false;
  if (new_price == 0) return false;

  it->second.asking_price = new_price;
  it->second.highest_bid = 0;
  it->second.highest_bidder_id = std::nullopt;

  gamedata->saveTransferListing(it->second);
  return true;
}

bool GameController::isTransferWindowOpen() const
{
  return game && game->getCurrentDate().isTransferWindowOpen();
}

std::vector<std::pair<PlayerID, TransferListing>>
GameController::getIncomingBids() const
{
  std::vector<std::pair<PlayerID, TransferListing>> bids;
  uint16_t managed_id = game->getManagedTeamId();

  for (const auto& [pid, listing] : transfer_listings)
  {
    if (listing.seller_team_id == managed_id &&
        listing.highest_bidder_id.has_value() && listing.highest_bid > 0)
    {
      bids.emplace_back(pid, listing);
    }
  }
  return bids;
}

// ========== AI Squad Evaluation ==========

GameController::SquadNeeds GameController::evaluateSquadNeeds(
    TeamID team_id) const
{
  SquadNeeds needs;
  const auto& team_players = gamedata->getPlayersForTeam(team_id);
  if (team_players.empty()) return needs;

  int gk = 0, cb = 0, lb = 0, rb = 0, mid = 0, wing = 0, st = 0;

  for (const auto& pref : team_players)
  {
    const Player& p = pref.get();
    // Skip players already listed for sale so we correctly identify
    // replacements
    if (isPlayerListed(p.getId())) continue;

    switch (p.getRole())
    {
      case PlayerRole::GK:
        gk++;
        break;
      case PlayerRole::CB:
        cb++;
        break;
      case PlayerRole::LB:
        lb++;
        break;
      case PlayerRole::RB:
        rb++;
        break;
      case PlayerRole::CDM:
      case PlayerRole::CM:
      case PlayerRole::CAM:
        mid++;
        break;
      case PlayerRole::LM:
      case PlayerRole::RM:
      case PlayerRole::LW:
      case PlayerRole::RW:
        wing++;
        break;
      case PlayerRole::ST:
        st++;
        break;
      default:
        break;
    }
  }

  // Target squad sizes per role group
  constexpr int TARGET_GK = 3, TARGET_CB = 5, TARGET_LB = 2, TARGET_RB = 2;
  constexpr int TARGET_MID = 6, TARGET_WING = 4, TARGET_ST = 3;

  auto calc = [](int current, int target, int& missing, int& surplus)
  {
    if (current < target)
    {
      missing = target - current;
      surplus = 0;
    }
    else
    {
      missing = 0;
      surplus = current - target;
    }
  };

  calc(gk, TARGET_GK, needs.missing_gk, needs.surplus_gk);
  calc(cb, TARGET_CB, needs.missing_cb, needs.surplus_cb);
  calc(lb, TARGET_LB, needs.missing_lb, needs.surplus_lb);
  calc(rb, TARGET_RB, needs.missing_rb, needs.surplus_rb);
  calc(mid, TARGET_MID, needs.missing_mid, needs.surplus_mid);
  calc(wing, TARGET_WING, needs.missing_wing, needs.surplus_wing);
  calc(st, TARGET_ST, needs.missing_st, needs.surplus_st);

  auto team_opt = gamedata->getTeam(team_id);
  if (team_opt.has_value() &&
      team_opt->get().getFinances().getBalance() > 10000000)
  {
    static constexpr std::array<PlayerRole, 7> CATEGORIES = {
        PlayerRole::GK, PlayerRole::CB, PlayerRole::LB, PlayerRole::RB,
        PlayerRole::CM, PlayerRole::LW, PlayerRole::ST};
    float weakestAverage = std::numeric_limits<float>::max();
    for (const PlayerRole category : CATEGORIES)
    {
      float total = 0.0f;
      int count = 0;
      for (const auto& reference : team_players)
      {
        const Player& player = reference.get();
        if (!isPlayerListed(player.getId()) &&
            getRoleCategory(player.getRole()) == category)
        {
          total +=
              static_cast<float>(player.getOverall(gamedata->getStatsConfig()));
          ++count;
        }
      }
      if (count > 0)
      {
        const float average = total / static_cast<float>(count);
        if (average < weakestAverage)
        {
          weakestAverage = average;
          needs.upgrade_target = category;
        }
      }
    }
  }

  return needs;
}

// ========== Attention Score ==========

float GameController::calculateAttentionScore(PlayerID pid) const
{
  auto player_opt = gamedata->getPlayer(pid);
  if (!player_opt.has_value()) return 0.0f;

  const Player& p = player_opt->get();
  const StatsConfig& config = gamedata->getStatsConfig();

  float score = static_cast<float>(p.getOverall(config)) / 100.0f;
  if (score <= 0.0f) return 0.0f;

  if (p.getAge() < 20)
    score *= 1.2f;
  else if (p.getAge() <= 28)
    score *= 1.0f;
  else if (p.getAge() <= 32)
    score *= 0.8f;
  else
    score *= 0.4f;

  if (p.getContractYears() <= 1)
    score *= 1.3f;
  else if (p.getContractYears() <= 2)
    score *= 1.1f;

  auto team_opt = gamedata->getTeam(p.getTeamId());
  if (team_opt.has_value())
  {
    score *= getLeagueAttentionMultiplier(team_opt->get().getLeagueId());
  }

  score *= 1.0f;

  return std::clamp(score, 0.0f, 1.5f);
}

float GameController::getLeagueAttentionMultiplier(LeagueID league_id) const
{
  switch (league_id)
  {
    case 2:
      return 1.3f;
    case 3:
      return 1.3f;
    case 1:
      return 1.2f;
    case 4:
      return 1.2f;
    case 5:
      return 1.1f;
    case 11:
      return 1.0f;
    case 10:
      return 1.0f;
    case 12:
      return 0.9f;
    default:
      return 0.8f;
  }
}

// ========== Max Price Calculation ==========

uint32_t GameController::calculateMaxPrice(PlayerID pid, TeamID buyer_id,
                                           const SquadNeeds& needs) const
{
  auto player_opt = gamedata->getPlayer(pid);
  auto buyer_opt = gamedata->getTeam(buyer_id);
  if (!player_opt.has_value() || !buyer_opt.has_value()) return 0;

  const Player& p = player_opt->get();
  uint32_t market_value = getPlayerMarketValue(pid);
  if (market_value == 0) return 0;

  auto getNeedMult = [&](PlayerRole role) -> float
  {
    switch (getRoleCategory(role))
    {
      case PlayerRole::GK:
        return needs.missing_gk >= 2 ? 2.2f
                                     : (needs.missing_gk == 1 ? 1.6f : 0.7f);
      case PlayerRole::CB:
        return needs.missing_cb >= 2 ? 2.0f
                                     : (needs.missing_cb == 1 ? 1.5f : 0.8f);
      case PlayerRole::LB:
        return needs.missing_lb >= 1 ? 1.8f : 0.7f;
      case PlayerRole::RB:
        return needs.missing_rb >= 1 ? 1.8f : 0.7f;
      case PlayerRole::CM:
        return needs.missing_mid >= 2 ? 2.0f
                                      : (needs.missing_mid == 1 ? 1.5f : 0.8f);
      case PlayerRole::LW:
        return needs.missing_wing >= 2
                   ? 2.0f
                   : (needs.missing_wing == 1 ? 1.5f : 0.8f);
      case PlayerRole::ST:
        return needs.missing_st >= 2 ? 2.2f
                                     : (needs.missing_st == 1 ? 1.6f : 0.8f);
      default:
        return 1.0f;
    }
  };
  float need_mult = getNeedMult(p.getRole());

  auto role_listings = getListingsByRole(getRoleCategory(p.getRole()));
  float scarcity_mult;
  if (role_listings.size() < 3)
    scarcity_mult = 1.5f;
  else if (role_listings.size() < 8)
    scarcity_mult = 1.1f;
  else
    scarcity_mult = 0.8f;

  const auto& team_players = gamedata->getPlayersForTeam(buyer_id);
  float team_avg = 0.0f;
  int count = 0;
  for (const auto& ref : team_players)
  {
    team_avg +=
        static_cast<float>(ref.get().getOverall(gamedata->getStatsConfig()));
    count++;
  }
  team_avg = count > 0 ? team_avg / static_cast<float>(count) : 50.0f;

  float player_ovr =
      static_cast<float>(p.getOverall(gamedata->getStatsConfig()));
  float diff = player_ovr - team_avg;

  float prestige_mult;
  if (diff > 15.0f)
    prestige_mult = 1.5f;
  else if (diff > 5.0f)
    prestige_mult = 1.2f;
  else if (diff > -5.0f)
    prestige_mult = 1.0f;
  else if (diff > -15.0f)
    prestige_mult = 0.8f;
  else
    prestige_mult = 0.6f;

  uint32_t budget = transferBudgetForTeam(buyer_id);
  const double valuation =
      static_cast<double>(market_value) * static_cast<double>(need_mult) *
      static_cast<double>(scarcity_mult) * static_cast<double>(prestige_mult);
  return static_cast<uint32_t>(
      std::min<double>(valuation, static_cast<double>(budget)));
}

// ========== Find Targets ==========

std::vector<PlayerID> GameController::findTargetsForRole(
    PlayerRole role, TeamID buyer_id, const SquadNeeds& /*needs*/) const
{
  std::vector<std::pair<PlayerID, float>> candidates;
  PlayerRole broad_cat = getRoleCategory(role);

  for (const auto& [pid, listing] : transfer_listings)
  {
    if (listing.seller_team_id == buyer_id ||
        !game->getTransfers().canBeTraded(pid))
      continue;

    auto player_opt = gamedata->getPlayer(pid);
    if (!player_opt.has_value()) continue;

    const Player& p = player_opt->get();

    if (getRoleCategory(p.getRole()) != broad_cat) continue;

    float score = listing.attention_score;

    if (p.getRole() == role) score *= 1.2f;

    if (canAffordPlayer(buyer_id, pid, listing.asking_price))
    {
      candidates.emplace_back(pid, score);
    }
  }

  const auto& free_agents = gamedata->getPlayersForTeam(FREE_AGENTS_TEAM_ID);
  for (const auto& ref : free_agents)
  {
    const Player& p = ref.get();
    if (getRoleCategory(p.getRole()) != broad_cat) continue;

    float score = calculateAttentionScore(p.getId());
    if (p.getRole() == role) score *= 1.2f;

    if (canAffordPlayer(buyer_id, p.getId(), 0))
    {
      candidates.emplace_back(p.getId(), score);
    }
  }

  std::ranges::sort(candidates, [](const auto& a, const auto& b)
                    { return a.second > b.second; });

  std::vector<PlayerID> result;
  for (const auto& [pid, score] : candidates)
  {
    result.push_back(pid);
  }
  return result;
}

// ========== AI Team Activity ==========

void GameController::evaluateAndActForTeam(TeamID team_id)
{
  SquadNeeds needs = evaluateSquadNeeds(team_id);
  const uint32_t budget = transferBudgetForTeam(team_id);

  auto tryBuy = [&](PlayerRole role, int priority) -> bool
  {
    if (priority <= 0 || budget == 0) return false;
    const auto targets = findTargetsForRole(role, team_id, needs);
    // The best few targets are sounded out; players who would refuse the
    // club (stature, playing time) are skipped.
    constexpr size_t SOUNDED_TARGETS = 5;
    const TransferMarket& market = game->getTransfers();
    const auto chosen = std::ranges::find_if(
        targets.begin(),
        targets.begin() + static_cast<std::ptrdiff_t>(
                              std::min(targets.size(), SOUNDED_TARGETS)),
        [&](PlayerID candidate)
        {
          const auto player = gamedata->getPlayer(candidate);
          return player &&
                 market.wouldJoin(
                     candidate, team_id,
                     player->get().getTeamId() == FREE_AGENTS_TEAM_ID
                         ? TransferNegotiation::ContractKind::FreeAgent
                         : TransferNegotiation::ContractKind::Transfer);
        });
    if (chosen == targets.end() ||
        chosen - targets.begin() >=
            static_cast<std::ptrdiff_t>(SOUNDED_TARGETS))
      return false;

    const PlayerID target = *chosen;
    auto player_opt = gamedata->getPlayer(target);
    if (!player_opt.has_value()) return false;

    if (player_opt->get().getTeamId() == FREE_AGENTS_TEAM_ID)
    {
      return signFreeAgent(target, team_id);
    }

    const uint32_t max_price = calculateMaxPrice(target, team_id, needs);

    const uint32_t marketValue = getPlayerMarketValue(target);
    const auto initialBid = static_cast<uint32_t>(
        static_cast<float>(marketValue) * randomFloat(0.75f, 1.15f));
    const uint32_t bid = std::min(initialBid, max_price);

    if (bid > 0 && bid <= budget)
    {
      return submitBid(target, team_id, bid);
    }
    return false;
  };

  const std::array<std::pair<PlayerRole, int>, 7> shortages = {{
      {PlayerRole::GK, needs.missing_gk},
      {PlayerRole::CB, needs.missing_cb},
      {PlayerRole::LB, needs.missing_lb},
      {PlayerRole::RB, needs.missing_rb},
      {PlayerRole::CM, needs.missing_mid},
      {PlayerRole::LW, needs.missing_wing},
      {PlayerRole::ST, needs.missing_st},
  }};
  for (const auto& [role, missing] : shortages)
  {
    if (tryBuy(role, missing)) return;
  }

  if (needs.upgrade_target && !transfer_listings.empty() &&
      randomFloat(0.0f, 1.0f) < 0.45f && tryBuy(*needs.upgrade_target, 1))
  {
    return;
  }

  auto trySell = [&](PlayerRole role, int surplus_count) -> bool
  {
    if (surplus_count <= 0) return false;

    const auto& team_players = gamedata->getPlayersForTeam(team_id);
    std::vector<PlayerID> candidates;
    for (const auto& ref : team_players)
    {
      const Player& p = ref.get();
      if (getRoleCategory(p.getRole()) == role && !isPlayerListed(p.getId()))
      {
        candidates.push_back(p.getId());
      }
    }

    if (candidates.empty()) return false;

    std::ranges::sort(
        candidates,
        [&](PlayerID a, PlayerID b)
        {
          auto pa = gamedata->getPlayer(a);
          auto pb = gamedata->getPlayer(b);
          if (!pa.has_value() || !pb.has_value()) return false;
          return pa->get().getOverall(gamedata->getStatsConfig()) <
                 pb->get().getOverall(gamedata->getStatsConfig());
        });

    const PlayerID toSell = candidates.front();
    const auto asking =
        static_cast<uint32_t>(static_cast<float>(getPlayerMarketValue(toSell)) *
                              randomFloat(0.8f, 1.3f));
    if (asking > 0)
    {
      listPlayerForTransfer(toSell, asking);
      return isPlayerListed(toSell);
    }
    return false;
  };

  const std::array<std::pair<PlayerRole, int>, 7> surpluses = {{
      {PlayerRole::GK, needs.surplus_gk},
      {PlayerRole::CB, needs.surplus_cb},
      {PlayerRole::LB, needs.surplus_lb},
      {PlayerRole::RB, needs.surplus_rb},
      {PlayerRole::CM, needs.surplus_mid},
      {PlayerRole::LW, needs.surplus_wing},
      {PlayerRole::ST, needs.surplus_st},
  }};
  for (const auto& [role, surplus] : surpluses)
  {
    if (trySell(role, surplus)) return;
  }

  if (needs.upgrade_target) tryBuy(*needs.upgrade_target, 1);
}

// ========== Helpers ==========

PlayerRole GameController::getRoleCategory(PlayerRole role) const
{
  using enum PlayerRole;
  switch (role)
  {
    case GK:
      return GK;
    case CB:
      return CB;
    case LB:
      return LB;
    case RB:
      return RB;
    case CDM:
    case CM:
    case CAM:
      return CM;
    case LM:
    case RM:
    case LW:
    case RW:
      return LW;
    case ST:
      return ST;
    default:
      return UNKNOWN;
  }
}

uint32_t GameController::transferBudgetForTeam(TeamID team_id) const
{
  auto team_opt = gamedata->getTeam(team_id);
  if (!team_opt.has_value()) return 0;

  const Finances& finances = team_opt->get().getFinances();
  if (finances.getBalance() <= 0 ||
      (game && game->getWorld().isTransferEmbargoed(team_id)))
    return 0;

  // The board's allowance, never more than the cash left after a payroll
  // reserve and the instalments still due this season.
  const int64_t committed = game ? game->getTransfers().committedPayables(
                                       team_id, game->getCurrentDate())
                                 : 0;
  const int64_t budget = ClubEconomy::availableTransferBudget(
      finances.getTransferBudget(), finances.getBalance(),
      getWeeklyWageBill(team_id), committed);
  return static_cast<uint32_t>(
      std::min<int64_t>(budget, std::numeric_limits<uint32_t>::max()));
}

void GameController::evaluateIncomingAIBids()
{
  auto managed_team_opt = game->getManagedTeamId();
  std::vector<PlayerID> to_accept;
  std::vector<PlayerID> to_reject;

  for (const auto& [pid, listing] : transfer_listings)
  {
    if (listing.seller_team_id == FREE_AGENTS_TEAM_ID ||
        listing.seller_team_id == managed_team_opt)
    {
      continue;
    }

    if (listing.highest_bidder_id.has_value() && listing.highest_bid > 0)
    {
      if (listing.highest_bid >= listing.asking_price)
      {
        to_accept.push_back(pid);
      }
      else if (listing.highest_bid >=
               static_cast<uint32_t>(static_cast<float>(listing.asking_price) *
                                     0.85f))
      {
        if (randomFloat(0.0f, 1.0f) < 0.30f)
        {
          to_accept.push_back(pid);
        }
        else
        {
          to_reject.push_back(pid);
        }
      }
      else
      {
        to_reject.push_back(pid);
      }
    }
  }

  std::ranges::sort(to_accept);
  for (PlayerID pid : to_accept)
  {
    // A deal the buyer can no longer fund falls through.
    if (!completeAiSale(pid)) rejectBid(pid);
  }
  for (PlayerID pid : to_reject)
  {
    rejectBid(pid);
  }
}

bool GameController::completeAiSale(PlayerID pid)
{
  const auto it = transfer_listings.find(pid);
  const auto player = gamedata->getPlayer(pid);
  if (it == transfer_listings.end() || !it->second.highest_bidder_id ||
      !player || player->get().getTeamId() != it->second.seller_team_id ||
      !isTransferWindowOpen())
    return false;
  TransferMarket& market = game->getTransfers();
  const TeamID buyer_id = *it->second.highest_bidder_id;
  TransferMarket::Deal deal;
  deal.player_id = pid;
  deal.buyer_id = buyer_id;
  deal.kind = TransferKind::Permanent;
  deal.terms = TransferNegotiation::aiOfferTerms(it->second.highest_bid);
  deal.contract = TransferNegotiation::demandedOffer(
      TransferNegotiation::contractDemand(market.playerContext(
          pid, buyer_id, TransferNegotiation::ContractKind::Transfer)));
  if (!canPayDeal(deal) ||
      !market.completeTransfer(deal, game->getCurrentDate(),
                               game->getManagedTeamId()))
    return false;
  transfer_listings.erase(pid);
  return true;
}

uint32_t GameController::listingPrice(PlayerID pid) const
{
  const auto listing = transfer_listings.find(pid);
  const auto player = gamedata->getPlayer(pid);
  if (listing == transfer_listings.end() || !player ||
      listing->second.seller_team_id != player->get().getTeamId())
    return 0;
  return listing->second.asking_price;
}

bool GameController::canPayDeal(const TransferMarket::Deal& deal) const
{
  const auto buyer = gamedata->getTeam(deal.buyer_id);
  const auto player = gamedata->getPlayer(deal.player_id);
  if (!buyer || !player ||
      (game && game->getWorld().isTransferEmbargoed(deal.buyer_id)))
    return false;
  const Finances& finances = buyer->get().getFinances();
  const auto upfront =
      static_cast<int64_t>(TransferNegotiation::upfrontAmount(deal.terms));
  const auto agent = static_cast<int64_t>(TransferMarket::agentFee(deal));
  const auto bonus = static_cast<int64_t>(deal.contract.signing_bonus);
  const int64_t payroll = getWeeklyWageBill(deal.buyer_id) +
                          static_cast<int64_t>(deal.contract.weekly_wage);
  // Fees come out of the board's transfer allowance; a free signing only
  // needs the cash for the agent and the bonus.
  const bool cash_ok =
      deal.terms.fee > 0
          ? upfront + agent <= transferBudgetForTeam(deal.buyer_id) &&
                bonus <= finances.getBalance()
          : agent + bonus <= finances.getBalance();
  return cash_ok && payroll <= finances.getWageBudget();
}

void GameController::processAITransferActivity()
{
  using Market = TransferTuning::Market;
  const GameDateValue today = game->getCurrentDate();
  const TeamID managed = game->getManagedTeamId();
  TransferMarket& market = game->getTransfers();
  const TransferNegotiation::WindowInfo window =
      TransferNegotiation::windowInfo(today);
  WorldRng rng = WorldRng::stream(
      gamedata->getWorldSeed(), RngDomain::Transfers,
      static_cast<std::uint64_t>(dayOrdinal(today)), AI_MARKET_STREAM);

  std::vector<TeamID> clubs;
  clubs.reserve(gamedata->getTeamsVector().size());
  for (const auto& team : gamedata->getTeamsVector())
  {
    const TeamID id = team.get().getId();
    if (id != FREE_AGENTS_TEAM_ID && id != managed) clubs.push_back(id);
  }
  std::ranges::sort(clubs);
  rng.shuffle(std::span<TeamID>(clubs));

  market.runAiPreContracts(today, managed, rng, Market::DAILY_PRE_CONTRACTS);
  if (!window.open)
  {
    // Out of the windows only free agents can be registered.
    int signings = 0;
    const size_t visits =
        std::min<size_t>(clubs.size(), Market::BASE_TEAM_EVALUATIONS);
    for (size_t index = 0;
         index < visits && signings < Market::CLOSED_WINDOW_FREE_SIGNINGS;
         ++index)
    {
      if (market.runAiClub(clubs[index], transfer_listings, today, managed, rng,
                           true))
        ++signings;
    }
    return;
  }

  evaluateIncomingAIBids();
  const float weight = TransferNegotiation::activityWeight(window);
  const auto visits = std::min<size_t>(
      clubs.size(),
      static_cast<size_t>(std::lround(
          static_cast<float>(Market::BASE_TEAM_EVALUATIONS) * weight)));
  const auto max_moves = static_cast<int>(
      std::lround(static_cast<float>(Market::BASE_DAILY_DEALS) * weight));
  int moves = 0;
  for (size_t index = 0; index < visits && moves < max_moves; ++index)
  {
    const TeamID club = clubs[index];
    market.runAiApproach(club, today, managed, rng);
    if (market.runAiClub(club, transfer_listings, today, managed, rng, false))
      ++moves;
    else
      evaluateAndActForTeam(club);
  }
}

// ========== Transfer Market: structured deals ==========

TransferNegotiation::WindowInfo GameController::getTransferWindow() const
{
  return game ? TransferNegotiation::windowInfo(game->getCurrentDate())
              : TransferNegotiation::WindowInfo{};
}

TransferNegotiation::ClubResponse GameController::makeTransferOffer(
    PlayerID player_id, const TransferNegotiation::OfferTerms& terms)
{
  using TransferNegotiation::ClubResponse;
  using TransferNegotiation::Reason;
  ClubResponse refusal;
  if (!game || !hasSelectedTeam()) return refusal;
  const TeamID managed = game->getManagedTeamId();
  const auto player = gamedata->getPlayer(player_id);
  TransferMarket& market = game->getTransfers();
  if (!isTransferWindowOpen())
  {
    refusal.reasons.push_back(Reason::WindowClosed);
    return refusal;
  }
  if (game->getWorld().isTransferEmbargoed(managed))
  {
    refusal.reasons.push_back(Reason::Embargo);
    return refusal;
  }
  if (!player || player->get().getTeamId() == managed ||
      player->get().getTeamId() == FREE_AGENTS_TEAM_ID ||
      !market.canBeTraded(player_id))
  {
    refusal.reasons.push_back(Reason::Unavailable);
    return refusal;
  }
  const TeamID seller = player->get().getTeamId();
  Negotiation talk;
  if (const Negotiation* existing = market.findNegotiation(player_id);
      existing && existing->seller == seller &&
      existing->kind == TransferNegotiation::ContractKind::Transfer)
    talk = *existing;
  talk.player_id = player_id;
  talk.seller = seller;
  talk.kind = TransferNegotiation::ContractKind::Transfer;

  TransferMarket::Deal probe;
  probe.player_id = player_id;
  probe.buyer_id = managed;
  probe.terms = terms;
  const int64_t signing_cash =
      static_cast<int64_t>(TransferNegotiation::upfrontAmount(terms)) +
      static_cast<int64_t>(TransferMarket::agentFee(probe));
  if (signing_cash > transferBudgetForTeam(managed))
  {
    refusal.reasons.push_back(Reason::OverBudget);
    return refusal;
  }

  const GameDateValue today = game->getCurrentDate();
  ClubResponse response = TransferNegotiation::evaluateOffer(
      market.saleContext(player_id, managed, today, listingPrice(player_id)),
      terms, talk.club_rounds);
  talk.expires =
      today + static_cast<size_t>(TransferTuning::Offer::AGREEMENT_VALID_DAYS);
  if (response.decision == ClubResponse::Decision::Accept)
  {
    talk.agreed = terms;
    talk.club_agreed = true;
  }
  else
  {
    ++talk.club_rounds;
  }
  market.setNegotiation(talk);
  game->getWorld().onTransferBid(today, player_id, managed, terms.fee, managed);
  return response;
}

std::optional<TransferNegotiation::ContractKind>
GameController::getContractTalkKind(PlayerID player_id) const
{
  if (!game || !hasSelectedTeam()) return std::nullopt;
  const auto player = gamedata->getPlayer(player_id);
  const TeamID managed = game->getManagedTeamId();
  if (!player || player->get().getTeamId() == managed) return std::nullopt;
  const TransferMarket& market = game->getTransfers();
  if (player->get().getTeamId() == FREE_AGENTS_TEAM_ID)
    return TransferNegotiation::ContractKind::FreeAgent;
  if (const Negotiation* talk = market.findNegotiation(player_id);
      talk && talk->club_agreed && isTransferWindowOpen() &&
      talk->seller == player->get().getTeamId())
    return TransferNegotiation::ContractKind::Transfer;
  const LoanDeal* loan = market.findLoan(player_id);
  if (!market.findPreContract(player_id) &&
      (!loan || loan->parent != managed) &&
      TransferNegotiation::canSignPreContract(player->get().getContractYears(),
                                              game->getCurrentDate()))
    return TransferNegotiation::ContractKind::PreContract;
  return std::nullopt;
}

TransferNegotiation::ContractDemand GameController::getPlayerDemand(
    PlayerID player_id, TransferNegotiation::ContractKind kind) const
{
  if (!game) return {};
  const TransferMarket& market = game->getTransfers();
  const auto context =
      market.playerContext(player_id, game->getManagedTeamId(), kind);
  TransferNegotiation::ContractDemand demand =
      TransferNegotiation::contractDemand(context);
  // The agent's ask softens with every proposal already turned down.
  if (const Negotiation* talk = market.findNegotiation(player_id))
    demand.asking_wage =
        TransferNegotiation::agentAsk(context, demand, talk->player_rounds);
  return demand;
}

std::uint8_t GameController::getContractRoundsLeft(PlayerID player_id) const
{
  constexpr std::uint8_t MAX_ROUNDS =
      TransferTuning::Negotiation::MAX_PLAYER_ROUNDS;
  if (!game) return 0;
  const Negotiation* talk = game->getTransfers().findNegotiation(player_id);
  const std::uint8_t used = talk ? talk->player_rounds : 0;
  return used >= MAX_ROUNDS ? 0 : static_cast<std::uint8_t>(MAX_ROUNDS - used);
}

GameController::ContractTalkResult GameController::proposeContract(
    PlayerID player_id, const TransferNegotiation::ContractOffer& offer)
{
  using TransferNegotiation::ContractKind;
  ContractTalkResult result;
  const auto kind = getContractTalkKind(player_id);
  if (!kind) return result;
  const TeamID managed = game->getManagedTeamId();
  const GameDateValue today = game->getCurrentDate();
  TransferMarket& market = game->getTransfers();
  const auto player = gamedata->getPlayer(player_id);

  Negotiation talk;
  if (const Negotiation* existing = market.findNegotiation(player_id))
    talk = *existing;
  talk.player_id = player_id;
  talk.seller = player->get().getTeamId();
  if (*kind != ContractKind::Transfer) talk.kind = *kind;
  if (getContractRoundsLeft(player_id) == 0)
  {
    result.response.reasons.push_back(TransferNegotiation::Reason::TalksEnded);
    return result;
  }

  result.response = TransferNegotiation::evaluateContract(
      market.playerContext(player_id, managed, *kind), offer,
      talk.player_rounds);
  if (!result.response.accepted)
  {
    ++talk.player_rounds;
    talk.expires = std::max(
        talk.expires, today + static_cast<size_t>(
                                  TransferTuning::Offer::AGREEMENT_VALID_DAYS));
    market.setNegotiation(talk);
    result.rounds_left = getContractRoundsLeft(player_id);
    return result;
  }

  TransferMarket::Deal deal;
  deal.player_id = player_id;
  deal.buyer_id = managed;
  deal.contract = offer;
  deal.kind = *kind == ContractKind::Transfer ? TransferKind::Permanent
                                              : TransferKind::Free;
  if (*kind == ContractKind::Transfer) deal.terms = talk.agreed;
  if (*kind != ContractKind::PreContract && !canPayDeal(deal))
  {
    result.over_budget = true;
    result.rounds_left = getContractRoundsLeft(player_id);
    return result;
  }
  result.completed =
      *kind == ContractKind::PreContract
          ? market.agreePreContract(player_id, managed, offer, today, managed)
          : market.completeTransfer(deal, today, managed);
  if (result.completed)
  {
    market.removeNegotiation(player_id);
    purgeStaleListings();
  }
  return result;
}

TransferNegotiation::ClubResponse GameController::makeLoanOffer(
    PlayerID player_id, const TransferNegotiation::LoanTerms& terms)
{
  using TransferNegotiation::ClubResponse;
  using TransferNegotiation::Reason;
  ClubResponse response;
  if (!game || !hasSelectedTeam()) return response;
  if (!isTransferWindowOpen())
  {
    response.reasons.push_back(Reason::WindowClosed);
    return response;
  }
  const TeamID managed = game->getManagedTeamId();
  if (game->getWorld().isTransferEmbargoed(managed))
  {
    response.reasons.push_back(Reason::Embargo);
    return response;
  }
  const auto player = gamedata->getPlayer(player_id);
  TransferMarket& market = game->getTransfers();
  if (!player || player->get().getTeamId() == managed ||
      player->get().getTeamId() == FREE_AGENTS_TEAM_ID ||
      !market.canBeTraded(player_id))
  {
    response.reasons.push_back(Reason::Unavailable);
    return response;
  }
  const Player& loanee = player->get();
  const TeamID parent = loanee.getTeamId();
  const GameDateValue today = game->getCurrentDate();
  loanee.updateMarketValue(gamedata->getStatsConfig());
  TransferNegotiation::LoanContext context;
  context.market_value = loanee.getMarketValue();
  context.weekly_wage = loanee.getWage();
  context.age = loanee.getAge();
  context.role = game->getWorld().squadRole(player_id);
  context.loan_listed = market.isLoanListed(player_id);
  context.weeks = TransferNegotiation::weeksBetween(
      today, TransferNegotiation::loanEndDate(today, terms.duration));
  context.within_limits = market.loanWithinLimits(player_id, parent, managed);
  response = TransferNegotiation::evaluateLoan(context, terms);
  if (response.decision != ClubResponse::Decision::Accept) return response;

  const auto parent_team = gamedata->getTeam(parent);
  const auto managed_team = getManagedTeam();
  const int gap =
      parent_team->get().getReputation() - managed_team->get().getReputation();
  if (!TransferNegotiation::playerAcceptsLoan(
          context.role, market.projectedRole(player_id, managed),
          loanee.getTraits().ambition, gap, response.reasons))
  {
    response.decision = ClubResponse::Decision::Reject;
    return response;
  }
  const int64_t borrower_wage =
      static_cast<int64_t>(loanee.getWage()) * terms.wage_share / 100;
  if (static_cast<int64_t>(terms.loan_fee) > transferBudgetForTeam(managed) ||
      getWeeklyWageBill(managed) + borrower_wage >
          managed_team->get().getFinances().getWageBudget())
  {
    response.decision = ClubResponse::Decision::Reject;
    response.reasons.push_back(Reason::OverBudget);
    return response;
  }
  if (!market.startLoan(player_id, managed, terms, today, managed))
  {
    response.decision = ClubResponse::Decision::Reject;
    response.reasons.push_back(Reason::LoanLimit);
    return response;
  }
  purgeStaleListings();
  return response;
}

bool GameController::setLoanListed(PlayerID player_id, bool listed)
{
  if (!game || !hasSelectedTeam()) return false;
  const auto player = gamedata->getPlayer(player_id);
  TransferMarket& market = game->getTransfers();
  if (!player || player->get().getTeamId() != game->getManagedTeamId() ||
      !market.canBeTraded(player_id))
    return false;
  market.setLoanListed(player_id, listed);
  return true;
}

bool GameController::recallLoan(PlayerID player_id)
{
  if (!game || !isTransferWindowOpen()) return false;
  TransferMarket& market = game->getTransfers();
  const LoanDeal* loan = market.findLoan(player_id);
  if (!loan || loan->parent != game->getManagedTeamId() || !loan->recall_clause)
    return false;
  return market.endLoan(player_id, game->getCurrentDate(),
                        game->getManagedTeamId(), true);
}

bool GameController::exerciseLoanOption(PlayerID player_id)
{
  if (!game) return false;
  TransferMarket& market = game->getTransfers();
  const LoanDeal* loan = market.findLoan(player_id);
  const TeamID managed = game->getManagedTeamId();
  if (!loan || loan->borrower != managed || loan->option_fee == 0) return false;
  TransferMarket::Deal probe;
  probe.player_id = player_id;
  probe.buyer_id = managed;
  probe.terms = TransferNegotiation::aiOfferTerms(loan->option_fee);
  const int64_t signing_cash =
      static_cast<int64_t>(TransferNegotiation::upfrontAmount(probe.terms)) +
      static_cast<int64_t>(TransferMarket::agentFee(probe));
  if (signing_cash > transferBudgetForTeam(managed)) return false;
  return market.exerciseLoanOption(player_id, game->getCurrentDate(), managed);
}

const std::vector<IncomingOffer>& GameController::getIncomingOffers() const
{
  static const std::vector<IncomingOffer> EMPTY;
  return game ? game->getTransfers().incomingOffers() : EMPTY;
}

bool GameController::acceptIncomingOffer(std::uint32_t offer_id)
{
  if (!game || !isTransferWindowOpen()) return false;
  TransferMarket& market = game->getTransfers();
  const IncomingOffer* found = market.findIncomingOffer(offer_id);
  if (!found) return false;
  const IncomingOffer offer = *found;
  const TeamID managed = game->getManagedTeamId();
  const GameDateValue today = game->getCurrentDate();
  const auto player = gamedata->getPlayer(offer.player_id);
  if (!player || player->get().getTeamId() != managed ||
      !market.canBeTraded(offer.player_id))
  {
    market.removeIncomingOffer(offer_id);
    return false;
  }
  bool completed = false;
  if (offer.loan)
  {
    completed = market.startLoan(offer.player_id, offer.buyer, offer.loan_terms,
                                 today, managed);
  }
  else
  {
    const auto context =
        market.playerContext(offer.player_id, offer.buyer,
                             TransferNegotiation::ContractKind::Transfer);
    TransferMarket::Deal deal;
    deal.player_id = offer.player_id;
    deal.buyer_id = offer.buyer;
    deal.terms = offer.terms;
    deal.contract = TransferNegotiation::demandedOffer(
        TransferNegotiation::contractDemand(context));
    // The bidder may have spent its budget since making the offer.
    completed =
        canPayDeal(deal) && market.completeTransfer(deal, today, managed);
  }
  market.removeIncomingOffer(offer_id);
  if (completed) purgeStaleListings();
  return completed;
}

bool GameController::rejectIncomingOffer(std::uint32_t offer_id)
{
  return game && game->getTransfers().removeIncomingOffer(offer_id);
}

TransferNegotiation::ClubResponse GameController::counterIncomingOffer(
    std::uint32_t offer_id, uint32_t fee)
{
  using TransferNegotiation::ClubResponse;
  using TransferNegotiation::Reason;
  constexpr std::uint8_t BUYER_PATIENCE = 2;
  constexpr double FEE_ROUNDING = 10'000.0;
  ClubResponse response;
  if (!game) return response;
  TransferMarket& market = game->getTransfers();
  const IncomingOffer* found = market.findIncomingOffer(offer_id);
  if (!found || found->loan || fee == 0) return response;
  IncomingOffer offer = *found;
  if (fee <= offer.max_fee)
  {
    offer.terms = TransferNegotiation::aiOfferTerms(fee);
    market.updateIncomingOffer(offer);
    if (acceptIncomingOffer(offer_id))
    {
      response.decision = ClubResponse::Decision::Accept;
      response.reasons.push_back(Reason::OfferAccepted);
    }
    return response;
  }
  if (offer.round >= BUYER_PATIENCE)
  {
    market.removeIncomingOffer(offer_id);
    response.reasons.push_back(Reason::TalksBroken);
    return response;
  }
  // The bidder meets the seller halfway, up to its ceiling.
  const double halfway = 0.5 * (static_cast<double>(offer.terms.fee) + fee);
  const auto improved = static_cast<uint32_t>(
      std::min(static_cast<double>(offer.max_fee),
               std::round(halfway / FEE_ROUNDING) * FEE_ROUNDING));
  offer.terms =
      TransferNegotiation::aiOfferTerms(std::max(improved, offer.terms.fee));
  ++offer.round;
  market.updateIncomingOffer(offer);
  response.decision = ClubResponse::Decision::Counter;
  response.counter_fee = offer.terms.fee;
  response.reasons.push_back(Reason::CounterOffer);
  return response;
}

int64_t GameController::getReleaseCost(PlayerID player_id) const
{
  if (!game) return 0;
  const auto player = gamedata->getPlayer(player_id);
  if (!player || player->get().getTeamId() != game->getManagedTeamId())
    return 0;
  return TransferNegotiation::severancePay(player->get().getWage(),
                                           player->get().getContractYears(),
                                           game->getCurrentDate());
}

bool GameController::releasePlayer(PlayerID player_id)
{
  if (!game || !hasSelectedTeam()) return false;
  const auto player = gamedata->getPlayer(player_id);
  const TeamID managed = game->getManagedTeamId();
  if (!player || player->get().getTeamId() != managed) return false;
  const bool released = game->getTransfers().releasePlayer(
      player_id, game->getCurrentDate(), managed);
  if (released) purgeStaleListings();
  return released;
}

// ========== World simulation ==========

const std::vector<InboxMessage>& GameController::getInbox() const
{
  static const std::vector<InboxMessage> EMPTY;
  return game ? game->getWorld().getInbox().getMessages() : EMPTY;
}

bool GameController::markInboxMessageRead(uint32_t message_id)
{
  return game && game->getWorld().getInbox().markRead(message_id);
}

void GameController::markAllInboxMessagesRead()
{
  if (game) game->getWorld().getInbox().markAllRead();
}

size_t GameController::getUnreadInboxCount() const
{
  return game ? game->getWorld().getInbox().unreadCount() : 0;
}

bool GameController::isTransferEmbargoed() const
{
  return game && game->getWorld().isTransferEmbargoed(game->getManagedTeamId());
}

const BoardState& GameController::getBoardState() const
{
  static const BoardState EMPTY;
  return game ? game->getWorld().getBoardState() : EMPTY;
}

const std::vector<FinanceTransaction>& GameController::getFinanceLedger(
    TeamID team_id) const
{
  static const std::vector<FinanceTransaction> EMPTY;
  if (!gamedata) return EMPTY;
  const auto team = gamedata->getTeam(team_id);
  return team ? team->get().getFinances().getLedger() : EMPTY;
}

FinanceSummary GameController::getFinanceSummary(TeamID team_id,
                                                 GameDateValue from,
                                                 GameDateValue to) const
{
  if (!gamedata) return {};
  const auto team = gamedata->getTeam(team_id);
  return team ? team->get().getFinances().summarize(from, to)
              : FinanceSummary{};
}

int64_t GameController::getWeeklyWageBill(TeamID team_id) const
{
  if (!gamedata) return 0;
  const auto team = gamedata->getTeam(team_id);
  return team ? team->get().getFinances().getCurrentWageSpending(*gamedata,
                                                                 team->get())
              : 0;
}

bool GameController::moveTransferToWageBudget(int64_t transfer_amount)
{
  const auto team = getManagedTeam();
  if (!team) return false;
  return team->get().getFinances().moveTransferToWageBudget(
      transfer_amount, getWeeklyWageBill(team->get().getId()));
}

bool GameController::setTicketPrice(uint32_t price)
{
  constexpr uint32_t MAX_TICKET_PRICE = 1000;
  const auto team = getManagedTeam();
  if (!team || price == 0 || price > MAX_TICKET_PRICE) return false;
  ClubProfile profile = team->get().getProfile();
  profile.ticket_price = price;
  team->get().setProfile(profile);
  return true;
}

uint32_t GameController::getFairTicketPrice(TeamID team_id) const
{
  if (!gamedata) return 0;
  const auto team = gamedata->getTeam(team_id);
  if (!team) return 0;
  const auto economies = buildLeagueEconomies(*gamedata);
  const auto economy = economies.find(team->get().getLeagueId());
  if (economy == economies.end()) return 0;
  return static_cast<uint32_t>(std::lround(
      ClubEconomy::fairTicketPrice(economy->second, team->get().getProfile())));
}

uint32_t GameController::getLastHomeAttendance(TeamID team_id) const
{
  return game ? game->getWorld().lastHomeAttendance(team_id) : 0;
}

bool GameController::isPlayerAvailable(PlayerID player_id) const
{
  if (!gamedata) return false;
  const auto player = gamedata->getPlayer(player_id);
  return player && player->get().isAvailable();
}

std::vector<PlayerID> GameController::getInjuredPlayers(TeamID team_id) const
{
  std::vector<PlayerID> injured;
  if (!gamedata) return injured;
  for (const auto& player : gamedata->getPlayersForTeam(team_id))
  {
    if (!player.get().isAvailable()) injured.push_back(player.get().getId());
  }
  return injured;
}

std::vector<PlayerID> GameController::getUnavailableLineupPlayers(
    TeamID team_id) const
{
  std::vector<PlayerID> unavailable;
  if (!gamedata) return unavailable;
  const auto team = gamedata->getTeam(team_id);
  if (!team) return unavailable;
  const Lineup& lineup = team->get().getLineup();
  const auto check = [&](const Player* player)
  {
    if (player && !player->isAvailable())
      unavailable.push_back(player->getId());
  };
  check(lineup.getGoalkeeper());
  for (const auto& positioned : lineup.getOutfieldPlayers())
    check(positioned.player);
  for (const Player* reserve : lineup.getReserves()) check(reserve);
  return unavailable;
}

SquadRole GameController::getSquadRole(PlayerID player_id) const
{
  return game ? game->getWorld().squadRole(player_id) : SquadRole::Fringe;
}

GameController::PotentialEstimate GameController::getPotentialEstimate(
    PlayerID player_id) const
{
  const auto row = getScoutedRow(player_id);
  if (!row) return {};
  return {row->potential_low, row->potential_high};
}

bool GameController::applyMatchConsequences(PlayerID player_id,
                                            uint8_t minutes_played,
                                            float end_condition, bool injured)
{
  return game && game->getWorld().applyMatchConsequences(
                     game->getCurrentDate(),
                     {player_id, minutes_played, end_condition, injured},
                     game->getManagedTeamId());
}

bool GameController::renewContract(PlayerID player_id, ContractTerms terms)
{
  const auto team = getManagedTeam();
  if (!team || !gamedata) return false;
  auto& players = gamedata->getPlayers();
  const auto found = players.find(player_id);
  if (found == players.end() ||
      found->second.getTeamId() != team->get().getId() ||
      !game->getTransfers().canBeTraded(player_id) ||
      !isContractOfferAcceptable(player_id, false, terms))
    return false;
  Player& player = found->second;
  const int64_t payroll = getWeeklyWageBill(team->get().getId()) -
                          player.getWage() + terms.weekly_wage;
  if (payroll > team->get().getFinances().getWageBudget()) return false;
  player.setWage(terms.weekly_wage);
  player.setContractYears(terms.years);
  return true;
}

std::optional<ScoutedPlayerView> GameController::getScoutedView(
    PlayerID player_id) const
{
  if (!game) return std::nullopt;
  return game->getWorld().getScouting().view(player_id);
}

std::optional<ScoutedPlayerRow> GameController::getScoutedRow(
    PlayerID player_id) const
{
  if (!game) return std::nullopt;
  return game->getWorld().getScouting().row(player_id);
}

std::vector<ScoutedPlayerRow> GameController::searchScoutedPlayers(
    const ScoutSearchFilter& filter) const
{
  if (!game) return {};
  return game->getWorld().getScouting().search(filter);
}

float GameController::getScoutingKnowledge(PlayerID player_id) const
{
  return game ? game->getWorld().getScouting().knowledgeOf(player_id) : 0.0f;
}

const std::vector<ScoutProfile>& GameController::getScouts() const
{
  static const std::vector<ScoutProfile> EMPTY;
  return game ? game->getWorld().getScouting().getScouts() : EMPTY;
}

const std::vector<ScoutAssignment>& GameController::getScoutAssignments() const
{
  static const std::vector<ScoutAssignment> EMPTY;
  return game ? game->getWorld().getScouting().getAssignments() : EMPTY;
}

int64_t GameController::getScoutAssignmentCost(ScoutTargetKind kind,
                                               uint32_t target_id,
                                               uint16_t days) const
{
  return game ? game->getWorld().getScouting().assignmentCost(kind, target_id,
                                                              days)
              : 0;
}

ScoutAssignError GameController::startScoutAssignment(uint32_t scout_id,
                                                      ScoutTargetKind kind,
                                                      uint32_t target_id,
                                                      uint16_t days)
{
  if (!game) return ScoutAssignError::NoTeam;
  return game->getWorld().getScouting().startAssignment(
      game->getCurrentDate(), scout_id, kind, target_id, days);
}

bool GameController::cancelScoutAssignment(uint32_t assignment_id)
{
  return game &&
         game->getWorld().getScouting().cancelAssignment(assignment_id);
}

const std::vector<ScoutReport>& GameController::getScoutReports() const
{
  static const std::vector<ScoutReport> EMPTY;
  return game ? game->getWorld().getScouting().getReports() : EMPTY;
}

const std::vector<RecruitmentFocus>& GameController::getRecruitmentFocuses()
    const
{
  static const std::vector<RecruitmentFocus> EMPTY;
  return game ? game->getWorld().getScouting().getFocuses() : EMPTY;
}

uint32_t GameController::saveRecruitmentFocus(const RecruitmentFocus& focus)
{
  return game ? game->getWorld().getScouting().upsertFocus(focus) : 0;
}

bool GameController::removeRecruitmentFocus(uint32_t focus_id)
{
  return game && game->getWorld().getScouting().removeFocus(focus_id);
}

const std::vector<ShortlistEntry>& GameController::getShortlist() const
{
  static const std::vector<ShortlistEntry> EMPTY;
  return game ? game->getWorld().getScouting().getShortlist() : EMPTY;
}

bool GameController::isShortlisted(PlayerID player_id) const
{
  return game && game->getWorld().getScouting().isShortlisted(player_id);
}

bool GameController::addToShortlist(PlayerID player_id)
{
  return game && game->getWorld().getScouting().addToShortlist(
                     game->getCurrentDate(), player_id);
}

bool GameController::removeFromShortlist(PlayerID player_id)
{
  return game && game->getWorld().getScouting().removeFromShortlist(player_id);
}

std::vector<SquadComparisonRow> GameController::compareShortlistWithSquad()
    const
{
  if (!game) return {};
  return game->getWorld().getScouting().compareWithSquad();
}

uint64_t GameController::getWorldSeed() const
{
  return gamedata ? gamedata->getWorldSeed() : 0;
}

// ========== Training ==========

std::optional<std::reference_wrapper<Team>> GameController::managedClub()
{
  if (!hasSelectedTeam()) return std::nullopt;
  return getManagedTeam();
}

std::optional<std::reference_wrapper<const Team>> GameController::managedClub()
    const
{
  if (!hasSelectedTeam()) return std::nullopt;
  return getManagedTeam();
}

const TeamTrainingPlan* GameController::getTrainingPlan() const
{
  const auto team = managedClub();
  if (!team || !gamedata) return nullptr;
  return gamedata->getTraining().findPlan(team->get().getId());
}

bool GameController::setTrainingPreset(TrainingPreset preset)
{
  const auto team = managedClub();
  if (!team || !gamedata || preset == TrainingPreset::COUNT) return false;
  TeamTrainingPlan& plan = gamedata->getTraining().plan(team->get().getId());
  plan.preset = preset;
  if (preset != TrainingPreset::Custom)
    plan.slots = TrainingModel::presetMicrocycle(preset);
  return true;
}

bool GameController::setTrainingSlot(MicrocycleDay day, TrainingSlot slot)
{
  const auto team = managedClub();
  if (!team || !gamedata || day == MicrocycleDay::COUNT ||
      slot.session == SessionType::COUNT ||
      slot.intensity == TrainingIntensity::COUNT)
    return false;
  TeamTrainingPlan& plan = gamedata->getTraining().plan(team->get().getId());
  TrainingSlot& current = plan.slots[static_cast<size_t>(day)];
  if (current == slot) return true;
  current = slot;
  plan.preset = TrainingPreset::Custom;
  return true;
}

bool GameController::setTrainingIntensity(TrainingIntensity intensity)
{
  const auto team = managedClub();
  if (!team || !gamedata || intensity == TrainingIntensity::COUNT) return false;
  gamedata->getTraining().plan(team->get().getId()).intensity = intensity;
  return true;
}

bool GameController::setCongestionAutoAdjust(bool enabled)
{
  const auto team = managedClub();
  if (!team || !gamedata) return false;
  gamedata->getTraining().plan(team->get().getId()).auto_congestion = enabled;
  return true;
}

bool GameController::setPlayerTrainingFocus(PlayerID player_id,
                                            TrainingFocus focus)
{
  const auto team = managedClub();
  if (!team || !gamedata || focus == TrainingFocus::COUNT) return false;
  const auto player = gamedata->getPlayer(player_id);
  if (!player || player->get().getTeamId() != team->get().getId())
    return false;
  gamedata->getTraining().player(player_id).focus = focus;
  return true;
}

GameController::PlayerWorkload GameController::getPlayerWorkload(
    PlayerID player_id) const
{
  PlayerWorkload workload;
  if (!gamedata) return workload;
  const auto player = gamedata->getPlayer(player_id);
  const PlayerTrainingState* state =
      gamedata->getTraining().findPlayer(player_id);
  if (!player || !state) return workload;
  workload.focus = state->focus;
  workload.acute = state->acute;
  workload.chronic = state->chronic;
  workload.ratio = TrainingModel::workloadRatio(*state);
  workload.risk = TrainingModel::workloadRisk(
      *state, player->get().getDynamics().condition);
  workload.trend = state->trend;
  return workload;
}

std::vector<GameController::TrainingDayPreview>
GameController::getTrainingWeekPreview() const
{
  std::vector<TrainingDayPreview> preview;
  const TeamTrainingPlan* plan = getTrainingPlan();
  if (!plan || !game) return preview;
  const TeamID team_id = game->getManagedTeamId();
  const auto& fixtures = game->getCalendar().getFullCalendar();
  const GameDateValue today = game->getCurrentDate();
  const auto matchOn = [&](const GameDateValue& date) -> const Match*
  {
    const auto found = fixtures.find(date);
    if (found == fixtures.end()) return nullptr;
    for (const Match& match : found->second)
    {
      if (match.getHomeTeamId() == team_id || match.getAwayTeamId() == team_id)
        return &match;
    }
    return nullptr;
  };

  constexpr int DAYS = 7;
  constexpr int LOOK_AHEAD = 14;
  std::array<const Match*, LOOK_AHEAD> matches{};
  for (int offset = 0; offset < LOOK_AHEAD; ++offset)
    matches[static_cast<size_t>(offset)] =
        matchOn(SeasonCalendar::addDays(today, offset));
  std::int32_t last_match_day = plan->last_match_day;
  for (int offset = 0; offset < DAYS; ++offset)
  {
    const GameDateValue date = SeasonCalendar::addDays(today, offset);
    const std::int32_t ordinal = dayOrdinal(date);
    int next = -1;
    for (int ahead = offset; ahead < LOOK_AHEAD && ahead <= offset + 7; ++ahead)
    {
      if (matches[static_cast<size_t>(ahead)])
      {
        next = ahead - offset;
        break;
      }
    }
    const int since = last_match_day > 0 ? ordinal - last_match_day : -1;
    TrainingDayPreview day;
    day.date = date;
    if (date.month == 6)
    {
      day.day.slot = {SessionType::Rest, TrainingIntensity::Low};
    }
    else
    {
      day.day = TrainingModel::scheduleDay(*plan, since, next, ordinal);
    }
    if (const Match* match = matches[static_cast<size_t>(offset)])
    {
      day.day.match_day = true;
      day.home = match->getHomeTeamId() == team_id;
      day.opponent = day.home ? match->getAwayTeamId() : match->getHomeTeamId();
      last_match_day = ordinal;
    }
    preview.push_back(day);
  }
  return preview;
}

float GameController::getTacticalFamiliarity(TeamID team_id) const
{
  return gamedata ? TrainingSystem::tacticalFamiliarity(*gamedata, team_id)
                  : 0.0f;
}

std::vector<TrainingAdvice> GameController::getTrainingAdvice() const
{
  const auto team = managedClub();
  if (!team || !gamedata) return {};
  int matches_next_week = 0;
  for (const TrainingDayPreview& day : getTrainingWeekPreview())
    matches_next_week += day.day.match_day ? 1 : 0;
  return TrainingModel::advise(TrainingSystem::adviceInputs(
      *gamedata, team->get().getId(), matches_next_week));
}

// ========== Staff ==========

std::vector<const StaffMember*> GameController::getStaff(TeamID team_id) const
{
  return gamedata ? gamedata->getStaff().clubStaff(team_id)
                  : std::vector<const StaffMember*>{};
}

std::vector<const StaffMember*> GameController::getStaffMarket() const
{
  return gamedata ? gamedata->getStaff().market()
                  : std::vector<const StaffMember*>{};
}

StaffEffects GameController::getStaffEffects(TeamID team_id) const
{
  return gamedata ? gamedata->getStaff().effects(team_id) : StaffEffects{};
}

float GameController::getScoutingAccuracy(TeamID team_id) const
{
  return gamedata ? gamedata->getStaff().effects(team_id).scouting_accuracy
                  : 0.0f;
}

uint32_t GameController::getStaffWageDemand(StaffID staff_id) const
{
  if (!gamedata) return 0;
  const StaffMember* member = gamedata->getStaff().find(staff_id);
  if (!member) return 0;
  return member->team_id == FREE_AGENTS_TEAM_ID
             ? StaffModel::marketWage(*member)
             : StaffModel::renewalWage(*member);
}

GameController::StaffActionResult GameController::hireStaff(StaffID staff_id,
                                                            uint8_t years)
{
  const auto team = managedClub();
  if (!team || !gamedata) return StaffActionResult::NoClub;
  StaffRoster& roster = gamedata->getStaff();
  StaffMember* member = roster.find(staff_id);
  if (!member) return StaffActionResult::UnknownStaff;
  if (member->team_id != FREE_AGENTS_TEAM_ID)
    return StaffActionResult::NotAvailable;
  if (years < 1 || years > 4) return StaffActionResult::InvalidTerms;
  const TeamID team_id = team->get().getId();
  if (roster.roleCount(team_id, member->role) >=
      StaffModel::ROLE_LIMITS[static_cast<size_t>(member->role)])
    return StaffActionResult::RoleFull;
  const uint32_t wage = StaffModel::marketWage(*member);
  if (team->get().getFinances().getBalance() < 12 * static_cast<int64_t>(wage))
    return StaffActionResult::CannotAfford;
  member->team_id = team_id;
  member->wage = wage;
  member->contract_years = years;
  roster.invalidate();
  return StaffActionResult::Ok;
}

GameController::StaffActionResult GameController::releaseStaff(
    StaffID staff_id)
{
  const auto team = managedClub();
  if (!team || !gamedata || !game) return StaffActionResult::NoClub;
  StaffRoster& roster = gamedata->getStaff();
  StaffMember* member = roster.find(staff_id);
  if (!member) return StaffActionResult::UnknownStaff;
  if (member->team_id != team->get().getId())
    return StaffActionResult::NotAvailable;
  const int64_t severance = StaffModel::severance(*member);
  Finances& finances = team->get().getFinances();
  if (finances.getBalance() < severance) return StaffActionResult::CannotAfford;
  finances.record(game->getCurrentDate(), FinanceCategory::Staff, -severance);
  member->team_id = FREE_AGENTS_TEAM_ID;
  member->contract_years = 0;
  member->wage = StaffModel::marketWage(*member);
  roster.invalidate();
  return StaffActionResult::Ok;
}

GameController::StaffActionResult GameController::extendStaffContract(
    StaffID staff_id, uint8_t years)
{
  const auto team = managedClub();
  if (!team || !gamedata) return StaffActionResult::NoClub;
  StaffRoster& roster = gamedata->getStaff();
  StaffMember* member = roster.find(staff_id);
  if (!member) return StaffActionResult::UnknownStaff;
  if (member->team_id != team->get().getId())
    return StaffActionResult::NotAvailable;
  if (years <= member->contract_years || years < 2 || years > 5)
    return StaffActionResult::InvalidTerms;
  member->wage = StaffModel::renewalWage(*member);
  member->contract_years = years;
  roster.invalidate();
  return StaffActionResult::Ok;
}
