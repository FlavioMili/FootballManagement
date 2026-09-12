// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <array>
#include <cstdint>

#include "global/languages.h"
#include "global/types.h"

/**
 * @file world_tuning.h
 * @brief Named parameters of the club/world simulation.
 *
 * Values marked [S] are anchored to published data (UEFA ECFIL 2025 revenue
 * mix and wage ratios, UEFA Elite Club Injury Study, Ekstrand 2020 layoffs,
 * Branquinho 2025 physical ageing). Values marked [P] are designer priors
 * chosen to be consistent with nearby sourced facts; tune them by calibration.
 */

/**
 * @enum WorldRegion
 * @brief Football region of a league (pre-season tours stay inside it).
 */
enum class WorldRegion : std::uint8_t
{
  Europe,
  EasternEurope,
  NorthAmerica,
  SouthAmerica
};

/**
 * @struct LeagueProfile
 * @brief Economic and sporting profile of a league.
 */
struct LeagueProfile
{
  LeagueID league_id;
  std::uint8_t reputation;       /*!< 1-100 sporting level. [P] */
  float average_revenue_eur;     /*!< Average club revenue per season. */
  float tv_share;                /*!< Share of revenue from broadcasting. */
  float continental_share;       /*!< Share from continental prize money. */
  float gate_share;              /*!< Share from matchday. */
  float commercial_share;        /*!< Share from sponsorship + other. */
  float wage_ratio;              /*!< Total wages / revenue. */
  float average_attendance;      /*!< Typical crowd for a mid-table club. */
  Language domestic_nationality; /*!< Most common nationality. */
  float domestic_share;          /*!< Share of domestic players. [P] */
  WorldRegion region;            /*!< Where the league is played. */
};

/**
 * League profiles keyed by the ids in assets/user_made_data/leagues. The top
 * five rows use ECFIL 2025 (FY2024) average revenue, revenue mix and wage
 * ratios [S]; the remaining leagues are [P] estimates in the "leagues 6-22"
 * range reported by the same source, with broadcasting as the largest
 * stream as in most leagues. Lower divisions overshoot on wages [S].
 *
 * Every league needs a row: second divisions share their country's
 * nationality and region and sit about 20-30 reputation points below the top
 * division, with no continental income. Their revenue, crowd and wage rows
 * are [P] estimates of each country's second tier.
 */
