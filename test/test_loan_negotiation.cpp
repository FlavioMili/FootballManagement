// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

// The AI borrower's side of the talks over a loan for a managed player (how
// it prices loan terms, when it accepts, how it counters within its wage
// room and budget, when it walks away) and the player's agent in contract
// talks (his package, his own fee and his lines).

#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>

#include "model/loan_negotiation.h"
#include "model/player_agent.h"
#include "model/transfer_tuning.h"
#include "model/world_simulation.h"

namespace
{
using namespace LoanNegotiation;
using TransferNegotiation::LoanDuration;
using TransferNegotiation::LoanTerms;
using Borrower = TransferTuning::Borrower;

constexpr std::uint32_t WAGE = 20'000;

BorrowerContext borrower()
{
  BorrowerContext context;
  context.weekly_wage = WAGE;
  context.market_value = 4'000'000;
  context.wage_room = 1'000'000;
  context.cash = 50'000'000;
  context.season_weeks = 40;
  context.apps_per_week = 0.6f;
  context.patience = 3;
  context.days_to_deadline = 30;
  // Half his wage over the season, with some room on top.
  context.ceiling = 600'000;
  return context;
}

LoanTerms share(std::uint8_t percent)
{
  LoanTerms terms;
  terms.wage_share = percent;
  return terms;
}

bool has(const BorrowerReply& reply, Why why)
{
  return std::ranges::find(reply.reasons, why) != reply.reasons.end();
}
}  // namespace

