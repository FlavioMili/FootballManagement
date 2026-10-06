// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

// The AI buyer's side of the talks over a bid for a managed player: how it
// prices a structure, when it accepts, how it counters, when it walks away
// and how rivals and the deadline move it; and the player's stance.

#include <gtest/gtest.h>

#include <algorithm>

#include "model/buyer_negotiation.h"
#include "model/transfer_tuning.h"

namespace
{
using namespace BuyerNegotiation;
using TransferNegotiation::OfferTerms;
using Buyer = TransferTuning::Buyer;

constexpr std::uint32_t CEILING = 10'000'000;

OfferTerms cash(std::uint32_t fee)
{
  OfferTerms terms;
  terms.fee = fee;
  return terms;
}

BuyerContext buyer(std::uint32_t ceiling = CEILING)
{
  BuyerContext context;
  context.ceiling = ceiling;
  context.cash = 1'000'000'000;
  context.age = 25;
  context.patience = 3;
  context.days_to_deadline = 30;
  return context;
}

bool has(const BuyerReply& reply, Why why)
{
  return std::ranges::find(reply.reasons, why) != reply.reasons.end();
}

bool sameTerms(const OfferTerms& a, const OfferTerms& b)
{
  return a.fee == b.fee && a.upfront_percent == b.upfront_percent &&
         a.instalment_years == b.instalment_years &&
         a.appearance_bonus == b.appearance_bonus &&
         a.appearance_target == b.appearance_target &&
         a.goal_bonus == b.goal_bonus && a.goal_target == b.goal_target &&
         a.sell_on_percent == b.sell_on_percent;
}
}  // namespace

TEST(BuyerNegotiationTest, DeferredMoneyAndAddOnsCostTheBuyerLessThanCash)
{
  EXPECT_DOUBLE_EQ(buyerCost(cash(CEILING), 25), CEILING);

  OfferTerms spread = cash(CEILING);
  spread.upfront_percent = 50;
  spread.instalment_years = 3;
  EXPECT_LT(buyerCost(spread, 25), CEILING);
  // The buyer discounts deferred money faster than the seller values it,
  // so instalments are worth more to the club than they cost the buyer.
  EXPECT_LT(buyerCost(spread, 25),
            TransferNegotiation::sellerValue(spread, 25));

  OfferTerms add_on = cash(CEILING);
  add_on.appearance_bonus = 2'000'000;
  add_on.appearance_target = 30;
  const double add_on_cost = buyerCost(add_on, 25) - CEILING;
  EXPECT_GT(add_on_cost, 0.0);
  EXPECT_LT(add_on_cost, 2'000'000.0) << "an add-on costs its odds";

  OfferTerms sell_on = cash(CEILING);
  sell_on.sell_on_percent = 15;
  EXPECT_GT(buyerCost(sell_on, 20), buyerCost(sell_on, 31))
      << "a young player's resale share costs more";
  EXPECT_GT(buyerCost(sell_on, 31), CEILING);
}

TEST(BuyerNegotiationTest, AcceptsACounterWithinItsCeiling)
{
  const OfferTerms asked = cash(9'500'000);
  const BuyerReply reply = respond(buyer(), cash(8'000'000), asked, false, 0.5);
  EXPECT_EQ(reply.decision, Decision::Accept);
  EXPECT_EQ(reply.move, Move::Accepted);
  EXPECT_TRUE(sameTerms(reply.terms, asked));
  EXPECT_TRUE(has(reply, Why::WithinBudget));
}

