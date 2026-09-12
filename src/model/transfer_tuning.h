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

/**
 * Named transfer-market and contract-negotiation tuning values.
 *
 * [S] values are anchored to the research notes (FIFA RSTP, FIFA transfer
 * reports, CIES, UEFA ECFIL); [P] values are designer priors consistent with
 * them (world-realism.md 9.3, rules-regulations.md 3).
 */
struct TransferTuning final
{
  struct Contract final
  {
    static constexpr std::uint8_t MINIMUM_YEARS = 1;
    /** RSTP: at most 5 years, 3 for players under 18. [S] */
    static constexpr std::uint8_t MAXIMUM_YEARS = 5;
    static constexpr std::uint8_t MINOR_MAXIMUM_YEARS = 3;
    static constexpr int MINOR_AGE = 18;
    static constexpr int YOUNG_PLAYER_MAXIMUM_AGE = 23;
    static constexpr int PRIME_PLAYER_MAXIMUM_AGE = 29;
    static constexpr std::uint8_t YOUNG_PLAYER_MINIMUM_YEARS = 3;
    static constexpr std::uint8_t PRIME_PLAYER_MINIMUM_YEARS = 2;
    static constexpr std::uint8_t VETERAN_MINIMUM_YEARS = 1;
    static constexpr float TRANSFER_WAGE_RAISE = 1.10f;
    static constexpr float FREE_AGENT_WAGE_RAISE = 1.05f;
    static constexpr std::uint32_t MINIMUM_WEEKLY_WAGE = 500;
    static constexpr std::int64_t MINIMUM_WEEKLY_WAGE_BUDGET = 100'000;
    static constexpr double WEEKS_PER_YEAR = 52.0;
  };

  /** Selling club's valuation of its player. */
  struct Valuation final
  {
    /** Asking premium by playing-time role (key players cost most). [P] */
    static constexpr float KEY_PLAYER_PREMIUM = 1.40f;
    static constexpr float FIRST_TEAM_PREMIUM = 1.18f;
    static constexpr float ROTATION_PREMIUM = 1.0f;
    static constexpr float BACKUP_PREMIUM = 0.92f;
    static constexpr float FRINGE_PREMIUM = 0.85f;
    /** Richer buyers pay more (CIES; psi ~0.2-0.3): +1% per reputation
     * point above the seller, clamped. [P] */
    static constexpr float BUYER_REPUTATION_SLOPE = 0.01f;
    static constexpr int BUYER_REPUTATION_GAP_MIN = -20;
    static constexpr int BUYER_REPUTATION_GAP_MAX = 30;
    /** Rival: same league and reputation within this gap. [P] */
    static constexpr int RIVAL_REPUTATION_GAP = 8;
    static constexpr float RIVAL_PREMIUM = 1.25f;
    /** Mid-season (winter window) reluctance for regular starters. [P] */
    static constexpr float MID_SEASON_PREMIUM = 1.20f;
    /** Last days of a window: no time to find a replacement. [P] */
    static constexpr int LATE_WINDOW_DAYS = 3;
    static constexpr float LATE_WINDOW_PREMIUM = 1.10f;
    /** A key player on a long deal is not sold to a clearly smaller club. */
    static constexpr int NOT_FOR_SALE_REPUTATION_GAP = 10;
    static constexpr std::uint8_t NOT_FOR_SALE_MIN_CONTRACT_YEARS = 3;
  };

