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
 * @struct LeagueShape
 * @brief How unequal the clubs of a league are in first-team quality.
 *
 * Top divisions are far more stratified than second tiers: points per game
 * spread 0.43-0.54 (SD over the table) against 0.28-0.36, a single hegemon in
 * Germany and France, three giants in Spain and Portugal, a big six in
 * England. [S: realism-research-2 2.1-2.2, FD-computed 2015-26]
 */
struct LeagueShape
{
  float level_sd;           /*!< SD of first-team level between clubs. */
  std::uint8_t elite_clubs; /*!< Dominant clubs at the top of the league. */
  float elite_gap;          /*!< Their extra edge in standard deviations. */
  /** Shift of the mean first-team level, which lines a second tier up with
   * its top division so that promoted clubs finish around 16th. */
  float level_offset;
};

/**
 * @struct LeagueMatchStyle
 * @brief Real match averages of a league (the engine is steered towards
 * them through the per-match context).
 */
struct LeagueMatchStyle
{
  float goals;          /*!< Goals per match. */
  float yellow_cards;   /*!< Yellow cards per match. */
  float referee_spread; /*!< SD of the referees' mean yellows per match. */
  float home_edge;      /*!< Home win % minus away win %. */
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
  float domestic_share;          /*!< Share of domestic players. */
  WorldRegion region;            /*!< Where the league is played. */
  float stadium_fill;            /*!< Capacity a mid-table club fills. */
  LeagueShape shape;             /*!< Quality hierarchy of the clubs. */
  float coach_changes;           /*!< Manager changes per club-season. */
  LeagueMatchStyle match_style;  /*!< Real match averages. */
};

/**
 * League profiles keyed by the ids in assets/user_made_data/leagues.
 *
 * Revenue, mix and wage ratios of the top five use ECFIL 2025 (FY2024) [S]
 * with France after its 2024-25 TV collapse (wages ~80%); Brazil, Russia and
 * the USA use national reports (R$545m, EUR 61m, ~US$65-80m per club) [S/P].
 * Second tiers: Championship ~EUR 46m with wages above 90% of revenue, a
 * healthy 2. Bundesliga (~EUR 50m, wages ~40%), Serie B EUR 24m at 82% [S];
 * the rest are [P] estimates. Crowds and stadium fill follow the 2024-25
 * league averages (England and Germany sell out, Russia fills 34%, Liga de
 * Expansion under 20%) [S]. Domestic shares follow CIES expatriate shares
 * (England, Italy and MLS ~40% domestic, Brazil and Argentina ~90%); second
 * tiers are assumed to have about half the expatriates [S/P].
 *
 * Shapes are fitted to the 20-slot points curves; coach changes per
 * club-season follow LMA/CIES counts (MLS 0.35 ... Serie B and Brazil 0.9-1.0)
 * [S/P]; match styles are football-data 2022-26 averages (Brazil, Argentina,
 * Mexico, the USA and Russia partly [P]). [S: realism-research-2 1, 3.4, 5, 7]
 *
 * Every league needs a row: second divisions share their country's
 * nationality and region and sit about 20-30 reputation points below the top
 * division, with no continental income.
 */
