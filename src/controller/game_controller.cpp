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
#include <sstream>

#include "database/gamedata.h"
#include "database/repositories/player_repository.h"
#include "database/repositories/team_repository.h"
#include "global/global.h"
#include "global/logger.h"
#include "model/transfer_tuning.h"

GameController::GameController()
    : game(nullptr), gamedata(nullptr), transfer_rng(std::random_device{}())
{
}

std::string GameController::getSavePath(int slot) const
{
  char* prefPath = SDL_GetPrefPath("FlavioMili", "FootballManagement");
  if (!prefPath)
  {
    return "save_" + std::to_string(slot) + ".db";
  }
  std::filesystem::path p(prefPath);
  SDL_free(prefPath);

  std::filesystem::create_directories(p);
  p /= ("save_" + std::to_string(slot) + ".db");
  return p.string();
}

void GameController::newGame(int slot)
{
  const auto startedAt = std::chrono::steady_clock::now();
  std::string path = getSavePath(slot);
  if (std::filesystem::exists(path))
  {
    std::filesystem::remove(path);
  }
  gamedata = std::make_shared<GameData>();
  db_conn = std::make_shared<DatabaseConnection>(path);
  game = std::make_unique<Game>(gamedata, db_conn);
  transfer_listings.clear();

  // Seed the transfer market with some initial listings
  for (int i = 0; i < 7; ++i)
  {
    processAITransferActivity();
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
  const int previousSeason = game->getCurrentSeason();
  game->advanceDay();
  if (game->getCurrentSeason() != previousSeason)
  {
    purgeReleasedPlayerListings();
  }
  if (isTransferWindowOpen())
  {
    processAITransferActivity();
  }
}

void GameController::purgeReleasedPlayerListings()
{
  std::vector<PlayerID> releasedListings;
  releasedListings.reserve(transfer_listings.size());
  for (const auto& [playerId, listing] : transfer_listings)
  {
    (void)listing;
    const auto player = gamedata->getPlayer(playerId);
    if (!player || player->get().getTeamId() == FREE_AGENTS_TEAM_ID)
      releasedListings.push_back(playerId);
  }
  for (PlayerID playerId : releasedListings)
  {
    transfer_listings.erase(playerId);
    gamedata->deleteTransferListing(playerId);
  }
}

bool GameController::setMatchResult(GameDateValue date, uint16_t home_id,
                                    uint16_t away_id, uint8_t home_score,
                                    uint8_t away_score)
{
  return game &&
         game->setMatchResult(date, home_id, away_id, home_score, away_score);
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

  // Don't list free agents
  if (player_opt->get().getTeamId() == FREE_AGENTS_TEAM_ID) return;

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

  const auto wage_budget = std::max(
      TransferTuning::Contract::MINIMUM_WEEKLY_WAGE_BUDGET,
      static_cast<int64_t>(
          static_cast<double>(finances.getBalance()) *
          (1.0 - static_cast<double>(finances.getTransferToWagesRatio())) /
          TransferTuning::Contract::WEEKS_PER_YEAR));
  return new_wage_total <= wage_budget;
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

  db_conn->beginTransaction();
  try
  {
    if (moves_money)
    {
      seller.getFinances().addBalance(static_cast<int64_t>(price));
      buyer.getFinances().subtractBalance(static_cast<int64_t>(price));
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
      buyer.getFinances().addBalance(static_cast<int64_t>(price));
      seller.getFinances().subtractBalance(static_cast<int64_t>(price));
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
  if (!isTransferWindowOpen()) return false;

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

  if (!isTransferWindowOpen()) return false;

  if (!canAffordPlayer(bidder_id, pid, bid_amount)) return false;

  if (bid_amount > it->second.highest_bid)
  {
    it->second.highest_bid = bid_amount;
    it->second.highest_bidder_id = bidder_id;
    gamedata->saveTransferListing(it->second);
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
    if (listing.seller_team_id == buyer_id) continue;

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
    auto targets = findTargetsForRole(role, team_id, needs);
    if (targets.empty()) return false;

    const PlayerID target = targets.front();
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
  int64_t balance = finances.getBalance();

  if (balance <= 0) return 0;

  float ratio = finances.getTransferToWagesRatio();
  const double budget = static_cast<double>(balance) *
                        std::clamp(static_cast<double>(ratio), 0.0, 1.0);
  return static_cast<uint32_t>(std::min<double>(
      budget, static_cast<double>(std::numeric_limits<uint32_t>::max())));
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

  for (PlayerID pid : to_accept)
  {
    acceptBid(pid);
  }
  for (PlayerID pid : to_reject)
  {
    rejectBid(pid);
  }
}

void GameController::processAITransferActivity()
{
  evaluateIncomingAIBids();

  for (const auto& [team_id, team] : gamedata->getTeams())
  {
    if (auto managed_team_opt = game->getManagedTeamId();
        team_id == FREE_AGENTS_TEAM_ID || team_id == managed_team_opt)
    {
      continue;
    }
    if (randomFloat(0.0f, 1.0f) < 0.15f)
    {
      evaluateAndActForTeam(team_id);
    }
  }
}
