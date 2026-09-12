// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <random>
#include <unordered_map>
#include <vector>

#include "global/stats_config.h"
#include "model/competition.h"
#include "model/game.h"
#include "model/league.h"
#include "model/player.h"
#include "model/staff.h"
#include "model/team.h"
#include "model/training.h"
#include "model/transfer_listing.h"

class MatchEngine;

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
  struct ContractTerms
  {
    uint32_t weekly_wage = 0;
    uint8_t years = 0;
  };

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
   * @param world_seed Seed of the generated world and of every random stream
   * of the world simulation. Without it FM_WORLD_SEED or the clock is used.
   */
  void newGame(int slot, std::optional<std::uint64_t> world_seed = std::nullopt);

  /**
   * @brief Loads an existing game from the given save slot.
   * @return True if loaded successfully, false otherwise.
   */
  bool loadGame(int slot);

  /** Duration of the most recent load/new-game initialization. */
  float getLastInitializationMilliseconds() const
  {
    return last_initialization_milliseconds;
  }

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

  /** Advances quiet days and stops on the next managed fixture. */
  int advanceToNextManagedFixture(int max_days = 60);

  /** Progress of the running advanceDay() / advanceToNextManagedFixture(). */
  struct ContinueProgress
  {
    /** Days before the one being simulated, out of the days to advance. */
    int days_done = 0;
    int days_total = 0;
    /** Matches simulated / scheduled so far on the day being simulated. */
    uint32_t matches_done = 0;
    uint32_t matches_total = 0;

    /** Overall completion in [0, 1]; a day counts once its matches ran. */
    float fraction() const;
  };
  /**
   * Thread-safe snapshot for a progress bar: poll it from the UI thread while
   * Continue runs on a worker thread.
   */
  ContinueProgress getContinueProgress() const;

  /** Threads simulating a matchday in the current game (1 = sequential). */
  void setSimulationThreads(unsigned threads);

  bool setMatchResult(GameDateValue date, uint16_t home_id, uint16_t away_id,
                      uint8_t home_score, uint8_t away_score);

  /** Records a managed match with the structured summary of the live engine
   * (team stats, per-player lines and the measured condition and injuries,
   * applied like for simulated matches). Drawn cup ties go to extra time and
   * penalties. */
  bool setMatchResult(GameDateValue date, uint16_t home_id, uint16_t away_id,
                      const MatchEngine& engine);

  /** Selected players (XI and reserves) injured or suspended for @p type. */
  std::vector<PlayerID> getIneligibleSelections(TeamID team_id,
                                                MatchType type) const;
  /** Replaces those players with eligible squad members (same role first);
   * returns how many were replaced. */
  size_t autoFixLineup(TeamID team_id, MatchType type);
  /** What autoFixLineup() would change: (replaced, replacement or 0 when
   * the player just leaves the matchday squad). */
  std::vector<std::pair<PlayerID, PlayerID>> previewLineupFix(
      TeamID team_id, MatchType type) const;

  // ========== Competitions ==========
  /** Fixture-derived table: points, GD, goals, head-to-head, name, ID. */
  std::vector<StandingRow> getStandings(LeagueID league_id) const;
  /** Domestic cup IDs: the top league ID of each country. */
  std::vector<LeagueID> getCupIds() const;
  std::optional<Competitions::CupStatus> getCupStatus(LeagueID cup_id) const;
  std::string getCupName(LeagueID cup_id) const;
  /** 1 = top division, 2 = second division, ... */
  uint8_t getLeagueTier(LeagueID league_id) const;
  /** All fixtures (league, cup, friendly) of a team this season by date. */
  std::vector<Match> getTeamFixtures(TeamID team_id) const;
  std::optional<MatchReport> getMatchReport(GameDateValue date, TeamID home_id,
                                            TeamID away_id) const;
  const std::vector<SeasonHistoryEntry>& getSeasonHistory() const;
  std::vector<PlayerSeasonStats> getTopScorers(MatchType competition_type,
                                               LeagueID competition_id,
                                               size_t limit = 10) const;
  std::vector<PlayerSeasonStats> getPlayerSeasonStats(PlayerID player_id) const;
  std::vector<PlayerSeasonStats> getPlayerCareer(PlayerID player_id) const;
  /** Matches still to serve in league or cup (0 = eligible). */
  uint8_t getSuspensionMatches(PlayerID player_id, MatchType scope) const;
  /** Players of a team with an outstanding suspension. */
  std::vector<DisciplinaryRecord> getSuspendedPlayers(TeamID team_id) const;

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
  bool canAffordPlayer(TeamID buyer_id, PlayerID player_id,
                       uint32_t target_price,
                       uint32_t offered_weekly_wage) const;
  bool buyPlayer(PlayerID player_id, TeamID buyer_id, uint32_t price);
  bool buyPlayerWithContract(PlayerID player_id, TeamID buyer_id,
                             uint32_t price, ContractTerms terms);
  bool signFreeAgent(PlayerID player_id, TeamID buyer_id);
  bool signFreeAgentWithContract(PlayerID player_id, TeamID buyer_id,
                                 ContractTerms terms);
  ContractTerms getContractDemand(PlayerID player_id, bool free_agent) const;
  bool isContractOfferAcceptable(PlayerID player_id, bool free_agent,
                                 ContractTerms terms) const;
  uint32_t getPlayerMarketValue(PlayerID player_id) const;

  // ========== Transfer Market: Bids & Negotiation ==========
  bool submitBid(PlayerID player_id, TeamID bidder_id, uint32_t bid_amount);
  bool acceptBid(PlayerID player_id);
  bool rejectBid(PlayerID player_id);
  bool counterOffer(PlayerID player_id, uint32_t new_price);
  bool isTransferWindowOpen() const;
  std::vector<std::pair<PlayerID, TransferListing>> getIncomingBids() const;

  // ========== Transfer Market: structured deals ==========
  /** Transfer window on the current date (deadline countdown). */
  TransferNegotiation::WindowInfo getTransferWindow() const;
  /**
   * Offer of the managed club for another club's player. An accepted fee
   * opens contract talks with the player for a week.
   */
  TransferNegotiation::ClubResponse makeTransferOffer(
      PlayerID player_id, const TransferNegotiation::OfferTerms& terms);
  /** Kind of contract talks the managed club can hold with a player now:
   * agreed transfer, free agent or pre-contract (final six months). */
  std::optional<TransferNegotiation::ContractKind> getContractTalkKind(
      PlayerID player_id) const;
  /** The player's demands for talks of @p kind with the managed club;
   * asking_wage is the agent's current ask in these talks. */
  TransferNegotiation::ContractDemand getPlayerDemand(
      PlayerID player_id, TransferNegotiation::ContractKind kind) const;

  struct ContractTalkResult
  {
    TransferNegotiation::ContractResponse response;
    bool completed = false;      /*!< Moved, or pre-contract signed. */
    bool over_budget = false;    /*!< Agreed but the club cannot pay. */
    std::uint8_t rounds_left = 0;
  };
  /** Proposes personal terms; completes the move when the player agrees. */
  ContractTalkResult proposeContract(
      PlayerID player_id, const TransferNegotiation::ContractOffer& offer);
  /** Proposals the player will still consider. */
  std::uint8_t getContractRoundsLeft(PlayerID player_id) const;

  /** Loan offer for another club's player; starts the loan on agreement. */
  TransferNegotiation::ClubResponse makeLoanOffer(
      PlayerID player_id, const TransferNegotiation::LoanTerms& terms);
  /** Offers (or withdraws) a managed player for loan. */
  bool setLoanListed(PlayerID player_id, bool listed);
  /** Recalls a loaned-out player (recall clause, open window). */
  bool recallLoan(PlayerID player_id);
  /** Buys a player on loan at the managed club for the option fee. */
  bool exerciseLoanOption(PlayerID player_id);

  /** AI offers for the managed club's players (loans included). */
  const std::vector<IncomingOffer>& getIncomingOffers() const;
  bool acceptIncomingOffer(std::uint32_t offer_id);
  bool rejectIncomingOffer(std::uint32_t offer_id);
  /** Asks the bidder for @p fee: accepted (sale completes), improved
   * (Counter with the new fee) or withdrawn (Reject). */
  TransferNegotiation::ClubResponse counterIncomingOffer(std::uint32_t offer_id,
                                                         uint32_t fee);

  /** Remaining wages owed if a managed player is released today. */
  int64_t getReleaseCost(PlayerID player_id) const;
  /** Terminates a managed player's contract, paying the remaining wages. */
  bool releasePlayer(PlayerID player_id);

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

  float randomFloat(float min, float max)
  {
    std::uniform_real_distribution<float> dis(min, max);
    return dis(transfer_rng);
  }

  // ========== World: inbox ==========
  /** Managed club's messages, oldest first. Display text via
   * InboxMessage::formatTitle() / formatBody(). */
  const std::vector<InboxMessage>& getInbox() const;
  /** Marks one message read; false if the id is unknown. */
  bool markInboxMessageRead(uint32_t message_id);
  void markAllInboxMessagesRead();
  size_t getUnreadInboxCount() const;

  // ========== World: board ==========
  /** Objective, expected/target position, confidence (0-100), dismissal. */
  const BoardState& getBoardState() const;

  // ========== World: finances ==========
  /** Dated transactions of a club, oldest first (empty if unknown). */
  const std::vector<FinanceTransaction>& getFinanceLedger(TeamID team_id) const;
  /** Per-category income/expense totals for transactions in [from, to]. */
  FinanceSummary getFinanceSummary(TeamID team_id, GameDateValue from,
                                   GameDateValue to) const;
  /** Current weekly payroll of a club. */
  int64_t getWeeklyWageBill(TeamID team_id) const;
  /**
   * Moves money between the managed club's budgets (52 weeks per season):
   * positive moves transfer budget into weekly wages, negative moves unused
   * wage room back. False if a budget would become insufficient.
   */
  bool moveTransferToWageBudget(int64_t transfer_amount);
  /** Sets the managed club's average ticket price (1-1000). */
  bool setTicketPrice(uint32_t price);
  /** Reference price at which the club meets its league's gate target. */
  uint32_t getFairTicketPrice(TeamID team_id) const;
  /** Crowd of the club's latest home match this session (0 if none). */
  uint32_t getLastHomeAttendance(TeamID team_id) const;

  // ========== World: players ==========
  /** False while injured. */
  bool isPlayerAvailable(PlayerID player_id) const;
  /** Injured players of a club. */
  std::vector<PlayerID> getInjuredPlayers(TeamID team_id) const;
  /** Selected players (XI and reserves) who are currently unavailable. */
  std::vector<PlayerID> getUnavailableLineupPlayers(TeamID team_id) const;
  /** Playing-time expectation from the player's ability rank. */
  SquadRole getSquadRole(PlayerID player_id) const;

  struct PotentialEstimate
  {
    float low = 0.0f;
    float high = 0.0f;
  };
  /** Scouted potential range (the scouting view's range; see below). */
  PotentialEstimate getPotentialEstimate(PlayerID player_id) const;

  /**
   * Physical consequences of the live match for one player (call before
   * setMatchResult, once per player and match day). end_condition < 0 lets
   * the simulation estimate drain and roll injuries; otherwise injured
   * reports an engine injury event whose diagnosis is drawn here.
   */
  bool applyMatchConsequences(PlayerID player_id, uint8_t minutes_played,
                              float end_condition, bool injured);

  /** Extends a managed player's contract if he accepts the terms. */
  bool renewContract(PlayerID player_id, ContractTerms terms);

  // ========== Scouting & recruitment ==========
  // Screens must show and sort other clubs' players by these estimates,
  // never by Player::getStats()/getOverall()/getPotential().

  /** Estimated attributes, overall, potential range and knowledge (0-100);
   * exact values only for the managed club's own players. */
  std::optional<ScoutedPlayerView> getScoutedView(PlayerID player_id) const;
  /** Compact estimate (overall, potential range, value, knowledge). */
  std::optional<ScoutedPlayerRow> getScoutedRow(PlayerID player_id) const;
  /** Player search on estimated values, best estimate first. */
  std::vector<ScoutedPlayerRow> searchScoutedPlayers(
      const ScoutSearchFilter& filter) const;
  /** Managed club's knowledge of a player, 0-100. */
  float getScoutingKnowledge(PlayerID player_id) const;

  const std::vector<ScoutProfile>& getScouts() const;
  const std::vector<ScoutAssignment>& getScoutAssignments() const;
  /** Price of an assignment; 0 if the target is invalid. */
  int64_t getScoutAssignmentCost(ScoutTargetKind kind, uint32_t target_id,
                                 uint16_t days) const;
  /** Sends a scout (cost booked to the ledger today). */
  ScoutAssignError startScoutAssignment(uint32_t scout_id,
                                        ScoutTargetKind kind,
                                        uint32_t target_id, uint16_t days);
  bool cancelScoutAssignment(uint32_t assignment_id);
  /** Scout reports, oldest first. */
  const std::vector<ScoutReport>& getScoutReports() const;

  const std::vector<RecruitmentFocus>& getRecruitmentFocuses() const;
  /** Adds (id 0) or updates a focus; returns its id, 0 on failure. */
  uint32_t saveRecruitmentFocus(const RecruitmentFocus& focus);
  bool removeRecruitmentFocus(uint32_t focus_id);

  const std::vector<ShortlistEntry>& getShortlist() const;
  bool isShortlisted(PlayerID player_id) const;
  bool addToShortlist(PlayerID player_id);
  bool removeFromShortlist(PlayerID player_id);
  /** Own depth per position against the best shortlisted estimate. */
  std::vector<SquadComparisonRow> compareShortlistWithSquad() const;

  // ========== Training ==========
  /** Managed club's training plan (nullptr without a managed club). */
  const TeamTrainingPlan* getTrainingPlan() const;
  /** Applies a preset's template (Custom keeps the current days). */
  bool setTrainingPreset(TrainingPreset preset);
  /** Changes one day of the managed club's template (preset -> Custom). */
  bool setTrainingSlot(MicrocycleDay day, TrainingSlot slot);
  /** Squad-wide intensity applied on top of the session intensities. */
  bool setTrainingIntensity(TrainingIntensity intensity);
  /** Collapses weeks with two matches to recovery and activation only. */
  bool setCongestionAutoAdjust(bool enabled);
  /** Individual training focus of a managed player. */
  bool setPlayerTrainingFocus(PlayerID player_id, TrainingFocus focus);

  struct PlayerWorkload
  {
    TrainingFocus focus = TrainingFocus::None;
    float acute = 0.0f;   /*!< 7-day load per day. */
    float chronic = 0.0f; /*!< 28-day load per day. */
    float ratio = 1.0f;   /*!< Acute:chronic. */
    WorkloadRisk risk = WorkloadRisk::Low;
    float trend = 0.0f; /*!< Smoothed weekly overall change. */
  };
  /** Focus, workload and development trend of a player. */
  PlayerWorkload getPlayerWorkload(PlayerID player_id) const;

  struct TrainingDayPreview
  {
    GameDateValue date;
    TrainingDay day;
    TeamID opponent = 0; /*!< On match days. */
    bool home = false;
  };
  /** The managed club's next seven days (today first) as scheduled. */
  std::vector<TrainingDayPreview> getTrainingWeekPreview() const;
  /** Tactical familiarity in [0, 1]; see TrainingSystem::tacticalFamiliarity. */
  float getTacticalFamiliarity(TeamID team_id) const;
  /** Assistant manager's recommendations, most important first. */
  std::vector<TrainingAdvice> getTrainingAdvice() const;

  // ========== Staff ==========
  enum class StaffActionResult : uint8_t
  {
    Ok,
    NoClub,
    UnknownStaff,
    NotAvailable, /*!< Not on the market / not at the managed club. */
    RoleFull,     /*!< StaffModel::ROLE_LIMITS reached. */
    CannotAfford, /*!< Cash below 12 weeks of the wage or the severance. */
    InvalidTerms
  };
  /** Staff of a club by role, best first. */
  std::vector<const StaffMember*> getStaff(TeamID team_id) const;
  /** Unattached staff, best first. */
  std::vector<const StaffMember*> getStaffMarket() const;
  /** What a club's staff does for the squad. */
  StaffEffects getStaffEffects(TeamID team_id) const;
  /**
   * Scouting accuracy of a club in [0, 1], from its scouts' judging
   * attributes and network size. Discrimination d' = 0.8 + 1.3 * accuracy;
   * StaffModel::potentialErrorSd() converts it to the standard deviation of
   * a potential estimate in overall points.
   */
  float getScoutingAccuracy(TeamID team_id) const;
  /** Weekly wage a candidate asks for (market) or to extend (own staff). */
  uint32_t getStaffWageDemand(StaffID staff_id) const;
  /** Hires a market candidate for @p years seasons (1-4). */
  StaffActionResult hireStaff(StaffID staff_id, uint8_t years);
  /** Releases a managed staff member, paying StaffModel::severance(). */
  StaffActionResult releaseStaff(StaffID staff_id);
  /** Extends a contract to @p years remaining seasons (2-5) at the
   * renewal wage. */
  StaffActionResult extendStaffContract(StaffID staff_id, uint8_t years);

  /** Seed of the current world. */
  uint64_t getWorldSeed() const;

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
  std::mt19937 transfer_rng;
  float last_initialization_milliseconds = 0.0f;
  std::atomic<int> continue_days_started{0};
  std::atomic<int> continue_days_total{0};
  /** One day of simulation plus the AI transfer activity that follows. */
  void simulateDay();
  bool executeTransfer(PlayerID pid, TeamID buyer_id, TeamID seller_id,
                       uint32_t price,
                       std::optional<ContractTerms> contract = std::nullopt);
  void processAITransferActivity();
  void evaluateIncomingAIBids();
  /** Drops listings of players who moved, left or may not be traded. */
  void purgeStaleListings();
  /** AI seller accepts the highest bid as a structured deal. */
  bool completeAiSale(PlayerID pid);
  /** Asking price of a live listing by the player's club (0 if none). */
  uint32_t listingPrice(PlayerID pid) const;
  /** Signing-day cash of a deal fits the buyer's transfer budget and the
   * new wage its wage budget. */
  bool canPayDeal(const TransferMarket::Deal& deal) const;

  std::string getSavePath(int slot) const;
  /** The managed club, only once one has been selected. */
  std::optional<std::reference_wrapper<Team>> managedClub();
  std::optional<std::reference_wrapper<const Team>> managedClub() const;
};