inline constexpr std::array<LeagueProfile, 22> LEAGUE_PROFILES = {{
    {3, 92, 372e6f, 0.46f, 0.05f, 0.14f, 0.35f, 0.64f, 38000.0f, Language::EN,
     0.70f, WorldRegion::Europe},
    {2, 87, 194e6f, 0.35f, 0.12f, 0.15f, 0.38f, 0.62f, 28000.0f, Language::ES,
     0.75f, WorldRegion::Europe},
    {1, 85, 146e6f, 0.38f, 0.12f, 0.15f, 0.35f, 0.66f, 30000.0f, Language::IT,
     0.75f, WorldRegion::Europe},
    {4, 84, 217e6f, 0.29f, 0.12f, 0.14f, 0.45f, 0.57f, 42000.0f, Language::DE,
     0.72f, WorldRegion::Europe},
    {5, 80, 140e6f, 0.19f, 0.09f, 0.18f, 0.54f, 0.73f, 26000.0f, Language::FR,
     0.75f, WorldRegion::Europe},
    {12, 70, 35e6f, 0.38f, 0.15f, 0.12f, 0.35f, 0.70f, 12000.0f, Language::PT,
     0.72f, WorldRegion::Europe},
    {11, 70, 55e6f, 0.45f, 0.05f, 0.15f, 0.35f, 0.75f, 20000.0f, Language::BR,
     0.85f, WorldRegion::SouthAmerica},
    {10, 64, 20e6f, 0.40f, 0.05f, 0.22f, 0.33f, 0.80f, 22000.0f, Language::ES,
     0.85f, WorldRegion::SouthAmerica},
    {9, 62, 40e6f, 0.45f, 0.03f, 0.18f, 0.34f, 0.70f, 24000.0f, Language::MX,
     0.78f, WorldRegion::NorthAmerica},
    {8, 62, 35e6f, 0.40f, 0.08f, 0.12f, 0.40f, 0.75f, 15000.0f, Language::RU,
     0.78f, WorldRegion::EasternEurope},
    {7, 60, 45e6f, 0.35f, 0.02f, 0.28f, 0.35f, 0.60f, 20000.0f, Language::US,
     0.72f, WorldRegion::NorthAmerica},
    {6, 55, 15e6f, 0.45f, 0.00f, 0.15f, 0.40f, 0.95f, 9000.0f, Language::IT,
     0.85f, WorldRegion::Europe},
    {14, 68, 40e6f, 0.30f, 0.00f, 0.25f, 0.45f, 0.98f, 18000.0f, Language::EN,
     0.75f, WorldRegion::Europe},
    {13, 64, 16e6f, 0.50f, 0.00f, 0.12f, 0.38f, 0.80f, 9000.0f, Language::ES,
     0.85f, WorldRegion::Europe},
    {15, 62, 30e6f, 0.30f, 0.00f, 0.25f, 0.45f, 0.60f, 22000.0f, Language::DE,
     0.80f, WorldRegion::Europe},
    {16, 58, 14e6f, 0.35f, 0.00f, 0.12f, 0.53f, 0.85f, 7000.0f, Language::FR,
     0.85f, WorldRegion::Europe},
    {22, 48, 3.5e6f, 0.40f, 0.00f, 0.15f, 0.45f, 0.90f, 2500.0f, Language::PT,
     0.80f, WorldRegion::Europe},
    {21, 50, 12e6f, 0.45f, 0.00f, 0.20f, 0.35f, 0.85f, 9000.0f, Language::BR,
     0.85f, WorldRegion::SouthAmerica},
    {20, 44, 3e6f, 0.35f, 0.00f, 0.30f, 0.35f, 0.85f, 8000.0f, Language::ES,
     0.85f, WorldRegion::SouthAmerica},
    {19, 42, 5e6f, 0.30f, 0.00f, 0.25f, 0.45f, 0.80f, 6000.0f, Language::MX,
     0.85f, WorldRegion::NorthAmerica},
    {18, 42, 6e6f, 0.15f, 0.00f, 0.10f, 0.75f, 0.85f, 4000.0f, Language::RU,
     0.85f, WorldRegion::EasternEurope},
    {17, 40, 8e6f, 0.10f, 0.00f, 0.40f, 0.50f, 0.60f, 5500.0f, Language::US,
     0.80f, WorldRegion::NorthAmerica},
}};

/** Profile used for leagues that are not listed above. [P] */
inline constexpr LeagueProfile DEFAULT_LEAGUE_PROFILE = {
    0,     50,    10e6f,   0.25f,        0.00f, 0.20f,
    0.55f, 0.80f, 8000.0f, Language::EN, 0.80f, WorldRegion::Europe};

/** Returns the profile of @p league_id or the default profile. */
constexpr const LeagueProfile& leagueProfile(LeagueID league_id)
{
  for (const LeagueProfile& profile : LEAGUE_PROFILES)
  {
    if (profile.league_id == league_id) return profile;
  }
  return DEFAULT_LEAGUE_PROFILE;
}

/** Named world simulation parameters. */
struct WorldTuning final
{
  struct Generation final
  {
    /** Reputation offsets by league rank bucket (giants .. strugglers). [P] */
    static constexpr std::array<float, 5> TIER_REPUTATION_OFFSET = {
        15.0f, 8.0f, 2.0f, -5.0f, -10.0f};
    /** Share of a league's clubs in each bucket (sums to 1). [P] */
    static constexpr std::array<float, 5> TIER_SHARE = {0.10f, 0.15f, 0.25f,
                                                        0.25f, 0.25f};
    /** First-team level = base + slope * reputation. [P] */
    static constexpr float TEAM_LEVEL_BASE = 38.0f;
    static constexpr float TEAM_LEVEL_SLOPE = 0.42f;
    /** From this age potential is at most the current ability plus a
     * small headroom: growth ends in the mid-20s. [S: peaks 25-27] */
    static constexpr int VETERAN_AGE = 29;
    static constexpr float VETERAN_HEADROOM = 2.0f;
    /** Name draws before a double surname is used. [P] */
    static constexpr int NAME_ATTEMPTS = 48;
    /** Wage elasticity to ability: W ~ overall^gamma. [P] */
    static constexpr float WAGE_ABILITY_EXPONENT = 7.0f;
    /** Opening cash as a share of expected income (uniform range). [P] */
    static constexpr float OPENING_BALANCE_MIN = 0.15f;
    static constexpr float OPENING_BALANCE_MAX = 0.45f;
  };

