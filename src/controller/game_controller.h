// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <random>
#include <unordered_map>
#include <vector>

#include "global/stats_config.h"
#include "model/game.h"
#include "model/league.h"
#include "model/player.h"
#include "model/team.h"
#include "model/transfer_listing.h"

/**
 * @class GameController
 * @brief Manages the core game logic, interacting with teams, leagues, and
 * players.
 *
 * This controller holds the central game state and provides methods to retrieve
 * and manipulate game data such as leagues, teams, and the managed team.
 */
class GameController
{
 public:
  struct SaveSlotMetadata
  {
    bool exists = false;
    std::string team_name = "";
    std::string game_date = "";
    std::string real_date = "";
  };

  /**
   * @brief Constructs a new GameController.
   */
  GameController();

  /**
   * @brief Starts a new game on the given save slot.
   */
  void newGame(int slot);

  /**
   * @brief Loads an existing game from the given save slot.
   * @return True if loaded successfully, false otherwise.
   */
  bool loadGame(int slot);

  /**
   * @brief Checks if a game is currently loaded.
   */
  bool isGameLoaded() const;

  /**
   * @brief Gets the current season year.
   * @return The current season as an integer.
   */
  int getCurrentSeason() const;

  /**
   * @brief Gets the current date in the game.
   * @return The current game date.
   */
  GameDateValue getCurrentDate() const;

  /**
   * @brief Checks if a team has been selected to be managed.
   * @return True if a team is selected, false otherwise.
   */
  bool hasSelectedTeam() const;

  /**
   * @brief Gets the team currently managed by the player (mutable).
   * @return An optional reference wrapper to the managed Team.
   */
  std::optional<std::reference_wrapper<Team>> getManagedTeam();

  /**
   * @brief Gets the team currently managed by the player (read-only).
   * @return An optional reference wrapper to the managed Team (const).
   */
  std::optional<std::reference_wrapper<const Team>> getManagedTeam() const;

  /**
   * @brief Selects the team for the player to manage.
   * @param team_id The ID of the team to select.
   */
  void selectManagedTeam(uint16_t team_id);

  /**
   * @brief Gets all leagues available in the game.
   * @return A vector of constant reference wrappers to the leagues.
   */
  const std::vector<std::reference_wrapper<const League>>& getLeagues() const;

  /**
   * @brief Gets all teams available in the game.
   * @return A vector of constant reference wrappers to the teams.
   */
  const std::vector<std::reference_wrapper<const Team>>& getTeams() const;

  /**
   * @brief Gets the players belonging to a specific team.
   * @param team_id The ID of the team.
   * @return A vector of constant reference wrappers to the players.
   */
  const std::vector<std::reference_wrapper<const Player>>& getPlayersForTeam(
      uint16_t team_id) const;

  /**
   * @brief Gets the teams belonging to a specific league.
   * @param league_id The ID of the league.
   * @return A vector of constant reference wrappers to the teams in the league.
   */
  std::vector<std::reference_wrapper<const Team>> getTeamsInLeague(
      uint8_t league_id) const;

  /**
   * @brief Gets a league by its ID.
   * @param league_id The ID of the league.
   * @return An optional reference wrapper to the league.
   */
  std::optional<std::reference_wrapper<const League>> getLeagueById(
      uint8_t league_id) const;

  /**
   * @brief Gets a team by its ID.
   * @param team_id The ID of the team.
   * @return An optional reference wrapper to the team.
   */
  std::optional<std::reference_wrapper<const Team>> getTeamById(
      uint16_t team_id) const;

  /**
   * @brief Gets the configuration settings for player stats.
   * @return A reference to the StatsConfig.
   */
  const StatsConfig& getStatsConfig() const;

  /**
   * @brief Advances the game date by one day.
   */
  void advanceDay();

  void setMatchResult(GameDateValue date, uint16_t home_id, uint16_t away_id,
                      uint8_t home_score, uint8_t away_score);

