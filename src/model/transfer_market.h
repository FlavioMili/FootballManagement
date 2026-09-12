// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "global/types.h"
#include "model/gamedate.h"
#include "model/inbox.h"
#include "model/transfer_listing.h"
#include "model/transfer_negotiation.h"

class CompetitionManager;
class DatabaseConnection;
class GameData;
class Player;
class Team;
class WorldRng;
class WorldSimulation;

/**
 * @enum TransferKind
 * @brief Type of a completed move (values are persisted).
 */
enum class TransferKind : std::uint8_t
{
  Permanent = 0, /*!< Club-to-club move with a fee. */
  Free,          /*!< Free agent signing. */
  Loan,
  LoanReturn,
  PreContract, /*!< Bosman move agreed six months ahead. */
  Release      /*!< Contract terminated by the club. */
};

/** Language key naming @p kind (e.g. "TRANSFER_KIND_LOAN"). */
const char* transferKindKey(TransferKind kind);

/** One entry of the transfer history. */
struct TransferRecord
{
  PlayerID player_id = 0;
  GameDateValue date;
  TeamID from_team = 0;
  TeamID to_team = 0;
  std::uint32_t fee = 0;
  TransferKind kind = TransferKind::Permanent;
};

/**
 * @enum ObligationKind
 * @brief Future payments created by a deal (values are persisted).
 */
enum class ObligationKind : std::uint8_t
{
  Instalment = 0,  /*!< amount due on @c due. */
  AppearanceBonus, /*!< amount due at @c target appearances for the payer. */
  GoalBonus,       /*!< amount due at @c target goals for the payer. */
  SellOn           /*!< payee gets @c amount % of the player's next fee. */
};

/** A scheduled or conditional payment between two clubs. */
struct TransferObligation
{
  std::uint32_t id = 0;
  ObligationKind kind = ObligationKind::Instalment;
  PlayerID player_id = 0;
  TeamID payer = 0; /*!< Sell-on: the club that will sell. */
  TeamID payee = 0;
  std::int64_t amount = 0;
  GameDateValue due; /*!< Instalment due date, otherwise the signing date. */
  std::uint16_t target = 0; /*!< Bonus threshold. */
  std::uint16_t baseline =
      0; /*!< Appearances/goals for the payer at signing. */
};

/** An active loan. The player belongs to the borrower's squad. */
struct LoanDeal
{
  PlayerID player_id = 0;
  TeamID parent = 0;
  TeamID borrower = 0;
  GameDateValue start;
  GameDateValue end;             /*!< Last day; the player returns then. */
  std::uint8_t wage_share = 100; /*!< % of the wage paid by the borrower. */
  std::uint32_t full_wage = 0;
  std::uint32_t option_fee = 0; /*!< 0 = no buy clause. */
  bool obligation = false;
  bool recall_clause = false;
};

/** A Bosman agreement executed on 1 July. */
struct PreContractDeal
{
  PlayerID player_id = 0;
  TeamID from_team = 0;
  TeamID to_team = 0;
  GameDateValue agreed;
  TransferNegotiation::ContractOffer terms;
};

/** Contract extras and loan-list flag of a player. */
struct PlayerMarketFlags
{
  std::uint32_t release_clause = 0;
  std::optional<SquadRole> promised_role;
  GameDateValue promise_date;
  bool loan_listed = false;

  bool empty() const
  {
    return release_clause == 0 && !promised_role && !loan_listed;
  }
};

/** An AI club's offer for one of the managed club's players. */
struct IncomingOffer
{
  std::uint32_t id = 0;
  PlayerID player_id = 0;
  TeamID buyer = 0;
  bool loan = false;
  TransferNegotiation::OfferTerms terms;
  TransferNegotiation::LoanTerms loan_terms;
  std::uint32_t max_fee = 0; /*!< Hidden ceiling of the buyer. */
  GameDateValue created;
  GameDateValue expires;
  std::uint8_t round = 0;
};

/** The managed club's talks as a buyer. */
struct Negotiation
{
  PlayerID player_id = 0;
  TeamID seller = 0;
  TransferNegotiation::ContractKind kind =
      TransferNegotiation::ContractKind::Transfer;
  TransferNegotiation::OfferTerms agreed; /*!< Fee agreed with the seller. */
  bool club_agreed = false;
  std::uint8_t club_rounds = 0;
  std::uint8_t player_rounds = 0;
  GameDateValue expires;
};

/**
 * @class TransferMarket
 * @brief Deals, loans, pre-contracts, payments and history of the market.
 *
 * Owned by Game next to the world simulation. It executes structured moves
 * (fees with instalments, add-ons and sell-on clauses, agent fees, signing
 * bonuses, loans and releases), runs the scheduled parts of existing deals
 * every day and drives the AI moves that do not go through the transfer
 * list (free agents, loans, releases, pre-contracts and approaches for the
 * managed club's players). All randomness comes from world RNG streams.
 */
class TransferMarket
{
 public:
  /** A structured permanent move. */
  struct Deal
  {
    PlayerID player_id = 0;
    TeamID buyer_id = 0;
    TransferKind kind = TransferKind::Permanent;
    TransferNegotiation::OfferTerms terms;
    TransferNegotiation::ContractOffer contract;
  };

