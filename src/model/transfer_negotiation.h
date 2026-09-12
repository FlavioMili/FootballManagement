// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "global/types.h"
#include "model/gamedate.h"

enum class SquadRole : std::uint8_t;  // model/world_simulation.h

/**
 * @namespace TransferNegotiation
 * @brief Pure decision rules of the transfer market.
 *
 * Club valuations, the seller's reading of a structured offer, the player's
 * contract demands and answers, loan terms and window timing. Nothing here
 * touches the game state, so every rule is unit-testable and deterministic.
 */
namespace TransferNegotiation
{

/** Why a club or player answered the way they did (language keys). */
enum class Reason : std::uint8_t
{
  // Selling club
  KeyPlayer,
  FirstTeamPlayer,
  SellingToRival,
  MidSeason,
  LateWindow,
  RicherBuyer,
  Listed,
  NotForSale,
  ReleaseClauseMet,
  OfferTooLow,
  TooLittleUpfront,
  CounterOffer,
  TalksBroken,
  OfferAccepted,
  LongContract,
  ExpiringContract,
  OpeningBidLow,
  KeyPlayerNotForSale,
  // Player
  WageTooLow,
  ContractTooShort,
  ContractTooLong,
  ClubTooSmall,
  PlayingTime,
  PromiseNotCredible,
  WantsReleaseClause,
  LoyalToClub,
  BiggerClub,
  TermsAccepted,
  TalksEnded,
  AgentPushback,
  // Loans
  NotForLoan,
  WageShareTooLow,
  OptionTooLow,
  LoanLimit,
  NoPlayingTimeOnLoan,
  // Buying club
  WindowClosed,
  Unavailable,
  OverBudget,
  Embargo,
  // Player's agent
  AgentFeeTooLow,
  COUNT
};

/** Language key describing @p reason (e.g. "NEG_REASON_KEY_PLAYER"). */
const char* reasonKey(Reason reason);

/**
 * @struct OfferTerms
 * @brief A club-to-club offer: fee, instalments, add-ons and sell-on.
 *
 * upfront_percent of the fee is paid on signing and the rest in equal
 * yearly instalments. Add-ons are paid once the player reaches the target
 * number of competitive appearances or goals for the buying club.
 */
struct OfferTerms
{
  std::uint32_t fee = 0;
  std::uint8_t upfront_percent = 100;
  std::uint8_t instalment_years = 0;
  std::uint32_t appearance_bonus = 0;
  std::uint16_t appearance_target = 0;
  std::uint32_t goal_bonus = 0;
  std::uint16_t goal_target = 0;
  std::uint8_t sell_on_percent = 0;
};

/** True when the structure is internally consistent. */
bool isValid(const OfferTerms& terms);

/** Amount paid on signing. */
std::uint32_t upfrontAmount(const OfferTerms& terms);

/** Yearly instalments (sum = fee - upfront). */
std::vector<std::uint32_t> instalmentAmounts(const OfferTerms& terms);

/** Structure used by AI clubs: big fees are spread over instalments. */
OfferTerms aiOfferTerms(std::uint32_t fee);

/**
 * An AI club's bid for a player the seller values at @p asking_fee: the AI
 * structure (instalments above AI_INSTALMENT_THRESHOLD) with the headline
 * fee raised until the seller's valuation of the deferred money reaches the
 * asking fee, so large deals between computer-managed clubs can complete.
 */
OfferTerms aiBidFor(std::uint32_t asking_fee, int age);

/** Everything the selling club weighs. */
struct SaleContext
{
  std::uint32_t market_value = 0;
  int age = 25;
  SquadRole role{};
  std::uint8_t contract_years = 1;
  std::uint8_t seller_reputation = 50;
  std::uint8_t buyer_reputation = 50;
  bool same_league = false;
  bool listed = false;
  std::uint32_t listing_price = 0;
  std::uint32_t release_clause = 0;
  bool winter_window = false;
  int days_to_deadline = 90;
  /** Seller's resolve in [0, 1), drawn per player and window: the most
   * stubborn clubs refuse to sell a key player at all. */
  float resolve = 0.0f;
};

/** Seller's asking fee and the reasons behind it. */
struct Valuation
{
  std::uint32_t asking_fee = 0;
  bool not_for_sale = false;
  std::vector<Reason> reasons;
};

Valuation valueForSale(const SaleContext& context);

/** Seller's present value of an offer (deferred money discounted,
 * add-ons and sell-on credited with caps). */
double sellerValue(const OfferTerms& terms, int age);

/** Answer of a club to an offer. */
struct ClubResponse
{
  enum class Decision : std::uint8_t
  {
    Accept,
    Counter,
    Reject
  };
  Decision decision = Decision::Reject;
  std::uint32_t counter_fee = 0;        /*!< Counter: fee with same shape. */
  std::uint8_t counter_wage_share = 0;  /*!< Loan counter. */
  std::uint32_t counter_option_fee = 0; /*!< Loan counter. */
  std::vector<Reason> reasons;
};

/**
 * Evaluates a transfer offer. @p round counts offers already made in these
 * talks (0 = first offer).
 */
ClubResponse evaluateOffer(const SaleContext& context, const OfferTerms& terms,
                           std::uint8_t round);

// ---------------------------------------------------------------------------
// Player contracts
// ---------------------------------------------------------------------------

enum class ContractKind : std::uint8_t
{
  Transfer,
  FreeAgent,
  PreContract,
  Renewal
};

/** Terms a club offers a player. */
struct ContractOffer
{
  std::uint32_t weekly_wage = 0;
  std::uint8_t years = 0;
  std::uint32_t signing_bonus = 0;
  std::uint32_t release_clause = 0; /*!< 0 = none. */
  std::optional<SquadRole> promised_role;
  /** Yearly wage rise in percent, applied on each anniversary. */
  std::uint8_t yearly_rise = 0;
  /** Paid to the player for every competitive appearance. */
  std::uint32_t appearance_bonus = 0;
  /** Fee paid to his agent on signing; none = the standard fee. */
  std::optional<std::uint32_t> agent_fee;
};

/** Competitive appearances per week of a player in @p role. */
float appearanceRate(SquadRole role);

/** Weekly worth of a yearly rise of @p percent over @p years on @p wage
 * (the average uplift over the contract). */
double yearlyRiseWorth(std::uint32_t wage, std::uint8_t percent,
                       std::uint8_t years);

/** What the player weighs. Reputations are 1-100; 0 = no club. */
struct PlayerContext
{
  ContractKind kind = ContractKind::Transfer;
  int age = 25;
  std::uint32_t current_wage = 0;
  std::uint32_t market_value = 0;
  std::uint8_t ambition = 50;
  std::uint8_t loyalty = 50;
  bool unsettled = false;
  std::uint8_t current_club_reputation = 0;
  std::uint8_t new_club_reputation = 50;
  SquadRole current_role{};   /*!< Role at the current club. */
  SquadRole projected_role{}; /*!< Role by ability rank at the new club. */
};

/**
 * The player's demands. weekly_wage is the lowest wage he signs for (never
 * shown); asking_wage is what his agent currently asks for in public.
 */
struct ContractDemand
{
  std::uint32_t weekly_wage = 0;
  std::uint32_t asking_wage = 0;
  std::uint8_t min_years = 1;
  std::uint8_t max_years = 5;
  std::uint32_t signing_bonus = 0;
  SquadRole desired_role{};
  bool wants_release_clause = false;
  std::uint32_t max_release_clause = 0;
};

/** Longest contract allowed at @p age (RSTP). */
std::uint8_t maxContractYears(int age);

/** Demands for talks that have not started (asking_wage = opening ask). */
ContractDemand contractDemand(const PlayerContext& context);

/**
 * Wage the agent asks for after @p round rejected proposals: above the real
 * demand by a margin that grows with ambition and with the step down in
 * club stature, falling to the demand itself by the last round.
 */
std::uint32_t agentAsk(const PlayerContext& context,
                       const ContractDemand& demand, std::uint8_t round);

/** Player's answer; accepted only when no refusal reason applies. After a
 * refusal demand.asking_wage is the agent's ask for the next proposal. */
struct ContractResponse
{
  bool accepted = false;
  std::vector<Reason> reasons;
  ContractDemand demand;
};

/** @p round counts proposals already rejected in these talks. */
ContractResponse evaluateContract(const PlayerContext& context,
                                  const ContractOffer& offer,
                                  std::uint8_t round);

/** Contract that exactly meets the demands (used by AI clubs). */
ContractOffer demandedOffer(const ContractDemand& demand);

// ---------------------------------------------------------------------------
// Loans
// ---------------------------------------------------------------------------

enum class LoanDuration : std::uint8_t
{
  SeasonEnd,
  SixMonths
};

/** A loan offer: wage share paid by the borrower, fee and buy clauses. */
struct LoanTerms
{
  LoanDuration duration = LoanDuration::SeasonEnd;
  std::uint8_t wage_share = 100; /*!< % of the wage paid by the borrower. */
  std::uint32_t loan_fee = 0;
  std::uint32_t option_fee = 0; /*!< 0 = no buy clause. */
  bool obligation = false;      /*!< Buy clause is mandatory at the end. */
  bool recall_clause = false;   /*!< Parent may recall in January. */
  /** Competitive appearances the borrower guarantees (0 = none); short of
   * them at the end of the loan it pays @c unplayed_fee to the parent. */
  std::uint8_t min_appearances = 0;
  std::uint32_t unplayed_fee = 0;
};

/** What the parent club weighs. */
struct LoanContext
{
  std::uint32_t market_value = 0;
  std::uint32_t weekly_wage = 0;
  int age = 21;
  SquadRole role{};
  bool loan_listed = false;
  int weeks = 40;
  bool within_limits = true;
};

ClubResponse evaluateLoan(const LoanContext& context, const LoanTerms& terms);

/** True when the player agrees to the loan; otherwise @p reasons says why. */
bool playerAcceptsLoan(SquadRole at_parent, SquadRole at_borrower,
                       std::uint8_t ambition, int reputation_gap,
                       std::vector<Reason>& reasons);

// ---------------------------------------------------------------------------
// Calendar helpers
// ---------------------------------------------------------------------------

/** Transfer window state on a date. */
struct WindowInfo
{
  bool open = false;
  bool winter = false;
  int days_to_deadline = 0; /*!< 0 on deadline day; -1 when shut. */
};

/** Window of clubs of @p league on @p date (TransferWindows). */
WindowInfo windowInfo(LeagueID league, const GameDateValue& date);

/** Relative AI activity: rises in the last week, spikes on deadline day. */
float activityWeight(const WindowInfo& window);

/** Players in the final six months (from 1 January) may pre-sign. */
bool canSignPreContract(std::uint8_t contract_years, const GameDateValue& date);

/** Last day of a loan starting on @p start (clipped to 30 June). */
GameDateValue loanEndDate(const GameDateValue& start, LoanDuration duration);

/** 30 June closing the season that contains @p date. */
GameDateValue seasonEndDate(const GameDateValue& date);

/** Last day of a contract with @p contract_years seasons left on @p date
 * (years are counted down on 1 July). */
GameDateValue contractEndDate(const GameDateValue& date,
                              std::uint8_t contract_years);

/** Whole weeks from @p from to @p to (0 if not after). */
int weeksBetween(const GameDateValue& from, const GameDateValue& to);

/** Remaining contract wages owed to a released player. */
std::int64_t severancePay(std::uint32_t weekly_wage,
                          std::uint8_t contract_years,
                          const GameDateValue& date);

/** Playing-time role for a 0-based ability rank (as the world sim). */
SquadRole roleForRank(std::size_t rank);

// ---------------------------------------------------------------------------
// Squad needs
// ---------------------------------------------------------------------------

/** Position group for squad planning: central midfielders (CDM, CM, CAM)
 * and wide players (LM, RM, LW, RW) are interchangeable. */
PlayerRole positionGroup(PlayerRole role);

/** One position group of a squad: players, slots in a typical XI, depth
 * wanted and the weakest of its starters (0 when short of starters). */
struct PositionNeed
{
  PlayerRole group = PlayerRole::UNKNOWN;
  std::uint8_t count = 0;
  std::uint8_t starters = 0;
  std::uint8_t wanted = 0;
  float weakest_starter = 0.0f;
};

/** The shape of a squad by position group. */
struct SquadNeeds
{
  static constexpr std::size_t GROUPS = 7;
  std::array<PositionNeed, GROUPS> positions{};
  float squad_level = 0.0f; /*!< Mean overall of the best 16. */

  const PositionNeed* find(PlayerRole role) const;
};

/** Needs of a squad given each player's role and overall. */
SquadNeeds squadNeeds(std::span<const std::pair<PlayerRole, float>> squad);

enum class FitKind : std::uint8_t
{
  None,     /*!< Would not improve the squad. */
  Starter,  /*!< Fills a missing starting place. */
  Upgrade,  /*!< Better than the weakest starter. */
  Depth     /*!< Needed cover. */
};

/** How a player would fit: score (higher is better), kind and the gain in
 * overall over the weakest starter (or the level asked of a starter). */
struct SquadFit
{
  float score = 0.0f;
  FitKind kind = FitKind::None;
  float gain = 0.0f;
};

SquadFit squadFit(const SquadNeeds& needs, PlayerRole role, float overall);

}  // namespace TransferNegotiation