TEST(BuyerNegotiationTest, CountersWithStructuresOfItsOwn)
{
  const OfferTerms current = cash(8'000'000);
  const OfferTerms asked = cash(12'000'000);
  const double previous = buyerCost(current, 25);

  // Spread: a higher headline, paid over more years.
  const BuyerReply spread = respond(buyer(), current, asked, false, 0.1);
  ASSERT_EQ(spread.decision, Decision::Counter);
  EXPECT_EQ(spread.move, Move::Improved);
  EXPECT_GT(spread.terms.fee, current.fee);
  EXPECT_GT(spread.terms.instalment_years, 0);
  EXPECT_LT(spread.terms.upfront_percent, 100);
  EXPECT_TRUE(has(spread, Why::MoreInstalments));

  // Add-ons instead of cash.
  const BuyerReply add_ons = respond(buyer(), current, asked, false, 0.5);
  ASSERT_EQ(add_ons.decision, Decision::Counter);
  EXPECT_GT(add_ons.terms.appearance_bonus, 0u);
  EXPECT_GT(add_ons.terms.appearance_target, 0);
  EXPECT_GE(add_ons.terms.fee, current.fee);
  EXPECT_TRUE(has(add_ons, Why::AddOnsInsteadOfCash));

  // The club's own shape, closer to its valuation.
  const BuyerReply match = respond(buyer(), current, asked, false, 0.9);
  ASSERT_EQ(match.decision, Decision::Counter);
  EXPECT_EQ(match.terms.instalment_years, 0);
  EXPECT_GT(match.terms.fee, current.fee);

  for (const BuyerReply* reply : {&spread, &add_ons, &match})
  {
    EXPECT_TRUE(TransferNegotiation::isValid(reply->terms));
    EXPECT_GT(buyerCost(reply->terms, 25), previous);
    EXPECT_LE(buyerCost(reply->terms, 25), CEILING)
        << "never above its ceiling";
  }
}

TEST(BuyerNegotiationTest, SpreadsThePaymentsWhenShortOfCash)
{
  BuyerContext context = buyer();
  context.cash = 3'000'000;
  const OfferTerms current = TransferNegotiation::aiOfferTerms(8'000'000);
  const BuyerReply reply =
      respond(context, current, cash(9'500'000), false, 0.9);
  ASSERT_EQ(reply.decision, Decision::Counter)
      << "within the ceiling, but not payable upfront";
  EXPECT_TRUE(has(reply, Why::CashLimited));
  EXPECT_GT(reply.terms.instalment_years, 0);
  EXPECT_LE(signingCash(reply.terms), context.cash);
}

TEST(BuyerNegotiationTest, CutsABigSellOnForAYoungPlayer)
{
  BuyerContext context = buyer();
  context.age = 20;
  OfferTerms asked = cash(12'000'000);
  asked.sell_on_percent = 20;
  const BuyerReply reply = respond(context, cash(8'000'000), asked, false, 0.9);
  ASSERT_EQ(reply.decision, Decision::Counter);
  EXPECT_LT(reply.terms.sell_on_percent, asked.sell_on_percent);
  EXPECT_TRUE(has(reply, Why::SellOnCut));
}

TEST(BuyerNegotiationTest, LastAnswerIsAFinalOfferAtTheCeiling)
{
  BuyerContext context = buyer();
  context.answered = context.patience - 1;
  const BuyerReply reply =
      respond(context, cash(8'000'000), cash(12'000'000), false, 0.9);
  ASSERT_EQ(reply.decision, Decision::Counter);
  EXPECT_EQ(reply.move, Move::FinalOffer);
  EXPECT_GE(buyerCost(reply.terms, 25), 0.97 * CEILING);
  EXPECT_LE(buyerCost(reply.terms, 25), CEILING);

  // A named price above the ceiling gets the final offer straight away.
  const BuyerReply firm =
      respond(buyer(), cash(8'000'000), cash(12'000'000), true, 0.9);
  EXPECT_EQ(firm.move, Move::FinalOffer);
}