  TransferMarket(std::shared_ptr<GameData> gamedata, WorldSimulation& world,
                 const CompetitionManager& competitions);

  // ---- Lifecycle ----

  /** Loan wages and returns, pre-contracts (1 July), instalments,
   * add-ons, promises, offer expiry and the weekly news digest. */
  void onDayAdvanced(const GameDateValue& date, TeamID managed_team_id);

  void load(const std::shared_ptr<DatabaseConnection>& db_conn);
  /** Writes the market state inside the caller's transaction. */
  void save(const std::shared_ptr<DatabaseConnection>& db_conn) const;
  void onSaved();

  // ---- Decision contexts ----

  TransferNegotiation::SaleContext saleContext(
      PlayerID player_id, TeamID buyer_id, const GameDateValue& date,
      std::uint32_t listing_price) const;
  TransferNegotiation::PlayerContext playerContext(
      PlayerID player_id, TeamID club_id,
      TransferNegotiation::ContractKind kind) const;
  /** Role the player would have by ability rank at @p club_id. */
  SquadRole projectedRole(PlayerID player_id, TeamID club_id) const;
  /** True if the player accepts the terms he demands from @p club_id. */
  bool wouldJoin(PlayerID player_id, TeamID club_id,
                 TransferNegotiation::ContractKind kind) const;

  // ---- Moves ----

  /** Fee or free move with the full deal structure; false if invalid. */
  bool completeTransfer(const Deal& deal, const GameDateValue& date,
                        TeamID managed_team_id);
  bool startLoan(PlayerID player_id, TeamID borrower_id,
                 const TransferNegotiation::LoanTerms& terms,
                 const GameDateValue& date, TeamID managed_team_id);
  /** Sends a loanee back (end of term or recall). */
  bool endLoan(PlayerID player_id, const GameDateValue& date,
               TeamID managed_team_id, bool recalled);
  /** Borrower buys the loanee for the option fee. */
  bool exerciseLoanOption(PlayerID player_id, const GameDateValue& date,
                          TeamID managed_team_id);
  /** Terminates the contract, paying the remaining wages. */
  bool releasePlayer(PlayerID player_id, const GameDateValue& date,
                     TeamID managed_team_id);
  bool agreePreContract(PlayerID player_id, TeamID club_id,
                        const TransferNegotiation::ContractOffer& terms,
                        const GameDateValue& date, TeamID managed_team_id);
  /** History and sell-on clauses of a move made by the legacy path. */
  void recordLegacyTransfer(PlayerID player_id, TeamID from_team,
                            TeamID to_team, std::uint32_t fee,
                            const GameDateValue& date);

  // ---- Rules ----

  /** Not on loan and not committed to another club. */
  bool canBeTraded(PlayerID player_id) const;
  bool loanWithinLimits(PlayerID player_id, TeamID parent,
                        TeamID borrower) const;
  /** Instalments a club owes from @p date until the season ends. */
  std::int64_t committedPayables(TeamID team_id,
                                 const GameDateValue& date) const;
  /**
   * Transfer money a club can commit on @p date: the board's allowance
   * capped by the cash left after the payroll reserve and the instalments
   * still owed this season (ClubEconomy::availableTransferBudget); zero
   * under a transfer embargo.
   */
  std::int64_t spendableBudget(TeamID team_id, const GameDateValue& date) const;
  /** Takes a departed player out of the managed club's line-up, filling
   * his place without rebuilding the rest of the manager's selection. */
  void removeFromLineup(Team& team, const Player& departed);
  /** Agent fee paid by the buyer: 10% of a fee, or weeks of wage. */
  static std::uint32_t agentFee(const Deal& deal);

  // ---- AI ----

  /**
   * One AI club's business on a visit: sheds surplus (loan-listing
   * prospects, releasing veterans), then fills its biggest need (a missing
   * position or a weak starter) with a free agent, a loan or a structured
   * fee deal with another AI club. Returns true when a player moved.
   */
  bool runAiClub(TeamID club_id,
                 const std::unordered_map<PlayerID, TransferListing>& listings,
                 const GameDateValue& date, TeamID managed_team_id,
                 WorldRng& rng, bool free_agents_only);
  /** Offers an AI club's surplus prospects for loan (no move). */
  void listLoanProspects(TeamID club_id);
  /** AI clubs agree pre-contracts (1 January - 30 June). */
  void runAiPreContracts(const GameDateValue& date, TeamID managed_team_id,
                         WorldRng& rng, int attempts);
  /** An AI club may bid for one of the managed club's players. */
  void runAiApproach(TeamID club_id, const GameDateValue& date,
                     TeamID managed_team_id, WorldRng& rng);

  // ---- Managed club offers and talks ----

  const std::vector<IncomingOffer>& incomingOffers() const { return incoming; }
  const IncomingOffer* findIncomingOffer(std::uint32_t offer_id) const;
  std::uint32_t addIncomingOffer(IncomingOffer offer);
  bool updateIncomingOffer(const IncomingOffer& offer);
  bool removeIncomingOffer(std::uint32_t offer_id);