  /**
   * @brief Saves the current state of the game.
   */
  void saveGame();

  /**
   * @brief Gets metadata for a save slot.
   */
  SaveSlotMetadata getSaveSlotMetadata(int slot) const;

  // ========== Transfer Market: Listing ==========
  void listPlayerForTransfer(PlayerID player_id, uint32_t asking_price);
  void removePlayerFromTransfer(PlayerID player_id);
  bool isPlayerListed(PlayerID player_id) const;

  // ========== Transfer Market: Queries ==========
  const std::unordered_map<PlayerID, TransferListing>& getAllListings() const;
  std::vector<const TransferListing*> getListingsExcludingTeam(
      TeamID team_id) const;
  std::vector<const TransferListing*> getListingsByRole(PlayerRole role) const;

  // ========== Transfer Market: Buying ==========
  bool canAffordPlayer(TeamID buyer_id, PlayerID player_id,
                       uint32_t target_price) const;
  bool buyPlayer(PlayerID player_id, TeamID buyer_id, uint32_t price);
  bool signFreeAgent(PlayerID player_id, TeamID buyer_id);
  uint32_t getPlayerMarketValue(PlayerID player_id) const;

  // ========== Transfer Market: Bids & Negotiation ==========
  bool submitBid(PlayerID player_id, TeamID bidder_id, uint32_t bid_amount);
  bool acceptBid(PlayerID player_id);
  bool rejectBid(PlayerID player_id);
  bool counterOffer(PlayerID player_id, uint32_t new_price);
  bool isTransferWindowOpen() const;
  std::vector<std::pair<PlayerID, TransferListing>> getIncomingBids() const;

  // ========== AI Helpers ==========
  struct SquadNeeds
  {
    int missing_gk = 0;
    int missing_cb = 0;
    int missing_lb = 0;
    int missing_rb = 0;
    int missing_mid = 0;
    int missing_wing = 0;
    int missing_st = 0;

    int surplus_gk = 0;
    int surplus_cb = 0;
    int surplus_lb = 0;
    int surplus_rb = 0;
    int surplus_mid = 0;
    int surplus_wing = 0;
    int surplus_st = 0;

    std::optional<PlayerRole>
        upgrade_target;  // Role that needs quality upgrade
  };

  SquadNeeds evaluateSquadNeeds(TeamID team_id) const;
  float calculateAttentionScore(PlayerID player_id) const;
  float getLeagueAttentionMultiplier(LeagueID league_id) const;
  uint32_t calculateMaxPrice(PlayerID player_id, TeamID buyer_id,
                             const SquadNeeds& needs) const;
  void evaluateAndActForTeam(TeamID team_id);
  std::vector<PlayerID> findTargetsForRole(PlayerRole role, TeamID buyer_id,
                                           const SquadNeeds& needs) const;
  PlayerRole getRoleCategory(PlayerRole role) const;
  uint32_t transferBudgetForTeam(TeamID team_id) const;

  static float randomFloat(float min, float max)
  {
    static std::mt19937 gen(
        std::random_device{}());  // NOSONAR (Not used for cryptography)
    std::uniform_real_distribution<float> dis(min, max);
    return dis(gen);
  }

  const Game* getGame() const { return game.get(); }

  /** @brief Gets the database connection (for repos that need it). */
  std::shared_ptr<DatabaseConnection> getDbConn() const { return db_conn; }

  /** @brief Gets the game data cache. */
  std::shared_ptr<GameData> getGameData() const { return gamedata; }

 private:
  std::shared_ptr<class DatabaseConnection> db_conn;
  std::unique_ptr<Game> game;
  std::shared_ptr<class GameData> gamedata;

  std::unordered_map<PlayerID, TransferListing> transfer_listings;
  void executeTransfer(PlayerID pid, TeamID buyer_id, TeamID seller_id,
                       uint32_t price);
  void processAITransferActivity();

  std::string getSavePath(int slot) const;
};
