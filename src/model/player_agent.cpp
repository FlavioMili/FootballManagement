// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "model/player_agent.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "model/transfer_tuning.h"
#include "model/world_simulation.h"

namespace PlayerAgent
{
namespace
{
using N = TransferTuning::Negotiation;
using Offer = TransferTuning::Offer;
using TransferNegotiation::Reason;

constexpr double WAGE_ROUNDING = 100.0;
constexpr double FEE_ROUNDING = 1'000.0;
/** His client wants a clearly bigger club: reputation points above. */
constexpr int KEEN_REPUTATION_GAP = 5;

int level(SquadRole role) { return static_cast<int>(role); }

std::uint32_t clampMoney(double amount)
{
  return static_cast<std::uint32_t>(std::clamp(
      amount, 0.0,
      static_cast<double>(std::numeric_limits<std::uint32_t>::max())));
}

std::uint32_t roundUp(double amount, double step)
{
  return clampMoney(std::ceil(amount / step) * step);
}

bool has(const std::vector<Reason>& reasons, Reason reason)
{
  return std::ranges::contains(reasons, reason);
}

std::uint8_t riseFor(const PlayerContext& context, std::uint8_t years)
{
  if (years < 2 || context.age > N::RISE_MAX_AGE) return 0;
  if (context.ambition >= N::RISE_HIGH_AMBITION) return N::RISE_HIGH;
  if (context.ambition >= N::RISE_MEDIUM_AMBITION) return N::RISE_MEDIUM;
  if (context.ambition >= N::RISE_LOW_AMBITION) return N::RISE_LOW;
  return 0;
}

const char* lineFor(const ContractResponse& response,
                    const ContractOffer& offer)
{
  const auto& reasons = response.reasons;
  if (response.accepted) return "AGENT_LINE_AGREED";
  if (has(reasons, Reason::TalksEnded)) return "AGENT_LINE_WALKED";
  if (has(reasons, Reason::ClubTooSmall)) return "AGENT_LINE_CLUB_TOO_SMALL";
  if (has(reasons, Reason::PromiseNotCredible))
    return "AGENT_LINE_PROMISE_DOUBT";
  if (has(reasons, Reason::PlayingTime)) return "AGENT_LINE_PLAYING_TIME";
  if (has(reasons, Reason::WageTooLow))
    return static_cast<double>(offer.weekly_wage) <
                   static_cast<double>(response.demand.weekly_wage) *
                       static_cast<double>(N::INSULT_WAGE_SHARE)
               ? "AGENT_LINE_INSULTED"
               : "AGENT_LINE_WAGE";
  if (has(reasons, Reason::WantsReleaseClause))
    return "AGENT_LINE_RELEASE_CLAUSE";
  if (has(reasons, Reason::ContractTooShort) ||
      has(reasons, Reason::ContractTooLong))
    return "AGENT_LINE_YEARS";
  if (has(reasons, Reason::AgentFeeTooLow)) return "AGENT_LINE_AGENT_FEE";
  return "AGENT_LINE_CLOSE";
}
}  // namespace

std::uint32_t standardAgentFee(ContractKind kind, std::uint32_t transfer_fee,
                               std::uint32_t weekly_wage)
{
  if (kind == ContractKind::Transfer && transfer_fee > 0)
    return static_cast<std::uint32_t>(static_cast<std::uint64_t>(transfer_fee) *
                                      Offer::AGENT_FEE_PERCENT / 100U);
  return clampMoney(static_cast<double>(weekly_wage) *
                    Offer::FREE_AGENT_FEE_WEEKS);
}

std::uint32_t lowestAgentFee(std::uint32_t standard)
{
  return clampMoney(std::floor(static_cast<double>(standard) *
                               static_cast<double>(N::AGENT_FEE_FLOOR) /
                               WAGE_ROUNDING) *
                    WAGE_ROUNDING);
}

std::uint32_t agentFeeAsk(const PlayerContext& context, std::uint32_t standard,
                          std::uint8_t round)
{
  const int last_round = std::max(1, N::MAX_PLAYER_ROUNDS - 1);
  if (round >= last_round) return standard;
  const double margin = static_cast<double>(N::AGENT_FEE_BASE_MARGIN) +
                        static_cast<double>(N::AGENT_FEE_AMBITION_MARGIN) *
                            (context.ambition / 100.0);
  const double remaining = 1.0 - static_cast<double>(round) / last_round;
  return std::max(standard,
                  roundUp(static_cast<double>(standard) *
                              (1.0 + margin * remaining),
                          FEE_ROUNDING));
}

Demands demands(const PlayerContext& context, std::uint32_t transfer_fee,
                std::uint8_t round, std::uint8_t years)
{
  const ContractDemand demand = TransferNegotiation::contractDemand(context);
  Demands result;
  result.flat_wage = TransferNegotiation::agentAsk(context, demand, round);
  result.desired_role = demand.desired_role;
  result.wants_release_clause = demand.wants_release_clause;
  result.max_release_clause = demand.max_release_clause;
  result.wants_promise =
      level(context.projected_role) > level(demand.desired_role);

  const std::uint8_t term = std::max<std::uint8_t>(years, 1);
  result.yearly_rise = riseFor(context, term);
  const float share = level(context.projected_role) >= level(SquadRole::Rotation)
                          ? N::APPEARANCE_BONUS_SQUAD_SHARE
                          : N::APPEARANCE_BONUS_REGULAR_SHARE;
  result.appearance_bonus = roundUp(
      static_cast<double>(result.flat_wage) * static_cast<double>(share),
      WAGE_ROUNDING);
  result.signing_bonus = demand.signing_bonus;
  if (context.kind == ContractKind::Transfer)
    result.signing_bonus += result.flat_wage * N::TRANSFER_SIGNING_WEEKS;

  // The package is worth his flat ask: the base wage makes up the rest.
  const double rise_factor =
      1.0 + TransferNegotiation::yearlyRiseWorth(1'000'000, result.yearly_rise,
                                                 term) /
                1'000'000.0;
  const double extras =
      static_cast<double>(result.appearance_bonus) *
          static_cast<double>(
              TransferNegotiation::appearanceRate(context.projected_role)) +
      static_cast<double>(result.signing_bonus - demand.signing_bonus) /
          (TransferTuning::Contract::WEEKS_PER_YEAR * term);
  result.asking_wage = std::max(
      TransferTuning::Contract::MINIMUM_WEEKLY_WAGE,
      roundUp((static_cast<double>(result.flat_wage) - extras) / rise_factor,
              WAGE_ROUNDING));

  result.standard_agent_fee =
      standardAgentFee(context.kind, transfer_fee, demand.weekly_wage);
  result.agent_fee = agentFeeAsk(context, result.standard_agent_fee, round);
  return result;
}

ContractOffer askedOffer(const Demands& demands, std::uint8_t years)
{
  ContractOffer offer;
  offer.weekly_wage = demands.asking_wage;
  offer.years = years;
  offer.signing_bonus = demands.signing_bonus;
  offer.release_clause =
      demands.wants_release_clause ? demands.max_release_clause : 0;
  offer.yearly_rise = demands.yearly_rise;
  offer.appearance_bonus = demands.appearance_bonus;
  offer.agent_fee = demands.agent_fee;
  return offer;
}

Reply respond(const PlayerContext& context, const ContractOffer& offer,
              std::uint8_t round, std::uint32_t transfer_fee)
{
  Reply reply;
  reply.response = TransferNegotiation::evaluateContract(context, offer, round);
  ContractResponse& response = reply.response;
  // On a free move the usual fee follows the wage he signs for.
  const std::uint32_t paid = offer.agent_fee.value_or(
      standardAgentFee(context.kind, transfer_fee, offer.weekly_wage));
  const std::uint32_t lowest = lowestAgentFee(standardAgentFee(
      context.kind, transfer_fee, response.demand.weekly_wage));
  if (paid < lowest)
  {
    response.reasons.push_back(Reason::AgentFeeTooLow);
    if (response.accepted)
    {
      response.accepted = false;
      std::erase(response.reasons, Reason::TermsAccepted);
      if (round + 1 >= N::MAX_PLAYER_ROUNDS)
        response.reasons.push_back(Reason::TalksEnded);
      response.demand.asking_wage = TransferNegotiation::agentAsk(
          context, response.demand,
          static_cast<std::uint8_t>(std::min(round + 1, 255)));
    }
  }
  reply.line_key = lineFor(response, offer);
  return reply;
}

const char* openingLine(const PlayerContext& context)
{
  const int gap = static_cast<int>(context.new_club_reputation) -
                  static_cast<int>(context.current_club_reputation);
  if (context.current_club_reputation > 0 &&
      context.ambition >= N::STATURE_AMBITION &&
      -gap >= N::STATURE_REFUSAL_GAP)
    return "AGENT_OPEN_RELUCTANT";
  if (context.kind == ContractKind::FreeAgent) return "AGENT_OPEN_FREE";
  if (context.current_club_reputation > 0 && gap >= KEEN_REPUTATION_GAP)
    return "AGENT_OPEN_KEEN";
  if (context.kind != ContractKind::FreeAgent &&
      context.loyalty >= N::LOYALTY_THRESHOLD)
    return "AGENT_OPEN_LOYAL";
  if (level(context.projected_role) > level(context.current_role))
    return "AGENT_OPEN_PLAYING_TIME";
  return "AGENT_OPEN_NEUTRAL";
}

const char* purchaseLine(const ClubResponse& response, bool would_join,
                         const PlayerContext& context)
{
  using Decision = ClubResponse::Decision;
  const bool keen = context.current_club_reputation > 0 &&
                    static_cast<int>(context.new_club_reputation) -
                            static_cast<int>(context.current_club_reputation) >=
                        KEEN_REPUTATION_GAP;
  // The selling club never heard the offer: nothing to say.
  if (has(response.reasons, Reason::WindowClosed) ||
      has(response.reasons, Reason::Embargo) ||
      has(response.reasons, Reason::Unavailable) ||
      has(response.reasons, Reason::OverBudget) ||
      has(response.reasons, Reason::SquadFull))
    return "";
  if (has(response.reasons, Reason::ReleaseClauseMet))
    return "AGENT_CLUB_CLAUSE";
  switch (response.decision)
  {
    case Decision::Accept:
      return would_join ? "AGENT_CLUB_AGREED" : "AGENT_CLUB_AGREED_DOUBT";
    case Decision::Counter:
      return keen ? "AGENT_CLUB_COUNTER_KEEN" : "AGENT_CLUB_COUNTER";
    case Decision::Reject:
      break;
  }
  if (has(response.reasons, Reason::NotForSale) ||
      has(response.reasons, Reason::KeyPlayerNotForSale))
    return "AGENT_CLUB_NOT_FOR_SALE";
  return would_join ? "AGENT_CLUB_REJECTED" : "AGENT_CLUB_NOT_INTERESTED";
}

}  // namespace PlayerAgent