TEST(LoanNegotiationTest, CostCountsWagesFeesClausesAndUnplayedGames)
{
  const BorrowerContext context = borrower();
  EXPECT_DOUBLE_EQ(borrowerCost(context, share(50)), WAGE * 0.5 * 40);
  EXPECT_LT(borrowerCost(context, share(50)), borrowerCost(context, share(75)));

  LoanTerms six = share(50);
  six.duration = LoanDuration::SixMonths;
  EXPECT_LT(borrowerCost(context, six), borrowerCost(context, share(50)));

  LoanTerms recall = share(50);
  recall.recall_clause = true;
  EXPECT_DOUBLE_EQ(borrowerCost(context, recall) - borrowerCost(context, share(50)),
                   static_cast<double>(WAGE) * Borrower::RECALL_COST_WEEKS);

  // A purchase clause is a decision apart: an option costs the loan
  // nothing, an obligation must stay near his value and within budget.
  LoanTerms option = share(50);
  option.option_fee = 6'000'000;
  EXPECT_DOUBLE_EQ(borrowerCost(context, option), borrowerCost(context, share(50)));
  LoanTerms obligation = option;
  obligation.obligation = true;
  EXPECT_FALSE(affordable(context, obligation)) << "1.5 times his value";
  obligation.option_fee = 4'400'000;
  EXPECT_TRUE(affordable(context, obligation));

  // The fee for unplayed games costs its odds: likely when he is
  // guaranteed more games than he would get.
  const double expected = expectedAppearances(context, LoanDuration::SeasonEnd);
  EXPECT_NEAR(expected, 24.0, 0.01);
  EXPECT_LT(unplayedOdds(expected, 10), 0.2);
  EXPECT_GT(unplayedOdds(expected, 30), 0.5);
  LoanTerms guaranteed = share(50);
  guaranteed.min_appearances = 30;
  guaranteed.unplayed_fee = 200'000;
  const double clause =
      borrowerCost(context, guaranteed) - borrowerCost(context, share(50));
  EXPECT_GT(clause, 100'000.0);
  EXPECT_LT(clause, 200'000.0);
  EXPECT_FALSE(isValid([] {
    LoanTerms bad;
    bad.min_appearances = 10;  // A guarantee without a fee means nothing.
    return bad;
  }()));
}

TEST(LoanNegotiationTest, AcceptsTermsWithinItsCeilingAndWageRoom)
{
  LoanTerms asked = share(60);  // 480K in wages and 80K for the recall.
  asked.recall_clause = true;
  const BorrowerReply reply = respond(borrower(), share(50), asked, 0.5);
  EXPECT_EQ(reply.decision, Decision::Accept);
  EXPECT_EQ(reply.move, Move::Accepted);
  EXPECT_EQ(reply.terms.wage_share, 60);
  EXPECT_TRUE(reply.terms.recall_clause);
  EXPECT_TRUE(has(reply, Why::WithinBudget));
}

TEST(LoanNegotiationTest, NeverTakesOnMoreWageThanItsRoom)
{
  BorrowerContext context = borrower();
  context.wage_room = WAGE * 6 / 10;  // Room for 60% of his wage.
  ASSERT_EQ(maxWageShare(context), 60);
  // Cheap enough, but the full wage does not fit: it counters at most 60%.
  for (const double roll : {0.1, 0.9})
  {
    const BorrowerReply reply = respond(context, share(50), share(100), roll);
    EXPECT_NE(reply.decision, Decision::Accept);
    ASSERT_EQ(reply.decision, Decision::Counter);
    EXPECT_LE(reply.terms.wage_share, 60);
    EXPECT_EQ(reply.terms.wage_share % Borrower::WAGE_SHARE_STEP, 0);
    EXPECT_TRUE(has(reply, Why::WageRoom));
    EXPECT_TRUE(affordable(context, reply.terms));
    if (roll < Borrower::FEE_INSTEAD_OF_WAGES_CHANCE)
    {
      // What it cannot pay in wages it offers as a loan fee.
      EXPECT_GT(reply.terms.loan_fee, 0u);
      EXPECT_TRUE(has(reply, Why::FeeInsteadOfWages));
    }
  }
  // Its budget changed since its offer: it walks away.
  context.wage_room = WAGE / 10;
  const BorrowerReply gone = respond(context, share(50), share(60), 0.5);
  EXPECT_EQ(gone.decision, Decision::WalkAway);
  EXPECT_TRUE(has(gone, Why::NoWageRoom));
}

TEST(LoanNegotiationTest, OffersAnOptionWhenAnObligationIsTooMuch)
{
  LoanTerms asked = share(50);
  asked.option_fee = 6'000'000;  // Over MAX_OBLIGATION_VALUE times his value.
  asked.obligation = true;
  const BorrowerReply reply = respond(borrower(), share(50), asked, 0.9);
  EXPECT_NE(reply.decision, Decision::Accept);
  ASSERT_EQ(reply.decision, Decision::Counter);
  EXPECT_FALSE(reply.terms.obligation);
  EXPECT_EQ(reply.terms.option_fee, 6'000'000u) << "the option itself stays";
  EXPECT_TRUE(has(reply, Why::ObligationDropped));
}

TEST(LoanNegotiationTest, CutsGuaranteesItCannotGive)
{
  LoanTerms asked = share(50);
  asked.min_appearances = 30;  // He would get about 24.
  asked.unplayed_fee = 400'000;
  const BorrowerReply reply = respond(borrower(), share(50), asked, 0.9);
  ASSERT_EQ(reply.decision, Decision::Counter);
  EXPECT_LE(reply.terms.min_appearances, 24);
  EXPECT_EQ(reply.terms.min_appearances % Borrower::APPEARANCES_STEP, 0);
  EXPECT_TRUE(has(reply, Why::AppearancesCut));
  EXPECT_TRUE(isValid(reply.terms));
}

TEST(LoanNegotiationTest, MovesTowardsTheAskThenMakesAFinalOffer)
{
  BorrowerContext context = borrower();
  const LoanTerms current = share(40);
  const LoanTerms asked = share(100);  // 800K against a 600K ceiling.
  const BorrowerReply first = respond(context, current, asked, 0.9);
  ASSERT_EQ(first.decision, Decision::Counter);
  EXPECT_EQ(first.move, Move::Improved);
  EXPECT_GT(first.terms.wage_share, current.wage_share);
  EXPECT_LT(borrowerCost(context, first.terms), effectiveCeiling(context));

  context.answered = 2;  // Its last answer.
  const BorrowerReply last = respond(context, first.terms, asked, 0.9);
  EXPECT_EQ(last.move, Move::FinalOffer);
  EXPECT_GE(last.terms.wage_share, first.terms.wage_share);
  EXPECT_LE(borrowerCost(context, last.terms), effectiveCeiling(context));

  context.answered = 3;
  EXPECT_EQ(respond(context, last.terms, asked, 0.9).decision,
            Decision::WalkAway);
}

TEST(LoanNegotiationTest, InsultsAreRestatedOnceThenEndTheTalks)
{
  BorrowerContext context = borrower();
  LoanTerms absurd = share(100);
  absurd.loan_fee = 500'000;  // 1.3M in all: over twice the ceiling.
  const BorrowerReply once = respond(context, share(50), absurd, 0.5);
  EXPECT_TRUE(once.insulted);
  EXPECT_EQ(once.move, Move::Restated);
  EXPECT_EQ(once.terms.wage_share, 50);
  context.insults = 1;
  const BorrowerReply twice = respond(context, share(50), absurd, 0.5);
  EXPECT_EQ(twice.decision, Decision::WalkAway);
  EXPECT_TRUE(has(twice, Why::Insulted));
}

TEST(LoanNegotiationTest, DeadlinePressureMeansMoreMoney)
{
  BorrowerContext calm = borrower();
  BorrowerContext urgent = borrower();
  urgent.days_to_deadline = 1;
  EXPECT_GT(effectiveCeiling(urgent), effectiveCeiling(calm));
  const BorrowerReply slow = respond(calm, share(40), share(100), 0.9);
  const BorrowerReply fast = respond(urgent, share(40), share(100), 0.9);
  EXPECT_GE(fast.terms.wage_share, slow.terms.wage_share);
  EXPECT_TRUE(has(fast, Why::DeadlineDay));
}

TEST(LoanNegotiationTest, OpeningOfferFitsTheWageRoom)
{
  BorrowerContext context = borrower();
  context.wage_room = WAGE * 33 / 100;
  const LoanTerms opening =
      openingOffer(context, LoanDuration::SeasonEnd, 50, 20, 0.0);
  EXPECT_EQ(opening.wage_share, 30) << "capped by the room, in editor steps";
  EXPECT_GT(opening.option_fee, context.market_value)
      << "a young player comes with an option to buy";
  EXPECT_FALSE(opening.obligation);
  EXPECT_EQ(openingOffer(context, LoanDuration::SeasonEnd, 50, 29, 0.0).option_fee,
            0u);
  EXPECT_GT(ceilingFor(context, opening, 0.5), borrowerCost(context, opening));
}

TEST(LoanNegotiationTest, SameInputsGiveTheSameAnswer)
{
  LoanTerms asked = share(90);
  asked.min_appearances = 20;
  asked.unplayed_fee = 300'000;
  asked.recall_clause = true;
  const BorrowerReply a = respond(borrower(), share(40), asked, 0.37);
  const BorrowerReply b = respond(borrower(), share(40), asked, 0.37);
  EXPECT_EQ(a.move, b.move);
  EXPECT_EQ(a.terms.wage_share, b.terms.wage_share);
  EXPECT_EQ(a.terms.loan_fee, b.terms.loan_fee);
  EXPECT_EQ(a.terms.min_appearances, b.terms.min_appearances);
  EXPECT_EQ(a.terms.unplayed_fee, b.terms.unplayed_fee);
}

// ---------------------------------------------------------------------------
// The player's agent
// ---------------------------------------------------------------------------

namespace
{
using TransferNegotiation::ContractKind;
using TransferNegotiation::ContractOffer;
using TransferNegotiation::PlayerContext;
using TransferNegotiation::Reason;
using N = TransferTuning::Negotiation;

PlayerContext target()
{
  PlayerContext context;
  context.kind = ContractKind::Transfer;
  context.age = 24;
  context.current_wage = 10'000;
  context.market_value = 5'000'000;
  context.ambition = 80;
  context.loyalty = 40;
  context.current_club_reputation = 50;
  context.new_club_reputation = 62;
  context.current_role = SquadRole::FirstTeam;
  context.projected_role = SquadRole::FirstTeam;
  return context;
}

bool refuses(const PlayerAgent::Reply& reply, Reason reason)
{
  return std::ranges::find(reply.response.reasons, reason) !=
         reply.response.reasons.end();
}

constexpr std::uint32_t FEE = 6'000'000;
constexpr std::uint8_t LAST_ROUND = N::MAX_PLAYER_ROUNDS - 1;
}  // namespace

TEST(ContractAgentTest, AsksForAPackageWorthHisFlatAsk)
{
  const PlayerContext context = target();
  const PlayerAgent::Demands demands = PlayerAgent::demands(context, FEE, 0, 4);
  EXPECT_EQ(demands.yearly_rise, N::RISE_HIGH) << "young and ambitious";
  EXPECT_GT(demands.appearance_bonus, 0u);
  EXPECT_GT(demands.signing_bonus, 0u) << "a signing-on fee on a transfer";
  EXPECT_LT(demands.asking_wage, demands.flat_wage)
      << "the extras lower the base wage";
  EXPECT_EQ(demands.standard_agent_fee, FEE / 10);
  EXPECT_GT(demands.agent_fee, demands.standard_agent_fee)
      << "he opens above the usual fee";
  EXPECT_EQ(PlayerAgent::agentFeeAsk(context, demands.standard_agent_fee,
                                     LAST_ROUND),
            demands.standard_agent_fee);

  // By the last round his package is exactly what the player signs for.
  const PlayerAgent::Demands last =
      PlayerAgent::demands(context, FEE, LAST_ROUND, 4);
  const PlayerAgent::Reply reply = PlayerAgent::respond(
      context, PlayerAgent::askedOffer(last, 4), LAST_ROUND, FEE);
  EXPECT_TRUE(reply.response.accepted);
  EXPECT_STREQ(reply.line_key, "AGENT_LINE_AGREED");

  // A veteran wants no rise.
  PlayerContext veteran = context;
  veteran.age = 32;
  EXPECT_EQ(PlayerAgent::demands(veteran, FEE, 0, 2).yearly_rise, 0);
}

TEST(ContractAgentTest, RiseAndAppearanceMoneyCountTowardsHisWage)
{
  const PlayerContext context = target();
  const auto demand = TransferNegotiation::contractDemand(context);
  ContractOffer offer = TransferNegotiation::demandedOffer(demand);
  offer.years = 4;
  offer.weekly_wage = demand.weekly_wage * 9 / 10;  // 10% short.
  EXPECT_FALSE(PlayerAgent::respond(context, offer, 0, FEE).response.accepted);
  offer.yearly_rise = 8;
  offer.appearance_bonus = demand.weekly_wage / 5;
  EXPECT_TRUE(PlayerAgent::respond(context, offer, 0, FEE).response.accepted)
      << "an 8% yearly rise and appearance money make up the difference";
}

TEST(ContractAgentTest, HisOwnFeeCanBlockADealThePlayerAccepts)
{
  const PlayerContext context = target();
  const auto demand = TransferNegotiation::contractDemand(context);
  ContractOffer offer = TransferNegotiation::demandedOffer(demand);
  offer.years = 4;
  const std::uint32_t lowest = PlayerAgent::lowestAgentFee(FEE / 10);
  offer.agent_fee = lowest - 100;
  const PlayerAgent::Reply squeezed = PlayerAgent::respond(context, offer, 0, FEE);
  EXPECT_FALSE(squeezed.response.accepted);
  EXPECT_TRUE(refuses(squeezed, Reason::AgentFeeTooLow));
  EXPECT_STREQ(squeezed.line_key, "AGENT_LINE_AGENT_FEE");
  offer.agent_fee = lowest;
  EXPECT_TRUE(PlayerAgent::respond(context, offer, 0, FEE).response.accepted);
  offer.agent_fee.reset();  // The standard fee.
  EXPECT_TRUE(PlayerAgent::respond(context, offer, 0, FEE).response.accepted);
}

TEST(ContractAgentTest, EveryRoundHasALineThatFitsTheAnswer)
{
  const PlayerContext context = target();
  const auto demand = TransferNegotiation::contractDemand(context);
  ContractOffer offer = TransferNegotiation::demandedOffer(demand);
  offer.years = 4;
  offer.weekly_wage = demand.weekly_wage / 2;
  EXPECT_STREQ(PlayerAgent::respond(context, offer, 0, FEE).line_key,
               "AGENT_LINE_INSULTED");
  offer.weekly_wage = demand.weekly_wage * 95 / 100;
  EXPECT_STREQ(PlayerAgent::respond(context, offer, 0, FEE).line_key,
               "AGENT_LINE_CLOSE");
  EXPECT_STREQ(PlayerAgent::respond(context, offer, LAST_ROUND, FEE).line_key,
               "AGENT_LINE_WALKED")
      << "refused in the last round: the talks end";
  offer.weekly_wage = demand.weekly_wage;
  offer.release_clause = 0;
  PlayerContext ambitious_small = context;
  ambitious_small.new_club_reputation = 40;  // A modest club: he wants a clause.
  ambitious_small.current_club_reputation = 45;
  const auto small_demand = TransferNegotiation::contractDemand(ambitious_small);
  ASSERT_TRUE(small_demand.wants_release_clause);
  offer.weekly_wage = small_demand.weekly_wage;
  EXPECT_STREQ(PlayerAgent::respond(ambitious_small, offer, 0, FEE).line_key,
               "AGENT_LINE_RELEASE_CLAUSE");

  EXPECT_STRNE(PlayerAgent::openingLine(context), "");
  EXPECT_STREQ(PlayerAgent::openingLine(context), "AGENT_OPEN_KEEN");
}

TEST(ContractAgentTest, SpeaksUpAfterTheSellingClubsAnswer)
{
  const PlayerContext context = target();
  TransferNegotiation::ClubResponse clause;
  clause.decision = TransferNegotiation::ClubResponse::Decision::Accept;
  clause.reasons.push_back(Reason::ReleaseClauseMet);
  EXPECT_STREQ(PlayerAgent::purchaseLine(clause, true, context),
               "AGENT_CLUB_CLAUSE");
  TransferNegotiation::ClubResponse counter;
  counter.decision = TransferNegotiation::ClubResponse::Decision::Counter;
  EXPECT_STREQ(PlayerAgent::purchaseLine(counter, true, context),
               "AGENT_CLUB_COUNTER_KEEN");
  TransferNegotiation::ClubResponse broke;
  broke.reasons.push_back(Reason::OverBudget);
  EXPECT_STREQ(PlayerAgent::purchaseLine(broke, true, context), "")
      << "an offer the club never heard";
}