  /** Structure of a club-to-club offer and the seller's reading of it. */
  struct Offer final
  {
    /** Offers worth at least this share of the asking fee are accepted. */
    static constexpr float ACCEPT_SHARE = 0.97f;
    /** Below this share of the asking fee the seller walks away. [P] */
    static constexpr float REJECT_SHARE = 0.60f;
    /** Seller discount of deferred money per year of delay. [P] */
    static constexpr float DEFERRED_DISCOUNT_PER_YEAR = 0.06f;
    /** Probability weight of conditional add-ons, and their credit cap as a
     * share of the base fee (add-ons are 10-25% of fees). [P] */
    static constexpr float ADD_ON_WEIGHT = 0.4f;
    static constexpr float ADD_ON_CREDIT_CAP = 0.25f;
    /** Sell-on clauses (10-20% typical, capped at 30%), valued by the
     * seller as a share of the fee times this weight (young players resell
     * more often). Credit capped at a share of the fee. [P] */
    static constexpr std::uint8_t MAX_SELL_ON_PERCENT = 30;
    static constexpr float SELL_ON_WEIGHT_YOUNG = 0.5f;
    static constexpr float SELL_ON_WEIGHT_SENIOR = 0.2f;
    static constexpr int SELL_ON_YOUNG_MAX_AGE = 23;
    static constexpr float SELL_ON_CREDIT_CAP = 0.10f;
    /** Instalments: 40-60% upfront, rest over 2-4 years. [P] */
    static constexpr std::uint8_t MIN_UPFRONT_PERCENT = 20;
    static constexpr std::uint8_t MAX_INSTALMENT_YEARS = 4;
    static constexpr std::uint8_t AI_UPFRONT_PERCENT = 50;
    /** AI deals above this fee are spread over instalments. [P] */
    static constexpr std::uint32_t AI_INSTALMENT_THRESHOLD = 5'000'000;
    /** Counter-offers before the seller ends the talks. [P] */
    static constexpr std::uint8_t MAX_CLUB_ROUNDS = 4;
    /** An agreed fee stays valid this long for the contract talks. */
    static constexpr int AGREEMENT_VALID_DAYS = 7;
    /** Incoming offers expire after this many days without an answer. */
    static constexpr int INCOMING_OFFER_DAYS = 5;
    /** Club agent fees are ~10% of fee spending (FIFA 2025). [S] */
    static constexpr std::uint32_t AGENT_FEE_PERCENT = 10;
    /** Agent fee on a free move, in weeks of the new wage. [P] */
    static constexpr std::uint32_t FREE_AGENT_FEE_WEEKS = 8;
  };

  /** Player side of a contract negotiation. */
  struct Negotiation final
  {
    /** Proposals a player considers before ending the talks. [P] */
    static constexpr std::uint8_t MAX_PLAYER_ROUNDS = 3;
    /** Wage raise asked for a transfer, scaled up by ambition. [P] */
    static constexpr float BASE_TRANSFER_RAISE = 1.10f;
    static constexpr float AMBITION_RAISE = 0.15f;
    /** Free agents and pre-contracts ask for a signing bonus instead of
     * a fee, in weeks of the asked wage. [P] */
    static constexpr std::uint32_t FREE_SIGNING_BONUS_WEEKS = 12;
    /** Extra wage asked per reputation point the new club is below the
     * current one, times ambition (0-1). [P] */
    static constexpr float STATURE_WAGE_SLOPE = 0.03f;
    /** Ambitious players refuse clubs this far below their level. [P] */
    static constexpr int STATURE_REFUSAL_GAP = 25;
    static constexpr std::uint8_t STATURE_AMBITION = 70;
    /** Loyal players ask more to leave their club. [P] */
    static constexpr std::uint8_t LOYALTY_THRESHOLD = 75;
    static constexpr float LOYALTY_PREMIUM = 1.10f;
    /** Players unsettled by interest accept a little less. [P] */
    static constexpr float UNSETTLED_DISCOUNT = 0.95f;
    /** Ambitious players moving to a modest club want a release clause of
     * at most this multiple of their market value. [P] */
    static constexpr std::uint8_t RELEASE_CLAUSE_AMBITION = 70;
    static constexpr std::uint8_t RELEASE_CLAUSE_CLUB_REPUTATION = 75;
    static constexpr float RELEASE_CLAUSE_VALUE_MULTIPLE = 2.5f;
    /** Promises are believed up to one level above the projected role. */
    static constexpr int PROMISE_CREDIBILITY_LEVELS = 1;
    /** A promise broken by this many levels after the grace period costs
     * morale. [P] */
    static constexpr int BROKEN_PROMISE_LEVELS = 2;
    static constexpr int PROMISE_GRACE_DAYS = 90;
    static constexpr float BROKEN_PROMISE_MORALE = 12.0f;
  };

