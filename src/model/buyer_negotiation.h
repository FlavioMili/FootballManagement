// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <vector>

#include "model/transfer_negotiation.h"

/**
 * @namespace BuyerNegotiation
 * @brief How an AI club negotiates to buy one of the managed club's players,
 * and how the player feels about the move.
 *
 * The buyer has a hidden ceiling on what the deal may cost it, measured by
 * buyerCost(): cash now counts in full, instalments are discounted, add-ons
 * cost their odds of being paid and a sell-on clause the expected share of a
 * future sale. It answers the club's counters with its own structures
 * (a higher fee spread over more years, add-ons instead of cash, a smaller
 * sell-on), loses patience after a few rounds, walks away from demands far
 * beyond its ceiling, pays more against rival bidders and near the deadline.
 * Pure rules: every draw is passed in, so the answers are deterministic.
 */
namespace BuyerNegotiation
{
using TransferNegotiation::OfferTerms;

/** One step of the talks over an incoming offer (values are persisted). */
enum class Move : std::uint8_t
{
  Bid = 0,     /*!< The buyer's opening bid. */
  Improved,    /*!< The buyer's counter-proposal. */
  FinalOffer,  /*!< The buyer's last word. */
  Restated,    /*!< The buyer stands by its offer after an unrealistic ask. */
  Counter,     /*!< The managed club's structured counter. */
  AskingPrice, /*!< The managed club named its price. */
  Accepted,    /*!< The buyer accepted the managed club's terms. */
  WalkedAway,  /*!< The buyer ended the talks. */
  COUNT
};

/** True for the moves made by the buying club. */
bool byBuyer(Move move);
/** Language key naming @p move (e.g. "OFFER_MOVE_BID"). */
const char* moveKey(Move move);

/** Why the buyer answered the way it did. */
enum class Why : std::uint8_t
{
  WithinBudget,
  Improved,
  MoreInstalments,
  AddOnsInsteadOfCash,
  AddOnsTrimmed,
  SellOnCut,
  CashLimited,
  FinalOffer,
  Unrealistic,
  Insulted,
  OutOfPatience,
  RivalBids,
  DeadlineDay,
  COUNT
};

/** Language key describing @p why (e.g. "OFFER_WHY_IMPROVED"). */
const char* whyKey(Why why);

/** Odds that an appearance add-on with @p target matches is paid. */
double appearanceOdds(std::uint16_t target);
/** Odds that a goal add-on with @p target goals is paid. */
double goalOdds(std::uint16_t target);
/** Expected cost per unit of fee of a 1% sell-on for a player of @p age. */
double sellOnCostPerPercent(int age);

/** What @p terms cost the buyer in present value (agent fee excluded). */
double buyerCost(const OfferTerms& terms, int age);
/** Agent fee the buyer pays on top of @p fee. */
std::uint32_t agentFee(std::uint32_t fee);
/** Cash the buyer pays on signing: the upfront part and the agent fee. */
std::int64_t signingCash(const OfferTerms& terms);

/** Everything the buyer weighs when answering. */
struct BuyerContext
{
  std::uint32_t ceiling = 0; /*!< Most it commits, in buyerCost() terms. */
  std::int64_t cash = 0;     /*!< Cash it can pay on signing. */
  int age = 25;              /*!< Of the player. */
  std::uint8_t patience = 3; /*!< Counters it answers in these talks. */
  std::uint8_t answered = 0; /*!< Counters answered so far. */
  std::uint8_t insults = 0;  /*!< Unrealistic demands so far. */
  std::uint8_t rivals = 0;   /*!< Other clubs bidding for the player. */
  int days_to_deadline = 30; /*!< -1 when the window is shut. */
};

/** True in the last days of an open window. */
bool deadlinePressure(int days_to_deadline);

/** The ceiling lifted by rival bids and deadline pressure. */
double effectiveCeiling(const BuyerContext& context);

enum class Decision : std::uint8_t
{
  Accept,
  Counter,
  WalkAway
};

/** The buyer's answer to a counter. */
struct BuyerReply
{
  Decision decision = Decision::WalkAway;
  Move move = Move::WalkedAway;
  OfferTerms terms;      /*!< Accept: the club's terms; Counter: its own. */
  bool insulted = false; /*!< The ask was far beyond its ceiling. */
  std::vector<Why> reasons;
};

/**
 * Answer to the club's @p asked terms while @p current is on the table.
 * @p firm marks a named price: the buyer meets it, makes its final offer or
 * leaves. @p roll in [0, 1) picks how the buyer reshapes its proposal.
 */
BuyerReply respond(const BuyerContext& context, const OfferTerms& current,
                   const OfferTerms& asked, bool firm, double roll);

/**
 * Opening bid of a club with @p ceiling: a share of it drawn with
 * @p share_roll (higher with rivals), the usual AI structure and, drawn with
 * @p add_on_roll, an appearance add-on.
 */
OfferTerms openingBid(std::uint32_t ceiling, int age, std::uint8_t rivals,
                      double share_roll, double add_on_roll);

/** Counters the buyer will answer (fewer near the deadline). */
std::uint8_t drawPatience(double roll, int days_to_deadline);

/** Days until the buyer answers a counter; 0 = the same day. */
int replyDelay(double roll, int days_to_deadline);

// ---------------------------------------------------------------------------
// The player
// ---------------------------------------------------------------------------

/** How the player feels about the move (shown in the talks). */
enum class PlayerStance : std::uint8_t
{
  AskedToLeave,    /*!< He has handed in a transfer request. */
  WantsBiggerClub, /*!< Ambitious, and the buyer is clearly bigger. */
  Open,            /*!< Would go, would stay. */
  HappyHere,       /*!< Loyal or settled; no pull towards the buyer. */
  Reluctant        /*!< Unlikely to agree terms with the buyer. */
};

/** Language key describing @p stance. */
const char* stanceKey(PlayerStance stance);

struct StanceFacts
{
  bool transfer_request = false;
  bool would_join = true; /*!< He accepts the terms he would demand. */
  int reputation_gap = 0; /*!< Buyer's reputation minus his club's. */
  std::uint8_t ambition = 50;
  std::uint8_t loyalty = 50;
  float morale = 60.0f;
};

PlayerStance stanceFor(const StanceFacts& facts);

/** Chance he fails to agree personal terms once the clubs agree. */
double termsRefusalChance(PlayerStance stance);

/** A bid worth at least BIG_BID_VALUE_SHARE of the market value. */
bool isBigBid(const OfferTerms& terms, int age, std::uint32_t market_value);

/** What turning a bid down does to the player. */
struct RejectionEffect
{
  float morale_delta = 0.0f; /*!< Before his temperament scales it. */
  float trust_delta = 0.0f;
  bool transfer_request = false;
};

RejectionEffect rejectionEffect(PlayerStance stance, bool big_bid,
                                std::uint8_t ambition);

}  // namespace BuyerNegotiation