inline constexpr std::array<LeagueProfile, 22> LEAGUE_PROFILES = {{
    {3, 92, 372e6f, 0.46f, 0.05f, 0.14f, 0.35f, 0.65f, 38000.0f, Language::EN,
     0.40f, WorldRegion::Europe, 0.96f, {9.0f, 6, 0.4f, 0.0f}, 0.45f,
     {2.95f, 3.89f, 0.25f, 13.1f}},
    {2, 87, 194e6f, 0.35f, 0.12f, 0.15f, 0.38f, 0.60f, 27000.0f, Language::ES,
     0.62f, WorldRegion::Europe, 0.83f, {8.5f, 3, 1.0f, 0.0f}, 0.40f,
     {2.62f, 4.67f, 0.30f, 18.0f}},
    {1, 85, 146e6f, 0.38f, 0.12f, 0.15f, 0.35f, 0.66f, 28000.0f, Language::IT,
     0.40f, WorldRegion::Europe, 0.81f, {9.0f, 4, 0.6f, 0.0f}, 0.55f,
     {2.54f, 4.09f, 0.30f, 9.0f}},
    {4, 84, 230e6f, 0.29f, 0.12f, 0.14f, 0.45f, 0.54f, 40000.0f, Language::DE,
     0.52f, WorldRegion::Europe, 0.97f, {7.5f, 1, 1.5f, 0.0f}, 0.40f,
     {3.19f, 3.99f, 0.30f, 11.9f}},
    {5, 80, 120e6f, 0.19f, 0.09f, 0.18f, 0.54f, 0.80f, 26000.0f, Language::FR,
     0.56f, WorldRegion::Europe, 0.78f, {8.5f, 1, 1.5f, 0.0f}, 0.50f,
     {2.83f, 3.67f, 0.30f, 11.3f}},
    {12, 70, 35e6f, 0.38f, 0.15f, 0.12f, 0.35f, 0.55f, 8000.0f, Language::PT,
     0.47f, WorldRegion::Europe, 0.50f, {10.0f, 3, 1.2f, -3.0f}, 0.50f,
     {2.65f, 5.13f, 0.35f, 12.3f}},
    {11, 70, 90e6f, 0.45f, 0.05f, 0.15f, 0.35f, 0.80f, 22000.0f, Language::BR,
     0.90f, WorldRegion::SouthAmerica, 0.55f, {5.5f, 0, 0.0f, 0.0f}, 0.95f,
     {2.46f, 5.50f, 0.40f, 21.1f}},
    {10, 64, 20e6f, 0.40f, 0.05f, 0.22f, 0.33f, 0.80f, 20000.0f, Language::ES,
     0.90f, WorldRegion::SouthAmerica, 0.60f, {3.5f, 2, 0.8f, 0.0f}, 0.80f,
     {2.05f, 4.90f, 0.40f, 18.4f}},
    {9, 62, 50e6f, 0.45f, 0.03f, 0.18f, 0.34f, 0.70f, 20000.0f, Language::MX,
     0.52f, WorldRegion::NorthAmerica, 0.55f, {5.5f, 4, 0.5f, 0.0f}, 0.70f,
     {2.82f, 4.50f, 0.35f, 17.8f}},
    {8, 62, 60e6f, 0.12f, 0.05f, 0.05f, 0.78f, 0.70f, 11000.0f, Language::RU,
     0.62f, WorldRegion::EasternEurope, 0.34f, {8.0f, 2, 0.8f, 0.0f}, 0.40f,
     {2.73f, 4.50f, 0.35f, 16.9f}},
    {7, 60, 65e6f, 0.35f, 0.02f, 0.28f, 0.35f, 0.55f, 21000.0f, Language::US,
     0.40f, WorldRegion::NorthAmerica, 0.85f, {3.5f, 0, 0.0f, 0.0f}, 0.35f,
     {2.96f, 4.30f, 0.30f, 19.1f}},
    {6, 55, 24e6f, 0.45f, 0.00f, 0.15f, 0.40f, 0.82f, 8500.0f, Language::IT,
     0.70f, WorldRegion::Europe, 0.55f, {4.0f, 3, 0.5f, -2.2f}, 0.90f,
     {2.47f, 4.92f, 0.50f, 13.5f}},
    {14, 68, 46e6f, 0.30f, 0.00f, 0.25f, 0.45f, 0.93f, 20000.0f, Language::EN,
     0.65f, WorldRegion::Europe, 0.75f, {4.0f, 3, 0.5f, -6.7f}, 0.55f,
     {2.54f, 3.79f, 0.50f, 12.9f}},
    {13, 64, 12e6f, 0.50f, 0.00f, 0.12f, 0.38f, 0.70f, 11000.0f, Language::ES,
     0.80f, WorldRegion::Europe, 0.68f, {3.2f, 0, 0.0f, -1.4f}, 0.55f,
     {2.36f, 4.89f, 0.50f, 20.1f}},
    {15, 62, 50e6f, 0.30f, 0.00f, 0.25f, 0.45f, 0.40f, 28000.0f, Language::DE,
     0.70f, WorldRegion::Europe, 0.80f, {4.2f, 0, 0.0f, -3.3f}, 0.50f,
     {3.00f, 4.51f, 0.50f, 14.2f}},
    {16, 58, 11e6f, 0.35f, 0.00f, 0.12f, 0.53f, 0.92f, 7000.0f, Language::FR,
     0.75f, WorldRegion::Europe, 0.60f, {3.9f, 0, 0.0f, -3.3f}, 0.45f,
     {2.49f, 3.71f, 0.50f, 12.7f}},
    {22, 48, 3.5e6f, 0.40f, 0.00f, 0.15f, 0.45f, 0.90f, 1800.0f, Language::PT,
     0.65f, WorldRegion::Europe, 0.30f, {4.0f, 0, 0.0f, -4.4f}, 0.60f,
     {2.50f, 5.70f, 0.60f, 7.2f}},
    {21, 50, 10e6f, 0.45f, 0.00f, 0.20f, 0.35f, 0.90f, 6000.0f, Language::BR,
     0.95f, WorldRegion::SouthAmerica, 0.35f, {3.2f, 0, 0.0f, -0.6f}, 1.00f,
     {2.17f, 5.50f, 0.60f, 24.5f}},
    {20, 44, 3e6f, 0.35f, 0.00f, 0.30f, 0.35f, 0.90f, 5000.0f, Language::ES,
     0.95f, WorldRegion::SouthAmerica, 0.45f, {3.0f, 0, 0.0f, 0.8f}, 0.90f,
     {1.91f, 5.00f, 0.60f, 23.7f}},
    {19, 42, 2.5e6f, 0.30f, 0.00f, 0.25f, 0.45f, 0.50f, 3400.0f, Language::MX,
     0.85f, WorldRegion::NorthAmerica, 0.20f, {4.0f, 0, 0.0f, -1.9f}, 0.80f,
     {2.85f, 4.50f, 0.50f, 25.9f}},
    {18, 42, 8e6f, 0.15f, 0.00f, 0.10f, 0.75f, 0.90f, 3500.0f, Language::RU,
     0.90f, WorldRegion::EasternEurope, 0.30f, {3.6f, 0, 0.0f, -3.1f}, 0.50f,
     {2.24f, 4.70f, 0.50f, 14.5f}},
    {17, 40, 7e6f, 0.10f, 0.00f, 0.40f, 0.50f, 0.80f, 5000.0f, Language::US,
     0.60f, WorldRegion::NorthAmerica, 0.60f, {3.2f, 0, 0.0f, 1.1f}, 0.40f,
     {2.70f, 4.30f, 0.50f, 12.3f}},
}};

