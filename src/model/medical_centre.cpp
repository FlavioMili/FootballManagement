// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "model/medical_centre.h"

#include <algorithm>
#include <cmath>

#include "model/world_tuning.h"

namespace
{
/** StaffEffects::layoff_multiplier of an untrained and of an elite staff. */
constexpr float WORST_LAYOFF = 1.12f;
constexpr float BEST_LAYOFF = 0.85f;
constexpr float WIDEST_MARGIN = 0.30f;
constexpr float NARROWEST_MARGIN = 0.10f;
/** Multipliers of the injury model (world_simulation.cpp, training.cpp). */
constexpr float AGE_SLOPE = 0.03f;
constexpr int AGE_FROM = 25;
constexpr float RECENT_INJURY = 1.3f;
constexpr float FATIGUE = 1.2f;
constexpr float FATIGUE_CONDITION = 60.0f;
constexpr int CONGESTION_DAYS = 4;
constexpr float WORKLOAD_THRESHOLD = 1.3f;
constexpr float WORKLOAD_SLOPE = 0.5f;
constexpr float WORKLOAD_CAP = 1.5f;
/** Band limits on the combined multiplier. */
constexpr float MODERATE_FROM = 1.25f;
constexpr float HIGH_FROM = 1.6f;
/** Age from which injuries recur more often. */
constexpr int VETERAN_AGE = 30;
}  // namespace

float MedicalCentre::staffQuality(const StaffEffects& effects)
{
  return std::clamp((WORST_LAYOFF - effects.layoff_multiplier) /
                        (WORST_LAYOFF - BEST_LAYOFF),
                    0.0f, 1.0f);
}

ReturnWindow MedicalCentre::returnWindow(std::uint16_t days_left,
                                         float staff_quality)
{
  if (days_left == 0) return {};
  const float margin =
      WIDEST_MARGIN -
      (WIDEST_MARGIN - NARROWEST_MARGIN) * std::clamp(staff_quality, 0.0f, 1.0f);
  const float days = static_cast<float>(days_left);
  // Setbacks are more common than early returns: twice the margin later.
  const int early = static_cast<int>(std::lround(days * margin * 0.5f));
  const int late = static_cast<int>(std::lround(days * margin));
  return {std::max(1, days_left - early), days_left + late};
}

RiskBand MedicalCentre::reinjuryRisk(InjuryType type, std::uint16_t days_left,
                                     int age)
{
  if (type == InjuryType::None) return RiskBand::Low;
  if (type == InjuryType::KneeAcl) return RiskBand::High;
  int level = InjuryModel::isMuscle(type) ? 1 : 0;
  if (InjuryModel::severity(days_left) == InjurySeverity::Major) ++level;
  if (age >= VETERAN_AGE) ++level;
  return static_cast<RiskBand>(std::min(level, 2));
}

InjuryRiskAssessment MedicalCentre::assess(const InjuryRiskInputs& inputs)
{
  InjuryRiskAssessment result;
  float multiplier = 1.0f;
  if (inputs.age > AGE_FROM)
  {
    multiplier *= std::exp(AGE_SLOPE * static_cast<float>(inputs.age - AGE_FROM));
    // Only flagged once it matters (about +16% at 30).
    if (inputs.age >= VETERAN_AGE) result.reasons |= RISK_REASON_AGE;
  }
  if (inputs.days_since_match >= 0 && inputs.days_since_match <= CONGESTION_DAYS)
  {
    using Fitness = WorldTuning::Fitness;
    multiplier *= static_cast<float>(0.5 * (Fitness::CONGESTION_MUSCLE_MULTIPLIER +
                                            Fitness::CONGESTION_OTHER_MULTIPLIER));
    result.reasons |= RISK_REASON_CONGESTION;
  }
  if (inputs.days_since_injury >= 0 &&
      inputs.days_since_injury <= RECURRENCE_WINDOW_DAYS)
  {
    multiplier *= RECENT_INJURY;
    result.reasons |= RISK_REASON_RECENT_INJURY;
  }
  if (inputs.condition < FATIGUE_CONDITION)
  {
    multiplier *= FATIGUE;
    result.reasons |= RISK_REASON_FATIGUE;
  }
  if (inputs.workload_ratio > WORKLOAD_THRESHOLD)
  {
    multiplier *= std::min(
        WORKLOAD_CAP,
        1.0f + WORKLOAD_SLOPE * (inputs.workload_ratio - WORKLOAD_THRESHOLD));
    result.reasons |= RISK_REASON_WORKLOAD;
  }
  multiplier *= inputs.staff_prevention;
  result.multiplier = multiplier;
  result.band = multiplier >= HIGH_FROM      ? RiskBand::High
                : multiplier >= MODERATE_FROM ? RiskBand::Moderate
                                              : RiskBand::Low;
  return result;
}

const char* MedicalCentre::bandKey(RiskBand band)
{
  switch (band)
  {
    case RiskBand::Moderate:
      return "MEDICAL_RISK_MODERATE";
    case RiskBand::High:
      return "MEDICAL_RISK_HIGH";
    case RiskBand::Low:
      break;
  }
  return "MEDICAL_RISK_LOW";
}
