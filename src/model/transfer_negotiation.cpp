// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "model/transfer_negotiation.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <ranges>

#include "model/transfer_tuning.h"
#include "model/world_rng.h"
#include "model/world_simulation.h"

namespace TransferNegotiation
{
namespace
{
constexpr std::array<const char*, static_cast<std::size_t>(Reason::COUNT)>
    REASON_KEYS = {
        "NEG_REASON_KEY_PLAYER",
        "NEG_REASON_FIRST_TEAM",
        "NEG_REASON_RIVAL",
        "NEG_REASON_MID_SEASON",
        "NEG_REASON_LATE_WINDOW",
        "NEG_REASON_RICHER_BUYER",
        "NEG_REASON_LISTED",
        "NEG_REASON_NOT_FOR_SALE",
        "NEG_REASON_RELEASE_CLAUSE_MET",
        "NEG_REASON_OFFER_TOO_LOW",
        "NEG_REASON_TOO_LITTLE_UPFRONT",
        "NEG_REASON_COUNTER",
        "NEG_REASON_TALKS_BROKEN",
        "NEG_REASON_OFFER_ACCEPTED",
        "NEG_REASON_WAGE_TOO_LOW",
        "NEG_REASON_CONTRACT_TOO_SHORT",
        "NEG_REASON_CONTRACT_TOO_LONG",
        "NEG_REASON_CLUB_TOO_SMALL",
        "NEG_REASON_PLAYING_TIME",
        "NEG_REASON_PROMISE_NOT_CREDIBLE",
        "NEG_REASON_WANTS_RELEASE_CLAUSE",
        "NEG_REASON_LOYAL",
        "NEG_REASON_BIGGER_CLUB",
        "NEG_REASON_TERMS_ACCEPTED",
        "NEG_REASON_TALKS_ENDED",
        "NEG_REASON_NOT_FOR_LOAN",
        "NEG_REASON_WAGE_SHARE_LOW",
        "NEG_REASON_OPTION_TOO_LOW",
        "NEG_REASON_LOAN_LIMIT",
        "NEG_REASON_NO_PLAYING_TIME_LOAN",
        "NEG_REASON_WINDOW_CLOSED",
        "NEG_REASON_UNAVAILABLE",
        "NEG_REASON_OVER_BUDGET",
};

constexpr std::uint32_t FEE_ROUNDING = 10'000;
constexpr int MONTH_JUNE = 6;
constexpr int SEASON_END_DAY = 30;

int level(SquadRole role) { return static_cast<int>(role); }

std::uint32_t roundFeeUp(double fee)
{
  const double rounded = std::ceil(std::max(fee, 0.0) / FEE_ROUNDING) *
                         static_cast<double>(FEE_ROUNDING);
  return static_cast<std::uint32_t>(std::min(
      rounded, static_cast<double>(std::numeric_limits<std::uint32_t>::max())));
}

float rolePremium(SquadRole role)
{
  using Valuation = TransferTuning::Valuation;
  switch (role)
  {
    case SquadRole::KeyPlayer:
      return Valuation::KEY_PLAYER_PREMIUM;
    case SquadRole::FirstTeam:
      return Valuation::FIRST_TEAM_PREMIUM;
    case SquadRole::Rotation:
      return Valuation::ROTATION_PREMIUM;
    case SquadRole::Backup:
      return Valuation::BACKUP_PREMIUM;
    case SquadRole::Fringe:
      return Valuation::FRINGE_PREMIUM;
  }
  return 1.0f;
}

bool isRefusal(Reason reason)
{
  switch (reason)
  {
    case Reason::WageTooLow:
    case Reason::ContractTooShort:
    case Reason::ContractTooLong:
    case Reason::ClubTooSmall:
    case Reason::PlayingTime:
    case Reason::WantsReleaseClause:
    case Reason::TalksEnded:
      return true;
    default:
      return false;
  }
}

/** Smallest fee with the same structure that the seller values at
 * @p target (the value is monotonic in the fee). */
std::uint32_t feeForValue(const OfferTerms& terms, double target, int age)
{
  OfferTerms probe = terms;
  double low = 0.0;
  double high = std::max(target * 4.0, 1.0);
  for (int iteration = 0; iteration < 48; ++iteration)
  {
    const double mid = 0.5 * (low + high);
    probe.fee = static_cast<std::uint32_t>(std::min(mid, 4.0e9));
    if (sellerValue(probe, age) >= target)
      high = mid;
    else
      low = mid;
  }
  return roundFeeUp(high);
}
}  // namespace

const char* reasonKey(Reason reason)
{
  const auto index = static_cast<std::size_t>(reason);
  return index < REASON_KEYS.size() ? REASON_KEYS[index] : "";
}

bool isValid(const OfferTerms& terms)
{
  using Offer = TransferTuning::Offer;
  if (terms.upfront_percent < Offer::MIN_UPFRONT_PERCENT ||
      terms.upfront_percent > 100 ||
      terms.instalment_years > Offer::MAX_INSTALMENT_YEARS ||
      terms.sell_on_percent > Offer::MAX_SELL_ON_PERCENT)
    return false;
  if (terms.upfront_percent < 100 && terms.instalment_years == 0) return false;
  if (terms.appearance_bonus > 0 && terms.appearance_target == 0) return false;
  if (terms.goal_bonus > 0 && terms.goal_target == 0) return false;
  return true;
}

std::uint32_t upfrontAmount(const OfferTerms& terms)
{
  if (terms.instalment_years == 0) return terms.fee;
  return static_cast<std::uint32_t>(static_cast<std::uint64_t>(terms.fee) *
                                    terms.upfront_percent / 100U);
}

std::vector<std::uint32_t> instalmentAmounts(const OfferTerms& terms)
{
  std::vector<std::uint32_t> amounts;
  const std::uint32_t deferred = terms.fee - upfrontAmount(terms);
  if (terms.instalment_years == 0 || deferred == 0) return amounts;
  const std::uint32_t share = deferred / terms.instalment_years;
  amounts.assign(terms.instalment_years, share);
  amounts.back() += deferred - share * terms.instalment_years;
  return amounts;
}

OfferTerms aiOfferTerms(std::uint32_t fee)
{
  using Offer = TransferTuning::Offer;
  OfferTerms terms;
  terms.fee = fee;
  if (fee >= Offer::AI_INSTALMENT_THRESHOLD)
  {
    terms.upfront_percent = Offer::AI_UPFRONT_PERCENT;
    terms.instalment_years = fee >= 4 * Offer::AI_INSTALMENT_THRESHOLD ? 3 : 2;
  }
  return terms;
}

Valuation valueForSale(const SaleContext& context)
{
  using V = TransferTuning::Valuation;
  Valuation valuation;
  if (context.listed && context.listing_price > 0)
  {
    valuation.asking_fee = context.listing_price;
    valuation.reasons.push_back(Reason::Listed);
  }
  else
  {
    double fee = static_cast<double>(context.market_value) *
                 static_cast<double>(rolePremium(context.role));
    if (context.role == SquadRole::KeyPlayer)
      valuation.reasons.push_back(Reason::KeyPlayer);
    else if (context.role == SquadRole::FirstTeam)
      valuation.reasons.push_back(Reason::FirstTeamPlayer);

    const int raw_gap = static_cast<int>(context.buyer_reputation) -
                        static_cast<int>(context.seller_reputation);
    const int gap = std::clamp(raw_gap, V::BUYER_REPUTATION_GAP_MIN,
                               V::BUYER_REPUTATION_GAP_MAX);
    if (gap > 0)
    {
      fee *= 1.0 + static_cast<double>(V::BUYER_REPUTATION_SLOPE) * gap;
      if (gap >= 5) valuation.reasons.push_back(Reason::RicherBuyer);
    }
    if (context.same_league && std::abs(raw_gap) <= V::RIVAL_REPUTATION_GAP)
    {
      fee *= static_cast<double>(V::RIVAL_PREMIUM);
      valuation.reasons.push_back(Reason::SellingToRival);
    }
    if (context.winter_window &&
        level(context.role) <= level(SquadRole::FirstTeam))
    {
      fee *= static_cast<double>(V::MID_SEASON_PREMIUM);
      valuation.reasons.push_back(Reason::MidSeason);
    }
    if (context.days_to_deadline >= 0 &&
        context.days_to_deadline <= V::LATE_WINDOW_DAYS &&
        level(context.role) <= level(SquadRole::Rotation))
    {
      fee *= static_cast<double>(V::LATE_WINDOW_PREMIUM);
      valuation.reasons.push_back(Reason::LateWindow);
    }
    valuation.not_for_sale =
        context.role == SquadRole::KeyPlayer &&
        context.contract_years >= V::NOT_FOR_SALE_MIN_CONTRACT_YEARS &&
        raw_gap <= -V::NOT_FOR_SALE_REPUTATION_GAP;
    if (valuation.not_for_sale) valuation.reasons.push_back(Reason::NotForSale);
    valuation.asking_fee = std::max(roundFeeUp(fee), FEE_ROUNDING);
  }
  if (context.release_clause > 0)
    valuation.asking_fee =
        std::min(valuation.asking_fee, context.release_clause);
  return valuation;
}

double sellerValue(const OfferTerms& terms, int age)
{
  using Offer = TransferTuning::Offer;
  const auto fee = static_cast<double>(terms.fee);
  const auto upfront = static_cast<double>(upfrontAmount(terms));
  const double average_delay =
      0.5 * static_cast<double>(terms.instalment_years + 1);
  const double deferred_factor = std::max(
      0.0, 1.0 - static_cast<double>(Offer::DEFERRED_DISCOUNT_PER_YEAR) *
                     average_delay);
  double add_ons = 0.0;
  if (terms.appearance_target > 0) add_ons += terms.appearance_bonus;
  if (terms.goal_target > 0) add_ons += terms.goal_bonus;
  add_ons = std::min(add_ons * static_cast<double>(Offer::ADD_ON_WEIGHT),
                     fee * static_cast<double>(Offer::ADD_ON_CREDIT_CAP));
  const float sell_on_weight = age <= Offer::SELL_ON_YOUNG_MAX_AGE
                                   ? Offer::SELL_ON_WEIGHT_YOUNG
                                   : Offer::SELL_ON_WEIGHT_SENIOR;
  const double sell_on = std::min(
      fee * terms.sell_on_percent / 100.0 * static_cast<double>(sell_on_weight),
      fee * static_cast<double>(Offer::SELL_ON_CREDIT_CAP));
  return upfront + (fee - upfront) * deferred_factor + add_ons + sell_on;
}

ClubResponse evaluateOffer(const SaleContext& context, const OfferTerms& terms,
                           std::uint8_t round)
{
  using Offer = TransferTuning::Offer;
  ClubResponse response;
  if (!isValid(terms))
  {
    response.reasons.push_back(terms.upfront_percent <
                                       Offer::MIN_UPFRONT_PERCENT
                                   ? Reason::TooLittleUpfront
                                   : Reason::OfferTooLow);
    return response;
  }
  if (context.release_clause > 0 && terms.fee >= context.release_clause &&
      terms.instalment_years == 0)
  {
    response.decision = ClubResponse::Decision::Accept;
    response.reasons.push_back(Reason::ReleaseClauseMet);
    return response;
  }

  const Valuation valuation = valueForSale(context);
  response.reasons = valuation.reasons;
  if (valuation.not_for_sale) return response;

  const auto asking = static_cast<double>(valuation.asking_fee);
  const double value = sellerValue(terms, context.age);
  if (value >= asking * static_cast<double>(Offer::ACCEPT_SHARE))
  {
    response.decision = ClubResponse::Decision::Accept;
    response.reasons.push_back(Reason::OfferAccepted);
    return response;
  }
  response.counter_fee = feeForValue(terms, asking, context.age);
  if (round + 1 >= Offer::MAX_CLUB_ROUNDS)
  {
    response.reasons.push_back(Reason::TalksBroken);
    return response;
  }
  if (value < asking * static_cast<double>(Offer::REJECT_SHARE))
  {
    response.reasons.push_back(Reason::OfferTooLow);
    return response;
  }
  response.decision = ClubResponse::Decision::Counter;
  response.reasons.push_back(Reason::CounterOffer);
  return response;
}

// ---------------------------------------------------------------------------
// Player contracts
// ---------------------------------------------------------------------------

std::uint8_t maxContractYears(int age)
{
  using Contract = TransferTuning::Contract;
  return age < Contract::MINOR_AGE ? Contract::MINOR_MAXIMUM_YEARS
                                   : Contract::MAXIMUM_YEARS;
}

ContractDemand contractDemand(const PlayerContext& context)
{
  using Contract = TransferTuning::Contract;
  using N = TransferTuning::Negotiation;
  ContractDemand demand;
  const double ambition = context.ambition / 100.0;

  double raise = static_cast<double>(Contract::FREE_AGENT_WAGE_RAISE);
  if (context.kind == ContractKind::Transfer ||
      context.kind == ContractKind::PreContract)
    raise = static_cast<double>(N::BASE_TRANSFER_RAISE) +
            static_cast<double>(N::AMBITION_RAISE) * ambition;
  else if (context.kind == ContractKind::Renewal)
    raise =
        static_cast<double>(Contract::FREE_AGENT_WAGE_RAISE) + 0.10 * ambition;
  double wage = static_cast<double>(context.current_wage) * raise;

  if (context.current_club_reputation > 0 &&
      context.kind != ContractKind::Renewal)
  {
    const int gap = static_cast<int>(context.current_club_reputation) -
                    static_cast<int>(context.new_club_reputation);
    if (gap > 0)
      wage *= 1.0 + static_cast<double>(N::STATURE_WAGE_SLOPE) * gap * ambition;
    else
      wage *= 1.0 - 0.005 * std::min(20, -gap);
  }
  const bool leaving = context.kind == ContractKind::Transfer ||
                       context.kind == ContractKind::PreContract;
  if (leaving && context.loyalty >= N::LOYALTY_THRESHOLD)
    wage *= static_cast<double>(N::LOYALTY_PREMIUM);
  if (context.unsettled && context.kind != ContractKind::Renewal)
    wage *= static_cast<double>(N::UNSETTLED_DISCOUNT);
  demand.weekly_wage = std::max(Contract::MINIMUM_WEEKLY_WAGE,
                                static_cast<std::uint32_t>(std::ceil(wage)));

  demand.max_years = maxContractYears(context.age);
  std::uint8_t min_years = Contract::VETERAN_MINIMUM_YEARS;
  if (context.age <= Contract::YOUNG_PLAYER_MAXIMUM_AGE)
    min_years = Contract::YOUNG_PLAYER_MINIMUM_YEARS;
  else if (context.age <= Contract::PRIME_PLAYER_MAXIMUM_AGE)
    min_years = Contract::PRIME_PLAYER_MINIMUM_YEARS;
  demand.min_years = std::min(min_years, demand.max_years);

  if (context.kind == ContractKind::FreeAgent ||
      context.kind == ContractKind::PreContract)
    demand.signing_bonus = demand.weekly_wage * N::FREE_SIGNING_BONUS_WEEKS;

  int desired = level(context.current_role);
  if (context.ambition >= N::STATURE_AMBITION && desired > 0) --desired;
  demand.desired_role = static_cast<SquadRole>(desired);

  demand.wants_release_clause =
      context.kind != ContractKind::Renewal &&
      context.ambition >= N::RELEASE_CLAUSE_AMBITION &&
      context.new_club_reputation < N::RELEASE_CLAUSE_CLUB_REPUTATION;
  if (demand.wants_release_clause)
    demand.max_release_clause =
        roundFeeUp(static_cast<double>(context.market_value) *
                   static_cast<double>(N::RELEASE_CLAUSE_VALUE_MULTIPLE));
  return demand;
}

ContractResponse evaluateContract(const PlayerContext& context,
                                  const ContractOffer& offer,
                                  std::uint8_t round)
{
  using N = TransferTuning::Negotiation;
  ContractResponse response;
  response.demand = contractDemand(context);
  const ContractDemand& demand = response.demand;
  auto& reasons = response.reasons;

  if (offer.years == 0)
    reasons.push_back(Reason::ContractTooShort);
  else if (offer.years > demand.max_years)
    reasons.push_back(Reason::ContractTooLong);
  else if (offer.years < demand.min_years)
    reasons.push_back(Reason::ContractTooShort);

  if (context.current_club_reputation > 0 &&
      context.kind != ContractKind::Renewal)
  {
    const int gap = static_cast<int>(context.current_club_reputation) -
                    static_cast<int>(context.new_club_reputation);
    if (context.ambition >= N::STATURE_AMBITION &&
        gap >= N::STATURE_REFUSAL_GAP)
      reasons.push_back(Reason::ClubTooSmall);
    else if (gap < 0)
      reasons.push_back(Reason::BiggerClub);
  }
  if ((context.kind == ContractKind::Transfer ||
       context.kind == ContractKind::PreContract) &&
      context.loyalty >= N::LOYALTY_THRESHOLD)
    reasons.push_back(Reason::LoyalToClub);

  const int projected = level(context.projected_role);
  int effective = projected;
  if (offer.promised_role)
  {
    const int promised = level(*offer.promised_role);
    const int credible = std::max(0, projected - N::PROMISE_CREDIBILITY_LEVELS);
    if (promised < credible) reasons.push_back(Reason::PromiseNotCredible);
    effective = std::min(projected, std::max(promised, credible));
  }
  if (effective > level(demand.desired_role) + 1)
    reasons.push_back(Reason::PlayingTime);

  if (demand.wants_release_clause &&
      (offer.release_clause == 0 ||
       offer.release_clause > demand.max_release_clause))
    reasons.push_back(Reason::WantsReleaseClause);

  if (offer.years > 0)
  {
    const double bonus_delta = static_cast<double>(offer.signing_bonus) -
                               static_cast<double>(demand.signing_bonus);
    const double effective_wage =
        static_cast<double>(offer.weekly_wage) +
        bonus_delta / (TransferTuning::Contract::WEEKS_PER_YEAR * offer.years);
    if (effective_wage + 0.5 < static_cast<double>(demand.weekly_wage))
      reasons.push_back(Reason::WageTooLow);
  }

  response.accepted = std::ranges::none_of(reasons, isRefusal);
  if (!response.accepted && round + 1 >= N::MAX_PLAYER_ROUNDS)
    reasons.push_back(Reason::TalksEnded);
  if (response.accepted) reasons.push_back(Reason::TermsAccepted);
  return response;
}

ContractOffer demandedOffer(const ContractDemand& demand)
{
  ContractOffer offer;
  offer.weekly_wage = demand.weekly_wage;
  offer.years = demand.min_years;
  offer.signing_bonus = demand.signing_bonus;
  offer.release_clause =
      demand.wants_release_clause ? demand.max_release_clause : 0;
  return offer;
}

// ---------------------------------------------------------------------------
// Loans
// ---------------------------------------------------------------------------

ClubResponse evaluateLoan(const LoanContext& context, const LoanTerms& terms)
{
  using L = TransferTuning::Loan;
  ClubResponse response;
  if (!context.within_limits)
  {
    response.reasons.push_back(Reason::LoanLimit);
    return response;
  }
  if (!context.loan_listed &&
      level(context.role) <= level(SquadRole::FirstTeam))
  {
    response.reasons.push_back(Reason::NotForLoan);
    return response;
  }

  const int required =
      context.loan_listed ? L::LISTED_WAGE_SHARE : L::UNLISTED_WAGE_SHARE;
  const double weekly_total =
      static_cast<double>(context.weekly_wage) * std::max(1, context.weeks);
  const double shortfall =
      weekly_total * std::max(0, required - terms.wage_share) / 100.0;
  const bool wages_covered = static_cast<double>(terms.loan_fee) >= shortfall;

  const double minimum_option =
      static_cast<double>(context.market_value) *
      static_cast<double>(terms.obligation ? L::OBLIGATION_VALUE_MULTIPLE
                                           : L::OPTION_VALUE_MULTIPLE);
  const bool wants_option = terms.option_fee > 0 || terms.obligation;
  const bool option_ok =
      !wants_option || static_cast<double>(terms.option_fee) >= minimum_option;

  if (wages_covered && option_ok && terms.wage_share <= 100)
  {
    response.decision = ClubResponse::Decision::Accept;
    response.reasons.push_back(Reason::OfferAccepted);
    return response;
  }
  response.decision = ClubResponse::Decision::Counter;
  if (!wages_covered)
  {
    const double fee_share =
        weekly_total > 0.0 ? terms.loan_fee * 100.0 / weekly_total : 0.0;
    response.counter_wage_share = static_cast<std::uint8_t>(
        std::clamp(static_cast<int>(std::ceil(required - fee_share)), 0, 100));
    response.reasons.push_back(Reason::WageShareTooLow);
  }
  else
  {
    response.counter_wage_share = terms.wage_share;
  }
  if (!option_ok)
  {
    response.counter_option_fee = roundFeeUp(minimum_option);
    response.reasons.push_back(Reason::OptionTooLow);
  }
  else
  {
    response.counter_option_fee = terms.option_fee;
  }
  return response;
}

bool playerAcceptsLoan(SquadRole at_parent, SquadRole at_borrower,
                       std::uint8_t ambition, int reputation_gap,
                       std::vector<Reason>& reasons)
{
  using N = TransferTuning::Negotiation;
  bool accepted = true;
  if (level(at_borrower) > level(SquadRole::Rotation) ||
      (at_parent != SquadRole::Fringe && level(at_borrower) > level(at_parent)))
  {
    reasons.push_back(Reason::NoPlayingTimeOnLoan);
    accepted = false;
  }
  if (ambition >= N::STATURE_AMBITION &&
      reputation_gap >= N::STATURE_REFUSAL_GAP + 10)
  {
    reasons.push_back(Reason::ClubTooSmall);
    accepted = false;
  }
  return accepted;
}

// ---------------------------------------------------------------------------
// Calendar helpers
// ---------------------------------------------------------------------------

WindowInfo windowInfo(const GameDateValue& date)
{
  WindowInfo window;
  window.open = date.isTransferWindowOpen();
  if (!window.open)
  {
    window.days_to_deadline = -1;
    return window;
  }
  window.winter = date.month == 1;
  constexpr int LONGEST_WINDOW_DAYS = 120;
  GameDateValue next = date + 1;
  while (next.isTransferWindowOpen() &&
         window.days_to_deadline < LONGEST_WINDOW_DAYS)
  {
    ++window.days_to_deadline;
    next = next + 1;
  }
  return window;
}

float activityWeight(const WindowInfo& window)
{
  using M = TransferTuning::Market;
  if (!window.open) return 0.0f;
  if (window.days_to_deadline == 0) return M::DEADLINE_DAY_WEIGHT;
  if (window.days_to_deadline <= M::LATE_WINDOW_DAYS)
    return M::LATE_WINDOW_WEIGHT;
  return 1.0f;
}

bool canSignPreContract(std::uint8_t contract_years, const GameDateValue& date)
{
  return contract_years == 1 && date.month >= 1 && date.month <= MONTH_JUNE;
}

GameDateValue seasonEndDate(const GameDateValue& date)
{
  const auto year = static_cast<std::uint16_t>(
      date.month > MONTH_JUNE ? date.year + 1 : date.year);
  return GameDateValue(year, MONTH_JUNE, SEASON_END_DAY);
}

GameDateValue loanEndDate(const GameDateValue& start, LoanDuration duration)
{
  // A loan agreed in the June part of the summer window covers the
  // season that starts on 1 July.
  const GameDateValue end =
      start.month == MONTH_JUNE
          ? GameDateValue(static_cast<std::uint16_t>(start.year + 1),
                          MONTH_JUNE, SEASON_END_DAY)
          : seasonEndDate(start);
  if (duration == LoanDuration::SeasonEnd) return end;
  const GameDateValue half =
      start + static_cast<std::size_t>(TransferTuning::Loan::SIX_MONTH_DAYS);
  return half < end ? half : end;
}

int weeksBetween(const GameDateValue& from, const GameDateValue& to)
{
  return std::max(0, (dayOrdinal(to) - dayOrdinal(from)) / 7);
}

std::int64_t severancePay(std::uint32_t weekly_wage,
                          std::uint8_t contract_years,
                          const GameDateValue& date)
{
  if (contract_years == 0) return 0;
  const int weeks = weeksBetween(date, seasonEndDate(date)) +
                    static_cast<int>(TransferTuning::Contract::WEEKS_PER_YEAR) *
                        (contract_years - 1);
  return static_cast<std::int64_t>(weekly_wage) * weeks;
}

SquadRole roleForRank(std::size_t rank)
{
  if (rank < 5) return SquadRole::KeyPlayer;
  if (rank < 11) return SquadRole::FirstTeam;
  if (rank < 16) return SquadRole::Rotation;
  if (rank < 22) return SquadRole::Backup;
  return SquadRole::Fringe;
}

}  // namespace TransferNegotiation
