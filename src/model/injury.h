// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>

class WorldRng;

/**
 * @enum InjuryType
 * @brief Diagnoses used by the injury model (values are persisted).
 */
enum class InjuryType : std::uint8_t
{
  None = 0,
  HamstringStrain,
  HamstringTightness,
  GroinStrain,
  AnkleSprain,
  QuadricepsStrain,
  CalfStrain,
  KneeMcl,
  ThighContusion,
  AchillesTendinopathy,
  Concussion,
  KneeCartilage,
  KneeMeniscus,
  KneeAcl,
  BackSpasm,
  FootContusion,
  BrokenMetatarsal,
  ShoulderDislocation,
  COUNT
};

/** @brief Severity bands used by UEFA injury studies. */
enum class InjurySeverity : std::uint8_t
{
  Minor,    /*!< Up to 7 days. */
  Moderate, /*!< 8 to 28 days. */
  Major     /*!< More than 28 days. */
};

/** @brief Where an injury happened; changes the diagnosis mix. */
enum class InjuryContext : std::uint8_t
{
  Match,
  Training
};

/** @brief A drawn injury. */
struct Injury
{
  InjuryType type = InjuryType::None;
  std::uint16_t days = 0;
};

namespace InjuryModel
{
/** Language key naming @p type (e.g. "INJURY_HAMSTRING_STRAIN"). */
const char* nameKey(InjuryType type);

/** True for muscle strains (sensitive to congestion and age). */
bool isMuscle(InjuryType type);

/** Severity band of a layoff. */
InjurySeverity severity(std::uint16_t days);

/**
 * Draws a diagnosis and layoff. Diagnosis frequencies follow Ekstrand et al.
 * (2020) with hamstrings up-weighted to ~24% (Ekstrand 2023); layoffs are
 * lognormal with the published median and P10-P90 range. ACL injuries are
 * twenty times rarer in training. A recent injury of the same type is more
 * likely to recur and lasts 33% longer.
 */
Injury draw(WorldRng& rng, InjuryContext context, InjuryType previous,
            bool recent_previous);

/**
 * How much more likely a player who plays carrying an injury is to break
 * down than a fit player: x2 with a minor knock, x3 with a moderate injury
 * and x4 with a major one (tissue that has not healed fails under match
 * load).
 */
float aggravationMultiplier(InjurySeverity severity);

/**
 * Chance that playing @p minutes with @p days_left of an unhealed injury
 * aggravates it: the match injury rate (ECIS) times
 * aggravationMultiplier(). Zero for a fit player.
 */
double aggravationChance(std::uint16_t days_left, int minutes);

/**
 * The layoff after an aggravation: the same diagnosis is drawn again as a
 * recurrence (33% longer than a first injury), and never ends before the
 * injury that was carried would have healed.
 */
Injury aggravate(WorldRng& rng, InjuryType type, std::uint16_t days_left);
}  // namespace InjuryModel