  /** Open talks of the managed club by player. */
  const std::unordered_map<PlayerID, Negotiation>& talks() const
  {
    return negotiations;
  }
  const Negotiation* findNegotiation(PlayerID player_id) const;
  void setNegotiation(const Negotiation& negotiation);
  void removeNegotiation(PlayerID player_id);

  // ---- Flags ----

  const PlayerMarketFlags* flags(PlayerID player_id) const;
  void setLoanListed(PlayerID player_id, bool listed);
  bool isLoanListed(PlayerID player_id) const;

  // ---- Queries ----

  const LoanDeal* findLoan(PlayerID player_id) const;
  const std::unordered_map<PlayerID, LoanDeal>& loans() const
  {
    return active_loans;
  }
  const PreContractDeal* findPreContract(PlayerID player_id) const;
  const std::unordered_map<PlayerID, PreContractDeal>& preContracts() const
  {
    return pre_contracts;
  }
  /** Every move, oldest first. */
  const std::vector<TransferRecord>& history() const { return records; }
  /** Moves of one player, oldest first. */
  std::vector<TransferRecord> historyFor(PlayerID player_id) const;
  const std::vector<TransferObligation>& obligations() const
  {
    return pending_obligations;
  }

 private:
  Player* mutablePlayer(PlayerID player_id);
  std::string teamName(TeamID team_id) const;
  void movePlayer(PlayerID player_id, TeamID from_team, TeamID to_team,
                  TeamID managed_team_id);
  void pay(TeamID payer, TeamID payee, std::int64_t amount,
           const GameDateValue& date);
  std::int64_t paySellOns(PlayerID player_id, TeamID seller, std::uint32_t fee,
                          const GameDateValue& date);
  void addRecord(const TransferRecord& record);
  void post(const GameDateValue& date, InboxCategory category,
            std::string title_key, std::string body_key,
            std::vector<std::string> args, std::optional<PlayerID> player_id,
            std::optional<TeamID> team_id);
  std::uint16_t careerCount(PlayerID player_id, TeamID team_id,
                            ObligationKind kind) const;
  PlayerMarketFlags& mutableFlags(PlayerID player_id);
  void pruneFlags(PlayerID player_id);
  void clearOnMove(PlayerID player_id);

  /** Position group an AI club wants to strengthen and the level a
   * signing must reach. */
  struct AiNeed
  {
    PlayerRole group = PlayerRole::UNKNOWN;
    float min_overall = 0.0f;
  };
  std::optional<AiNeed> assessNeed(
      const std::vector<std::pair<double, PlayerID>>& ranked) const;
  void listLoanProspects(
      const std::vector<std::pair<double, PlayerID>>& ranked);
  bool aiShedSurplus(TeamID club_id,
                     const std::vector<std::pair<double, PlayerID>>& ranked,
                     const GameDateValue& date, TeamID managed_team_id,
                     WorldRng& rng);
  bool aiSignFreeAgent(TeamID club_id, const AiNeed& need,
                       std::int64_t wage_room, const GameDateValue& date,
                       TeamID managed_team_id);
  bool aiTakeLoan(TeamID club_id, const AiNeed& need, std::int64_t wage_room,
                  const GameDateValue& date, TeamID managed_team_id,
                  WorldRng& rng);
  bool aiBuy(TeamID club_id, const AiNeed& need, std::int64_t wage_room,
             const std::unordered_map<PlayerID, TransferListing>& listings,
             const GameDateValue& date, TeamID managed_team_id, WorldRng& rng);

  void payLoanWageShares(const GameDateValue& date);
  void processLoanEnds(const GameDateValue& date, TeamID managed_team_id);
  void executePreContracts(const GameDateValue& date, TeamID managed_team_id);
  void payDueInstalments(const GameDateValue& date, TeamID managed_team_id);
  void checkAddOns(const GameDateValue& date, TeamID managed_team_id);
  void checkPromises(const GameDateValue& date, TeamID managed_team_id);
  void expireOffers(const GameDateValue& date);
  void postNewsDigest(const GameDateValue& date, TeamID managed_team_id);

  std::shared_ptr<GameData> gamedata;
  WorldSimulation& world;
  const CompetitionManager& competitions;

  std::vector<TransferRecord> records;
  std::size_t persisted_records = 0;
  std::vector<TransferObligation> pending_obligations;
  std::unordered_map<PlayerID, LoanDeal> active_loans;
  std::unordered_map<PlayerID, PreContractDeal> pre_contracts;
  std::unordered_map<PlayerID, PlayerMarketFlags> player_flags;
  std::vector<IncomingOffer> incoming;
  std::unordered_map<PlayerID, Negotiation> negotiations;
  std::uint32_t next_id = 1;

  // Instalments due before season end, per payer (rebuilt when dirty).
  mutable std::unordered_map<TeamID, std::int64_t> payables_cache;
  mutable std::int32_t payables_day = -1;
  mutable bool payables_dirty = true;
};
