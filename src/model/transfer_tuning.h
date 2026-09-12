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
    /** Asking premium by playing-time role: nobody sells at market value,
     * key players cost most (10-40% above value). [P] */
    static constexpr float KEY_PLAYER_PREMIUM = 1.40f;
    static constexpr float FIRST_TEAM_PREMIUM = 1.25f;
    static constexpr float ROTATION_PREMIUM = 1.15f;
    static constexpr float BACKUP_PREMIUM = 1.12f;
    static constexpr float FRINGE_PREMIUM = 1.08f;
    /** Contract length: each year beyond LONG_CONTRACT_BASE_YEARS adds a
     * premium (no need to sell); a final-year player is cheaper (CIES:
     * expiring contracts are the strongest discount). [P] */
    static constexpr std::uint8_t LONG_CONTRACT_BASE_YEARS = 2;
    static constexpr float LONG_CONTRACT_PREMIUM_PER_YEAR = 0.05f;
    static constexpr float EXPIRING_CONTRACT_FACTOR = 0.90f;
    /** Share of key players on multi-year deals whose club refuses any bid
     * in a given window (drawn per player and window). [P] */
    static constexpr float KEY_PLAYER_REFUSAL_SHARE = 0.30f;
    static constexpr std::uint8_t KEY_PLAYER_REFUSAL_MIN_YEARS = 2;
    /** A clearly bigger buyer can still prise a key player away. */
    static constexpr int KEY_PLAYER_REFUSAL_BUYER_GAP = 10;
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
    /** An opening bid must reach the full valuation; once talks are under
     * way offers worth this share of it close the deal. */
    static constexpr float OPENING_ACCEPT_SHARE = 1.0f;
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

  /**
   * An AI club negotiating for one of the managed club's players. Real
   * deals are structured: add-ons are typically 10-30% of the headline
   * fee, sell-on clauses 10-20%, and big fees are spread over 2-4 years.
   * The buyer reads a structure by its present cost: deferred money is
   * cheaper for it than cash (it discounts faster than a seller, which is
   * why instalments help both sides), add-ons cost their odds of being
   * paid and a sell-on costs the expected share of a future sale. [P]
   */
  struct Buyer final
  {
    /** Buyer's discount of deferred money per year of delay. */
    static constexpr float DEFERRED_DISCOUNT_PER_YEAR = 0.09f;
    /** Odds of paying an add-on: appearance targets are likelier than
     * goal targets; both fall with the target. */
    static constexpr float APPEARANCE_ODDS_BASE = 0.90f;
    static constexpr float APPEARANCE_ODDS_PER_MATCH = 0.012f;
    static constexpr float GOAL_ODDS_BASE = 0.70f;
    static constexpr float GOAL_ODDS_PER_GOAL = 0.025f;
    static constexpr float MIN_ADD_ON_ODDS = 0.15f;
    static constexpr float MAX_ADD_ON_ODDS = 0.85f;
    /** Expected share of the fee a sell-on clause costs per percent point
     * (resale odds times resale value), by age. */
    static constexpr int SELL_ON_YOUNG_AGE = 21;
    static constexpr int SELL_ON_PRIME_AGE = 24;
    static constexpr int SELL_ON_SETTLED_AGE = 27;
    static constexpr float SELL_ON_COST_YOUNG = 0.45f;
    static constexpr float SELL_ON_COST_PRIME = 0.35f;
    static constexpr float SELL_ON_COST_SETTLED = 0.20f;
    static constexpr float SELL_ON_COST_VETERAN = 0.08f;
    /** Opening bid as a share of the ceiling (more with rivals). */
    static constexpr float OPENING_SHARE_MIN = 0.75f;
    static constexpr float OPENING_SHARE_MAX = 0.92f;
    static constexpr float OPENING_RIVAL_BONUS = 0.05f;
    /** Share of opening bids that carry an appearance add-on. */
    static constexpr float OPENING_ADD_ON_CHANCE = 0.35f;
    /** Add-ons offered instead of cash, as a share of the fee. */
    static constexpr float ADD_ON_SHARE = 0.15f;
    static constexpr float MAX_ADD_ON_SHARE = 0.30f;
    static constexpr std::uint16_t ADD_ON_APPEARANCES = 30;
    static constexpr std::uint16_t ADD_ON_GOALS = 10;
    /** Counters the buyer answers before it stops (drawn per offer). */
    static constexpr std::uint8_t MIN_PATIENCE = 2;
    static constexpr std::uint8_t MAX_PATIENCE = 4;
    /** Patience of offers made before talks were drawn (older saves). */
    static constexpr std::uint8_t DEFAULT_PATIENCE = 3;
    /** Share of the gap to the seller's ask (capped by the ceiling) the
     * buyer closes on each answer; the last answer is its final offer. */
    static constexpr float FIRST_CONCESSION = 0.45f;
    static constexpr float CONCESSION_STEP = 0.15f;
    /** A counter worth more than this multiple of the ceiling insults the
     * buyer; a second insult, or one beyond WALK_OUT_MULTIPLE, ends the
     * talks. */
    static constexpr float INSULT_MULTIPLE = 1.6f;
    static constexpr float WALK_OUT_MULTIPLE = 2.5f;
    static constexpr std::uint8_t MAX_INSULTS = 2;
    /** Auction effect: each rival bidder lifts the ceiling and speeds up
     * concessions, capped. */
    static constexpr float RIVAL_CEILING_BONUS = 0.07f;
    static constexpr float MAX_RIVAL_CEILING_BONUS = 0.20f;
    static constexpr float RIVAL_CONCESSION_BONUS = 0.10f;
    /** Deadline pressure: in the last days of a window the buyer stretches
     * its ceiling, concedes faster and answers the same day. */
    static constexpr int DEADLINE_DAYS = 2;
    static constexpr float DEADLINE_CEILING_BONUS = 0.06f;
    static constexpr float DEADLINE_CONCESSION_BONUS = 0.25f;
    /** Days the buyer takes to answer a counter (outside the deadline),
     * and days it leaves the club to answer its own proposal. */
    static constexpr int MIN_REPLY_DAYS = 1;
    static constexpr int MAX_REPLY_DAYS = 2;
    static constexpr int ANSWER_DAYS = 4;
    /** A club whose talks ended (turned down, walked away, ignored) does
     * not bid for the player again for this long, nor in the same window. */
    static constexpr int TALKS_COOLDOWN_DAYS = 30;
    /** A second club bids for a player who already has an offer on this
     * share of its approaches. */
    static constexpr float RIVAL_APPROACH_CHANCE = 0.5f;
    /** Chance the player fails to agree personal terms once the clubs
     * agree, by his stance. */
    static constexpr float TERMS_REFUSAL_OPEN = 0.05f;
    static constexpr float TERMS_REFUSAL_HAPPY = 0.15f;
    static constexpr float TERMS_REFUSAL_RELUCTANT = 0.60f;
    /** Stance: ambitious players want a clearly bigger club. */
    static constexpr std::uint8_t KEEN_AMBITION = 55;
    static constexpr int KEEN_REPUTATION_GAP = 5;
    static constexpr std::uint8_t HAPPY_LOYALTY = 75;
    static constexpr float HAPPY_MORALE = 60.0f;
    /** A bid worth this share of the market value is a big bid: turning
     * it down upsets a player who wants the move. */
    static constexpr float BIG_BID_VALUE_SHARE = 0.9f;
    static constexpr float REJECTED_KEEN_MORALE = 10.0f;
    static constexpr float REJECTED_ASKED_MORALE = 12.0f;
    static constexpr float REJECTED_SMALL_BID_MORALE = 3.0f;
    static constexpr float REJECTED_OPEN_MORALE = 3.0f;
    static constexpr float REJECTED_TRUST = 6.0f;
    /** Ambition from which a keen player asks to leave after a big bid is
     * turned down. */
    static constexpr std::uint8_t REQUEST_AMBITION = 70;
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
    /** The agent opens above what the player will sign for: a base margin
     * plus more for ambitious players and for a step down in stature (per
     * reputation point, capped). The ask falls to the real demand by the
     * last round. [P] */
    static constexpr float AGENT_BASE_MARGIN = 0.06f;
    static constexpr float AGENT_AMBITION_MARGIN = 0.12f;
    static constexpr float AGENT_STATURE_MARGIN_PER_POINT = 0.005f;
    static constexpr float AGENT_STATURE_MARGIN_CAP = 0.10f;
    static constexpr std::uint32_t AGENT_ASK_ROUNDING = 100;
    /** Wage offers this close below the demand get the agent's pushback
     * instead of a flat refusal. [P] */
    static constexpr float AGENT_PUSHBACK_BAND = 0.10f;
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
    /** A parent this many reputation points above the borrower pays most
     * of the wage, so smaller clubs can afford its prospects. [P] */
    static constexpr int SMALL_BORROWER_REPUTATION_GAP = 10;
    static constexpr std::uint8_t SMALL_BORROWER_WAGE_SHARE = 25;
  };

  /** AI market activity. */
  struct Market final
  {
    /** Shares of the world's clubs evaluated per window day and the cap on
     * completed deals per day (scaled by the window's activity weight), so
     * activity per club does not depend on the size of the world. Tuned for
     * ~10-15 moves per club and season with 15-25% of them paying a fee.
     * The deadline-day spike carries ~10-15% of window spend. [S/P] */
    static constexpr float DAILY_EVALUATION_SHARE = 0.24f;
    static constexpr float DAILY_DEAL_SHARE = 0.24f;
    static constexpr float LATE_WINDOW_WEIGHT = 1.8f;
    static constexpr float DEADLINE_DAY_WEIGHT = 6.0f;
    static constexpr int LATE_WINDOW_DAYS = 7;
    /** Share of clubs signing a free agent per day while the window is
     * shut, and pre-contract approaches per club and day from 1 January. */
    static constexpr float CLOSED_WINDOW_SIGNING_SHARE = 0.017f;
    static constexpr float DAILY_PRE_CONTRACT_SHARE = 0.0125f;

    /** Count for @p clubs clubs at @p share (at least one). */
    static constexpr int perDay(std::size_t clubs, float share,
                                float weight = 1.0f)
    {
      const float count = static_cast<float>(clubs) * share * weight;
      return count < 1.0f ? 1 : static_cast<int>(count + 0.5f);
    }
    /** Squad size AI clubs aim for; above it they loan-list prospects,
     * two above it they release veterans, and they stop buying at
     * AI_MAX_SQUAD + 2. [P] */
    static constexpr std::size_t AI_TARGET_SQUAD = 26;
    static constexpr std::size_t AI_MAX_SQUAD = 30;
    static constexpr int MAX_LOAN_LISTED_PER_CLUB = 8;
    /** Clubs sounded out when placing a loan-listed prospect. */
    static constexpr int LOAN_PLACEMENT_TRIES = 8;
    /** AI clubs loan out prospects ranked beyond the matchday squad. */
    static constexpr std::size_t LOAN_OUT_MIN_RANK = 16;
    /** Veterans beyond this rank and age are released by AI clubs. */
    static constexpr std::size_t RELEASE_MIN_RANK = 22;
    static constexpr int RELEASE_MIN_AGE = 30;
    /** Above AI_MAX_SQUAD any surplus player past prospect age may go. */
    static constexpr int SURPLUS_RELEASE_MIN_AGE = 24;
    /** A weakest starter this far below the squad level is an upgrade
     * need; a signing must beat him by UPGRADE_MARGIN. A missing position
     * accepts players down to SHORTAGE_LEVEL_MARGIN below the level. [P] */
    static constexpr float UPGRADE_DEFICIT = 2.0f;
    static constexpr float UPGRADE_MARGIN = 1.0f;
    static constexpr float SHORTAGE_LEVEL_MARGIN = 12.0f;
    /** A club below AI_TARGET_SQUAD with no position need signs cover
     * for its thinnest group down to this far below its level. [P] */
    static constexpr float DEPTH_LEVEL_MARGIN = 6.0f;
    /** No AI signing pays one player more than this share of the club's
     * weekly wage budget: a top earner takes ~10-15% of a wage bill, i.e.
     * ~6-9% of revenue, so wage demands stay at the club's level. [P] */
    static constexpr float MAX_SINGLE_WAGE_SHARE = 0.15f;
    /** Pre-contract targets more than this above the club's level do not
     * drop down to it. [P] */
    static constexpr float PRE_CONTRACT_MAX_ABOVE_LEVEL = 6.0f;
    /** Unsigned free agents lower their wage expectations each month. [P] */
    static constexpr float FREE_AGENT_MONTHLY_WAGE_FACTOR = 0.90f;
    /** Relegation: this share of a relegated club's first-team players have
     * a clause that lets them leave for free, at most this many per club.
     * In the summer after promotion a club measures its starters against
     * its level plus PROMOTED_LEVEL_BOOST. [P] */
    static constexpr float RELEGATION_CLAUSE_SHARE = 0.25f;
    static constexpr int RELEGATION_CLAUSE_MAX_EXITS = 3;
    static constexpr int RELEGATION_CLAUSE_MAX_AGE = 31;
    static constexpr float PROMOTED_LEVEL_BOOST = 3.0f;
    /** Share of visits without a structured move on which a club also
     * works the transfer list (bids for its targets, lists its surplus):
     * most moves are free or loans, few carry a fee. [P] */
    static constexpr double LIST_ACTIVITY_CHANCE = 0.35;
    /** Share of needs a club first tries to fill with a loan, and share
     * of the needs left over that it pays a fee for. [P] */
    static constexpr double LOAN_BEFORE_FEE_CHANCE = 0.9;
    static constexpr double FEE_ROUTE_CHANCE = 0.6;
    /** A club with less cash than this many weeks of payroll only signs
     * players for a missing position. [P] */
    static constexpr std::int64_t UPGRADE_CASH_RESERVE_WEEKS = 8;
    /** A loanee may be this far below the level asked of a signing. [P] */
    static constexpr float LOAN_LEVEL_SLACK = 4.0f;
    /** Fee route: players sampled beyond the list, offers tried, and the
     * most an AI club pays relative to market value. [P] */
    static constexpr std::size_t AI_BUY_SAMPLE = 120;
    static constexpr std::size_t AI_BUY_ATTEMPTS = 6;
    static constexpr double AI_MAX_VALUE_MULTIPLE = 1.6;
    /** Chance per evaluated club that an AI club approaches one of the
     * managed club's players. [P] */
    static constexpr float MANAGED_APPROACH_CHANCE = 0.0035f;
    /** Transfer news kept for the feed, posted every NEWS_DIGEST_DAYS
     * while a window is open (and when it closes). [P] */
    static constexpr std::size_t NEWS_DIGEST_LIMIT = 5;
    static constexpr int NEWS_DIGEST_DAYS = 14;
  };

  /** How well a target fits the managed squad (recruitment screens). [P] */
  struct Fit final
  {
    /** Bonus for a position without enough starters / enough depth. */
    static constexpr float MISSING_STARTER_BONUS = 40.0f;
    static constexpr float MISSING_DEPTH_BONUS = 15.0f;
    /** Score per overall point above the weakest starter, clamped. */
    static constexpr float UPGRADE_WEIGHT = 2.0f;
    static constexpr float MIN_GAIN = -10.0f;
    static constexpr float MAX_GAIN = 15.0f;
    /** Gain needed to call a target an upgrade. */
    static constexpr float UPGRADE_MARGIN = 1.0f;
  };
};
