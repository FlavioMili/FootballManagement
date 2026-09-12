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

#include "model/buyer_negotiation.h"
#include "model/transfer_negotiation.h"

/**
 * @namespace LoanNegotiation
 * @brief How an AI club negotiates to borrow one of the managed club's
 * players.
 *
 * The borrower has a hidden ceiling on what the loan may cost it, measured
 * by borrowerCost(): the share of the wage it pays over the loan, the loan
 * fee, a recall clause and the expected fee for appearances it fails to give
 * him. Its wage room caps the share it can take on, its budget the fees, and
 * a mandatory purchase must stay near the player's value. It answers the
 * club's counters on later days with structures of its own (a fee instead of
 * a bigger share, an option instead of an obligation, fewer guaranteed
 * appearances), runs out of patience, walks away from demands far beyond
 * its ceiling and concedes faster near the deadline, like the buyer
 * (BuyerNegotiation, whose moves and decisions it shares). Pure rules: every
 * draw is passed in.
 */
namespace LoanNegotiation
{
using BuyerNegotiation::Decision;
using BuyerNegotiation::Move;
using TransferNegotiation::LoanDuration;
using TransferNegotiation::LoanTerms;

/** Why the borrower answered the way it did. */
enum class Why : std::uint8_t
{
  WithinBudget,
  Improved,
  WageRoom,          /*!< Its wage room caps the share it can pay. */
  FeeInsteadOfWages, /*!< A loan fee in place of a bigger share. */
  ObligationDropped, /*!< An option to buy instead of an obligation. */
  AppearancesCut,    /*!< Fewer guaranteed appearances. */
  FinalOffer,
  Unrealistic,
  Insulted,
  OutOfPatience,
  NoWageRoom, /*!< It can no longer fit his wage at all. */
  DeadlineDay,
  COUNT
};

/** Language key describing @p why (e.g. "LOAN_WHY_WAGE_ROOM"). */
const char* whyKey(Why why);

/** True when the loan terms are consistent (shares, clauses). */
bool isValid(const LoanTerms& terms);

/** Everything the borrower weighs when answering. */
struct BorrowerContext
{
  std::uint32_t ceiling = 0;      /*!< Most it commits, borrowerCost terms. */
  std::int64_t cash = 0;          /*!< Budget for fees and a purchase. */
  std::int64_t wage_room = 0;     /*!< Weekly wages it can still add. */
  std::uint32_t weekly_wage = 0;  /*!< The player's full wage. */
  std::uint32_t market_value = 0;
  int season_weeks = 40;          /*!< Weeks to the end of the season. */
  float apps_per_week = 0.6f;     /*!< How often he would play there. */
  std::uint8_t patience = 3;
  std::uint8_t answered = 0;
  std::uint8_t insults = 0;
  int days_to_deadline = 30; /*!< -1 when the window is shut. */
};

/** Weeks a loan of @p duration runs from now. */
int loanWeeks(const BorrowerContext& context, LoanDuration duration);
/** Appearances the borrower expects to give him over the loan. */
double expectedAppearances(const BorrowerContext& context,
                           LoanDuration duration);
/** Odds he plays fewer than @p minimum games when @p expected are likely. */
double unplayedOdds(double expected, std::uint8_t minimum);
/** Highest wage share (whole percent) its wage room allows. */
std::uint8_t maxWageShare(const BorrowerContext& context);
/** What @p terms cost the borrower over the loan. */
double borrowerCost(const BorrowerContext& context, const LoanTerms& terms);
/** The ceiling lifted by deadline pressure. */
double effectiveCeiling(const BorrowerContext& context);
/** The terms fit its wage room and budget (cost aside). */
bool affordable(const BorrowerContext& context, const LoanTerms& terms);

/** The borrower's answer to a counter. */
struct BorrowerReply
{
  Decision decision = Decision::WalkAway;
  Move move = Move::WalkedAway;
  LoanTerms terms; /*!< Accept: the club's terms; Counter: its own. */
  bool insulted = false;
  std::vector<Why> reasons;
};

/**
 * Answer to the club's @p asked terms while @p current is on the table.
 * @p roll in [0, 1) picks how the borrower reshapes its proposal.
 */
BorrowerReply respond(const BorrowerContext& context, const LoanTerms& current,
                      const LoanTerms& asked, double roll);

/**
 * Opening offer for a loan of @p duration: @p share of the wage (capped by
 * the wage room) and, drawn with @p option_roll for a young player, an
 * option to buy.
 */
LoanTerms openingOffer(const BorrowerContext& context, LoanDuration duration,
                       std::uint8_t share, int age, double option_roll);

/** Ceiling of a borrower opening with @p opening, drawn with @p roll. */
std::uint32_t ceilingFor(const BorrowerContext& context,
                         const LoanTerms& opening, double roll);

}  // namespace LoanNegotiation