/** Profile used for leagues that are not listed above. [P] */
inline constexpr LeagueProfile DEFAULT_LEAGUE_PROFILE = {
    0,     50,    10e6f,   0.25f,        0.00f, 0.20f,
    0.55f, 0.80f, 8000.0f, Language::EN, 0.80f, WorldRegion::Europe,
    0.80f, {4.0f, 0, 0.0f, 0.0f},        0.50f, {2.80f, 4.30f, 0.30f, 14.0f}};

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
    /** Club reputation points per point of first-team level inside a
     * league (the LeagueShape spread); revenue follows reputation, so this
     * sets how much richer the elite are. [P] */
    static constexpr float REPUTATION_PER_LEVEL = 0.8f;
    static constexpr float CLUB_REPUTATION_NOISE = 0.5f;
    /** Reputation headroom kept below the cap of 99 for the elite. [P] */
    static constexpr float REPUTATION_CAP_MARGIN = 2.0f;
    /** Highest mean first-team level of a generated club. [P] */
    static constexpr float MAX_CLUB_LEVEL = 90.0f;
    /** League first-team level = base + slope * league reputation. [P] */
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
    static constexpr float PLAYER_WAGE_SHARE_MIN = 0.42f;
    static constexpr float PLAYER_WAGE_SHARE_MAX = 0.74f;
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
    /** AI owners rescue a club whose cash falls below this many weeks of
     * payroll in the red, restoring a cushion of CUSHION weeks. The monthly
     * chance depends on the owner: benefactors always pay, member-owned
     * clubs cannot. [S: ~20% of top-division clubs receive owner equity a
     * year (UEFA ECFIL); P thresholds] */
    static constexpr std::int64_t OWNER_RESCUE_TRIGGER_WEEKS = 4;
    static constexpr std::int64_t OWNER_RESCUE_CUSHION_WEEKS = 8;
    static constexpr float BENEFACTOR_RESCUE_CHANCE = 1.0f;
    static constexpr float AMBITIOUS_RESCUE_CHANCE = 0.5f;
    static constexpr float PATIENT_RESCUE_CHANCE = 0.25f;
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
    /** Share of the potential gap closed per season by age. [P, fitted to
     * +4 / +3 / +1.5 overall per season at 16-18 / 19-21 / 22-24] */
    static constexpr float GROWTH_RATE_TEEN = 0.30f;
    static constexpr float GROWTH_RATE_20_21 = 0.42f;
    static constexpr float GROWTH_RATE_22_23 = 0.38f;
    static constexpr float GROWTH_RATE_24_25 = 0.28f;
    static constexpr float GROWTH_RATE_26_28 = 0.08f;
    /** Minutes factor M = 0.6 + 0.4 * min(1, minutes / 1800). [P] */
    static constexpr float MINUTES_FOR_FULL_GROWTH = 1800.0f;

    /** Yearly decline of pace and strength: from about 28, steeper after
     * 30 and 32. [S: high-intensity efforts -1.8%/yr, speed peaks at 25.7
     * with a clear drop after 32 (Branquinho 2025, LaLiga 2012-20)] */
    static constexpr float physicalDecline(int age)
    {
      if (age >= 34) return 0.065f;
      if (age >= 32) return 0.050f;
      if (age >= 30) return 0.032f;
      if (age >= 28) return 0.012f;
      return 0.0f;
    }
    /** Endurance fades at about half that rate. [S: total distance
     * -0.56%/yr] */
    static constexpr float ENDURANCE_FACTOR = 0.5f;
    /** Technical skills hold until about 31. [P] */
    static constexpr float technicalDecline(int age)
    {
      if (age >= 35) return 0.040f;
      if (age >= 33) return 0.025f;
      if (age >= 31) return 0.015f;
      return 0.0f;
    }
    /** Passing and vision keep improving into the early 30s and fade late
     * (negative = decline). [S: pass accuracy +0.25%/yr of age] */
    static constexpr float craftChange(int age)
    {
      if (age >= 35) return -0.025f;
      if (age >= 33) return -0.010f;
      if (age >= 27) return 0.003f;
      return 0.0f;
    }
    /** Goalkeeping peaks later and holds to the mid-30s. [S: GK peak ~31,
     * elite keepers retire at 38-40] */
    static constexpr float goalkeepingDecline(int age)
    {
      if (age >= 36) return 0.035f;
      if (age >= 34) return 0.015f;
      return 0.0f;
    }
    /** Goalkeepers and centre-backs age at this share of the physical and
     * technical rates until 33. [S: GK and CB hold their peak to ~31] */
    static constexpr float LATE_PEAK_ROLE_FACTOR = 0.6f;
    static constexpr int LATE_PEAK_ROLE_UNTIL = 33;
  };

  struct MatchContext final
  {
    /** Goals and yellow cards per match the engine produces at a neutral
     * context inside the simulated world, where clubs of very different
     * strength meet. The engine already scores less in second divisions
     * (their lower finishing), so those have their own goal reference.
     * [Fitted to fm_lab season: goals per league against the scale, 22
     * leagues] */
    static constexpr float REFERENCE_GOALS_TOP = 2.85f;
    static constexpr float REFERENCE_GOALS_SECOND = 2.42f;
    /** Goals grow about as goalRateScale^1.3 in the world (the scale acts on
     * shot precision; the fit gives 1.35 in top and 1.24 in second
     * divisions, 1.06 between two 65-rated lab sides), so the scale is the
     * goal ratio to the power 1/1.3. */
    static constexpr float GOAL_RESPONSE_EXPONENT = 1.3f;
    static constexpr float REFERENCE_YELLOWS = 4.2f;
    static constexpr float REFERENCE_HOME_EDGE = 14.0f;
    /** SD of referees' mean yellows the engine's per-match spread stands
     * for; top divisions sit below it, second tiers above. [S: England
     * P10-P90 3.67-4.27 top flight, 3.07-4.42 Championship] */
    static constexpr float REFERENCE_REFEREE_SPREAD = 0.4f;
    /** The home advantage scale is the square root of the league's home
     * edge over the reference (damped: the engine's home effect already
     * grows at lower levels), within these bounds. [P] */
    static constexpr float MIN_HOME_SCALE = 0.6f;
    static constexpr float MAX_HOME_SCALE = 1.4f;
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