TEST(BuyerNegotiationTest, WalksAwayWhenInsultedOrOutOfPatience)
{
  const OfferTerms current = cash(8'000'000);
  // Twice the ceiling: unrealistic; the buyer stands by its bid once.
  const BuyerReply first =
      respond(buyer(), current, cash(2 * CEILING), false, 0.5);
  EXPECT_EQ(first.decision, Decision::Counter);
  EXPECT_EQ(first.move, Move::Restated);
  EXPECT_TRUE(first.insulted);
  EXPECT_TRUE(sameTerms(first.terms, current));

  BuyerContext insulted = buyer();
  insulted.insults = 1;
  const BuyerReply second =
      respond(insulted, current, cash(2 * CEILING), false, 0.5);
  EXPECT_EQ(second.decision, Decision::WalkAway);
  EXPECT_TRUE(has(second, Why::Insulted));

  const BuyerReply outrageous =
      respond(buyer(), current, cash(3 * CEILING), false, 0.5);
  EXPECT_EQ(outrageous.decision, Decision::WalkAway);

  BuyerContext tired = buyer();
  tired.answered = tired.patience;
  const BuyerReply patience =
      respond(tired, current, cash(11'000'000), false, 0.5);
  EXPECT_EQ(patience.decision, Decision::WalkAway);
  EXPECT_TRUE(has(patience, Why::OutOfPatience));
}

TEST(BuyerNegotiationTest, SameInputsGiveTheSameAnswer)
{
  const OfferTerms current = cash(8'000'000);
  const OfferTerms asked = cash(12'000'000);
  for (const double roll : {0.05, 0.4, 0.8})
  {
    const BuyerReply a = respond(buyer(), current, asked, false, roll);
    const BuyerReply b = respond(buyer(), current, asked, false, roll);
    EXPECT_EQ(a.decision, b.decision);
    EXPECT_EQ(a.move, b.move);
    EXPECT_TRUE(sameTerms(a.terms, b.terms));
    EXPECT_EQ(a.reasons, b.reasons);
  }
  EXPECT_TRUE(sameTerms(openingBid(CEILING, 24, 0, 0.3, 0.2),
                        openingBid(CEILING, 24, 0, 0.3, 0.2)));
  EXPECT_EQ(drawPatience(0.7, 30), drawPatience(0.7, 30));
  EXPECT_EQ(replyDelay(0.7, 30), replyDelay(0.7, 30));
}

TEST(BuyerNegotiationTest, RivalBidsRaiseTheCeiling)
{
  BuyerContext alone = buyer();
  BuyerContext contested = buyer();
  contested.rivals = 2;
  EXPECT_GT(effectiveCeiling(contested), effectiveCeiling(alone));
  contested.rivals = 20;
  EXPECT_LE(
      effectiveCeiling(contested),
      CEILING * (1.0 + static_cast<double>(Buyer::MAX_RIVAL_CEILING_BONUS)) +
          1.0);

  // 10% above the ceiling: countered alone, accepted against two rivals.
  const OfferTerms asked = cash(11'000'000);
  EXPECT_EQ(respond(alone, cash(8'000'000), asked, false, 0.5).decision,
            Decision::Counter);
  contested.rivals = 2;
  const BuyerReply reply =
      respond(contested, cash(8'000'000), asked, false, 0.5);
  EXPECT_EQ(reply.decision, Decision::Accept);
  EXPECT_TRUE(has(reply, Why::RivalBids));

  // Rivals also make the opening bid bolder.
  EXPECT_GT(buyerCost(openingBid(CEILING, 25, 1, 0.5, 0.9), 25),
            buyerCost(openingBid(CEILING, 25, 0, 0.5, 0.9), 25));
}

TEST(BuyerNegotiationTest, DeadlinePressureMeansSameDayAnswersAndMoreMoney)
{
  BuyerContext relaxed = buyer();
  BuyerContext deadline = buyer();
  deadline.days_to_deadline = 1;
  EXPECT_GT(effectiveCeiling(deadline), effectiveCeiling(relaxed));
  EXPECT_EQ(replyDelay(0.9, 1), 0);
  EXPECT_EQ(replyDelay(0.9, 0), 0);
  EXPECT_GE(replyDelay(0.0, 30), Buyer::MIN_REPLY_DAYS);
  EXPECT_LE(replyDelay(0.99, 30), Buyer::MAX_REPLY_DAYS);
  EXPECT_LE(drawPatience(0.99, 1), Buyer::MIN_PATIENCE);

  const OfferTerms current = cash(8'000'000);
  const OfferTerms asked = cash(12'000'000);
  const BuyerReply calm = respond(relaxed, current, asked, false, 0.9);
  const BuyerReply urgent = respond(deadline, current, asked, false, 0.9);
  EXPECT_GT(buyerCost(urgent.terms, 25), buyerCost(calm.terms, 25))
      << "it concedes faster when time runs out";
}