  /** Loans (RSTP Art. 10 from 2024/25). */
  struct Loan final
  {
    /** At most 6 out and 6 in at a time; players aged 21 or younger do
     * not count. [S] */
    static constexpr int MAX_LOANS_OUT = 6;
    static constexpr int MAX_LOANS_IN = 6;
    static constexpr int EXEMPT_MAX_AGE = 21;
    /** At most 3 loans between the same two clubs. [S] */
    static constexpr int MAX_BETWEEN_CLUBS = 3;
    static constexpr int SIX_MONTH_DAYS = 182;
    /** Wage share the parent expects for a loan-listed player, and for a
     * player it did not offer. [P] */
    static constexpr std::uint8_t LISTED_WAGE_SHARE = 50;
    static constexpr std::uint8_t UNLISTED_WAGE_SHARE = 80;
    /** Option / obligation fees must reach these multiples of value. */
    static constexpr float OPTION_VALUE_MULTIPLE = 1.10f;
    static constexpr float OBLIGATION_VALUE_MULTIPLE = 1.0f;
    /** Loanees above this age are rarely wanted by the parent back. */
    static constexpr int PROSPECT_MAX_AGE = 23;
  };

  /** AI market activity. */
  struct Market final
  {
    /** Clubs evaluated per window day and completed deals per day. The
     * deadline-day spike carries ~10-15% of window spend. [S/P] */
    static constexpr int BASE_TEAM_EVALUATIONS = 24;
    static constexpr int BASE_DAILY_DEALS = 16;
    static constexpr float LATE_WINDOW_WEIGHT = 1.8f;
    static constexpr float DEADLINE_DAY_WEIGHT = 6.0f;
    static constexpr int LATE_WINDOW_DAYS = 7;
    /** Free-agent signings per day while the window is shut. */
    static constexpr int CLOSED_WINDOW_FREE_SIGNINGS = 2;
    /** Pre-contract approaches per day from 1 January. */
    static constexpr int DAILY_PRE_CONTRACTS = 2;
    /** Squad size AI clubs aim for; above it they loan-list prospects,
     * two above it they release veterans, and they stop buying at
     * AI_MAX_SQUAD + 2. [P] */
    static constexpr std::size_t AI_TARGET_SQUAD = 26;
    static constexpr std::size_t AI_MAX_SQUAD = 30;
    static constexpr int MAX_LOAN_LISTED_PER_CLUB = 4;
    /** Clubs sounded out when placing a loan-listed prospect. */
    static constexpr int LOAN_PLACEMENT_TRIES = 8;
    /** AI clubs loan out prospects ranked beyond the matchday squad. */
    static constexpr std::size_t LOAN_OUT_MIN_RANK = 18;
    /** Veterans beyond this rank and age are released by AI clubs. */
    static constexpr std::size_t RELEASE_MIN_RANK = 22;
    static constexpr int RELEASE_MIN_AGE = 30;
    /** A weakest starter this far below the squad level is an upgrade
     * need; a signing must beat him by UPGRADE_MARGIN. A missing position
     * accepts players down to SHORTAGE_LEVEL_MARGIN below the level. [P] */
    static constexpr float UPGRADE_DEFICIT = 2.0f;
    static constexpr float UPGRADE_MARGIN = 1.0f;
    static constexpr float SHORTAGE_LEVEL_MARGIN = 12.0f;
    /** Share of needs a club first tries to fill with a loan. [P] */
    static constexpr double LOAN_BEFORE_FEE_CHANCE = 0.7;
    /** A loanee may be this far below the level asked of a signing. [P] */
    static constexpr float LOAN_LEVEL_SLACK = 4.0f;
    /** Fee route: players sampled beyond the list, offers tried, and the
     * most an AI club pays relative to market value. [P] */
    static constexpr std::size_t AI_BUY_SAMPLE = 120;
    static constexpr std::size_t AI_BUY_ATTEMPTS = 6;
    static constexpr double AI_MAX_VALUE_MULTIPLE = 1.6;
    /** Chance per evaluated club that an AI club approaches one of the
     * managed club's players. [P] */
    static constexpr float MANAGED_APPROACH_CHANCE = 0.04f;
    /** Transfer news kept for the feed. */
    static constexpr std::size_t NEWS_DIGEST_LIMIT = 5;
  };
};