  struct Finance final
  {
    /** Revenue index slope on reputation relative to league mean. [P] */
    static constexpr float REVENUE_REPUTATION_SLOPE = 0.075f;
    /** Share of TV money paid as an equal monthly share (rest is merit). [S]
     * Premier League equal share ~75% of central payments. */
    static constexpr float TV_EQUAL_SHARE = 0.75f;
    /** Continental prize money weights for the top five places. [P] */
    static constexpr std::array<float, 5> CONTINENTAL_WEIGHTS = {
        0.35f, 0.25f, 0.18f, 0.12f, 0.10f};
    /** Share of the continental money of its expected place a board
     * budgets on; the rest is upside. [P] */
    static constexpr float BUDGETED_CONTINENTAL_SHARE = 0.5f;
    /** Player wages / expected income = base + slope * (league wage ratio
     * - 0.65), within [min, max]. Cups and continental money lift the
     * realised revenue about 10% above the budgeted income, so wages end
     * near 57-65% of revenue in top divisions. [S: ECFIL wages 57-73% of
     * revenue by league; lower tiers overshoot] */
    static constexpr float PLAYER_WAGE_SHARE_BASE = 0.63f;
    static constexpr float PLAYER_WAGE_SHARE_SLOPE = 0.60f;
    static constexpr float PLAYER_WAGE_SHARE_MIN = 0.55f;
    static constexpr float PLAYER_WAGE_SHARE_MAX = 0.70f;
    /** Non-player wages / expected income. [S: ~18% of revenue in ECFIL,
     * including admin staff paid out of other income] */
    static constexpr float STAFF_SHARE = 0.12f;
    /** Floor of other operating costs (stadium, travel, admin). [P] */
    static constexpr float MIN_OPERATING_SHARE = 0.15f;
    /** Clubs budget this total cost / expected income ratio, so a club that
     * finishes where expected makes a small operating profit. [P: about
     * half of top-division clubs are profitable, ECFIL] */
    static constexpr float TARGET_COST_RATIO = 0.95f;
    /** Home league matches per season with 20 clubs. */
    static constexpr float HOME_MATCHES_PER_SEASON = 19.0f;
    /** Price elasticity of attendance (inelastic demand). [S: 0.3-0.7] */
    static constexpr float TICKET_PRICE_ELASTICITY = 0.5f;
    /** Attendance sensitivity to sporting success. [P] */
    static constexpr float ATTENDANCE_SUCCESS_WEIGHT = 0.25f;
    /** Share of transfer income returned to the transfer budget. [P] */
    static constexpr float TRANSFER_INCOME_REINVESTMENT = 0.5f;
    /** Season wage budget = expected player wage bill times this margin. [P] */
    static constexpr double WAGE_BUDGET_MARGIN = 1.05;
    /** Weeks of payroll kept in the bank before any transfer spending. [P] */
    static constexpr std::int64_t CASH_RESERVE_WEEKS = 12;
    /** Transfer budget = share of cash + share of revenue each season. [P] */
    static constexpr float TRANSFER_BUDGET_CASH_SHARE = 0.35f;
    static constexpr float TRANSFER_BUDGET_REVENUE_SHARE = 0.08f;
  };