TEST(BuyerNegotiationTest, OpeningBidStaysBelowTheCeiling)
{
  const OfferTerms low = openingBid(CEILING, 25, 0, 0.0, 0.9);
  const OfferTerms high = openingBid(CEILING, 25, 0, 1.0, 0.9);
  EXPECT_LE(buyerCost(high, 25),
            CEILING * static_cast<double>(Buyer::OPENING_SHARE_MAX));
  EXPECT_GE(buyerCost(low, 25),
            CEILING * static_cast<double>(Buyer::OPENING_SHARE_MIN) - 10'000.0);
  EXPECT_GT(high.fee, low.fee);
  EXPECT_GT(high.instalment_years, 0) << "big fees are spread";
  const OfferTerms with_add_on = openingBid(CEILING, 25, 0, 0.5, 0.1);
  EXPECT_GT(with_add_on.appearance_bonus, 0u);
  EXPECT_TRUE(TransferNegotiation::isValid(with_add_on));
}

TEST(BuyerNegotiationTest, PlayerStanceAndHisReactionToARejection)
{
  StanceFacts facts;
  facts.reputation_gap = 15;
  facts.ambition = 80;
  EXPECT_EQ(stanceFor(facts), PlayerStance::WantsBiggerClub);
  facts.transfer_request = true;
  EXPECT_EQ(stanceFor(facts), PlayerStance::AskedToLeave);
  facts.transfer_request = false;
  facts.would_join = false;
  EXPECT_EQ(stanceFor(facts), PlayerStance::Reluctant);
  facts.would_join = true;
  facts.reputation_gap = -5;
  facts.morale = 70.0f;
  EXPECT_EQ(stanceFor(facts), PlayerStance::HappyHere);
  facts.morale = 40.0f;
  EXPECT_EQ(stanceFor(facts), PlayerStance::Open);

  EXPECT_EQ(termsRefusalChance(PlayerStance::WantsBiggerClub), 0.0);
  EXPECT_GT(termsRefusalChance(PlayerStance::Reluctant),
            termsRefusalChance(PlayerStance::Open));

  const RejectionEffect keen =
      rejectionEffect(PlayerStance::WantsBiggerClub, true, 80);
  EXPECT_LT(keen.morale_delta, 0.0f);
  EXPECT_LT(keen.trust_delta, 0.0f);
  EXPECT_TRUE(keen.transfer_request);
  EXPECT_FALSE(rejectionEffect(PlayerStance::WantsBiggerClub, true, 60)
                   .transfer_request);
  EXPECT_GT(
      rejectionEffect(PlayerStance::WantsBiggerClub, false, 80).morale_delta,
      keen.morale_delta)
      << "a small bid turned down hurts less";
  const RejectionEffect happy =
      rejectionEffect(PlayerStance::HappyHere, true, 80);
  EXPECT_EQ(happy.morale_delta, 0.0f);
  EXPECT_FALSE(happy.transfer_request);

  EXPECT_TRUE(isBigBid(cash(9'000'000), 25, 10'000'000));
  EXPECT_FALSE(isBigBid(cash(5'000'000), 25, 10'000'000));
}

TEST(BuyerNegotiationTest, NeverAgreesADealItsWageBudgetCannotCarry)
{
  BuyerContext context = buyer();
  context.wage_fits = false;
  // Even a counter well within its ceiling is not agreed.
  const BuyerReply reply =
      respond(context, cash(8'000'000), cash(8'500'000), false, 0.5);
  EXPECT_EQ(reply.decision, Decision::WalkAway);
  EXPECT_TRUE(has(reply, Why::WageBudget));
  context.wage_fits = true;
  EXPECT_EQ(
      respond(context, cash(8'000'000), cash(8'500'000), false, 0.5).decision,
      Decision::Accept);
}
