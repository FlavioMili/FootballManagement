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
}  // namespace InjuryModel