  struct Fitness final
  {
    /** Residual fatigue decays with a 48 h time constant. [S/P] */
    static constexpr float FATIGUE_TIME_CONSTANT_HOURS = 48.0f;
    /** Condition lost over 90 minutes = base - slope * stamina. [P] */
    static constexpr float MATCH_DRAIN_BASE = 38.0f;
    static constexpr float MATCH_DRAIN_STAMINA_SLOPE = 0.18f;
    /** Sharpness gain per 90 minutes and daily decay without play. [P] */
    static constexpr float SHARPNESS_GAIN_PER_90 = 12.0f;
    static constexpr float SHARPNESS_DAILY_DECAY = 0.5f;
    static constexpr float SHARPNESS_FLOOR = 30.0f;
    /** Injury hazards per hour of exposure. [S: ECIS 21-27.5 / 3.5-4.1] */
    static constexpr double MATCH_INJURY_RATE_PER_HOUR = 24.0 / 1000.0;
    static constexpr double TRAINING_INJURY_RATE_PER_HOUR = 3.8 / 1000.0;
    /** Average training hours per training day. [P] */
    static constexpr double TRAINING_HOURS_PER_DAY = 1.2;
    /** Muscle injury multiplier with <= 4 days of recovery. [S: RR 1.32] */
    static constexpr double CONGESTION_MUSCLE_MULTIPLIER = 1.32;
    static constexpr double CONGESTION_OTHER_MULTIPLIER = 1.09;
    /** Away match injury odds. [S: OR 0.89] */
    static constexpr double AWAY_MULTIPLIER = 0.89;
    /** Recent same-site injury (within ~2 months). [S: RR 4.8 / 2.7] */
    static constexpr double RECENT_REINJURY_MULTIPLIER = 4.8;
    static constexpr double PRIOR_REINJURY_MULTIPLIER = 2.7;
    /** Re-injury layoffs are longer. [S: x1.33] */
    static constexpr float REINJURY_DURATION_MULTIPLIER = 1.33f;
  };

  struct Development final
  {
    /** Share of the potential gap closed per season by age. [P] */
    static constexpr float GROWTH_RATE_TEEN = 0.30f;
    static constexpr float GROWTH_RATE_20_21 = 0.25f;
    static constexpr float GROWTH_RATE_22_23 = 0.18f;
    static constexpr float GROWTH_RATE_24_25 = 0.12f;
    static constexpr float GROWTH_RATE_26_28 = 0.06f;
    /** Minutes factor M = 0.6 + 0.4 * min(1, minutes / 1800). [P] */
    static constexpr float MINUTES_FOR_FULL_GROWTH = 1800.0f;
    /** Yearly decline fractions of attribute value. [S: physical -1%/yr at
     * 30-32 and -3%/yr after 32, endurance at half rate] */
    static constexpr float PHYSICAL_DECLINE_30_32 = 0.01f;
    static constexpr float PHYSICAL_DECLINE_AFTER_32 = 0.03f;
    /** Technical and mental decline. [P] */
    static constexpr float TECHNICAL_DECLINE_32_33 = 0.01f;
    static constexpr float TECHNICAL_DECLINE_AFTER_33 = 0.02f;
    static constexpr float MENTAL_DECLINE_AFTER_34 = 0.01f;
  };

  struct Morale final
  {
    /** Weekly update M = 0.8 M + 0.2 target. [P, world-realism 9.6] */
    static constexpr float PERSISTENCE = 0.8f;
    static constexpr float NEUTRAL = 55.0f;
    /** Morale change per point above/below expectation in a match. [P] */
    static constexpr float RESULT_WEIGHT = 2.0f;
  };

  struct Youth final
  {
    /** Potential mean relative to first-team level. [S: ~4-5% of scholars
     * reach the top tier; sd 9 puts 1.67 sd above the mean at team level] */
    static constexpr float POTENTIAL_OFFSET = -15.0f;
    static constexpr float POTENTIAL_STDDEV = 9.0f;
    static constexpr float FACILITY_POTENTIAL_BONUS = 5.0f;
    /** AI clubs trim their first-team squad (academy players excluded) to
     * this size at the season end, releasing the weakest players outside the
     * matchday squad. [P: real senior squads hold 25-30 players] */
    static constexpr std::size_t AI_SQUAD_TARGET = 28;
  };

  struct Board final
  {
    static constexpr float INITIAL_CONFIDENCE = 60.0f;
    static constexpr float WARNING_THRESHOLD = 30.0f;
    static constexpr float DISMISSAL_THRESHOLD = 12.0f;
    /** Consecutive monthly reviews below the dismissal threshold. [P] */
    static constexpr int DISMISSAL_REVIEWS = 3;
    /** Minimum league matches before a dismissal is possible. [P] */
    static constexpr int MIN_MATCHES_FOR_DISMISSAL = 12;
  };
};
