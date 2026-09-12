// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <vector>

#include "global/types.h"
#include "model/injury.h"
#include "model/staff.h"
#include "model/training.h"

/** @brief Three-step risk band shown by the medical centre. */
enum class RiskBand : std::uint8_t
{
  Low,
  Moderate,
  High
};

/** @brief Days until a player is expected back (inclusive range). */
struct ReturnWindow
{
  int earliest_days = 0;
  int latest_days = 0;
};

/** @brief Observable facts behind a player's injury risk. */
struct InjuryRiskInputs
{
  int age = 25;
  int days_since_injury = -1; /*!< Since the last injury began; -1 = none. */
  int days_since_match = -1;  /*!< -1 = no match played yet. */
  float condition = 100.0f;
  float workload_ratio = 1.0f;     /*!< Acute:chronic load. */
  float staff_prevention = 1.0f;   /*!< StaffEffects::injury_prevention. */
};

/** @brief Why a risk is raised (bit flags). */
enum RiskReason : std::uint8_t
{
  RISK_REASON_NONE = 0,
  RISK_REASON_AGE = 1 << 0,
  RISK_REASON_CONGESTION = 1 << 1,
  RISK_REASON_RECENT_INJURY = 1 << 2,
  RISK_REASON_FATIGUE = 1 << 3,
  RISK_REASON_WORKLOAD = 1 << 4,
};

struct InjuryRiskAssessment
{
  float multiplier = 1.0f; /*!< Relative to an average fresh player. */
  RiskBand band = RiskBand::Low;
  std::uint8_t reasons = RISK_REASON_NONE;
};

/** @brief One injured player of the managed squad. */
struct MedicalInjuryRow
{
  PlayerID player_id = 0;
  InjuryType type = InjuryType::None;
  std::uint16_t days_left = 0;
  InjurySeverity severity = InjurySeverity::Minor;
  ReturnWindow window;
  RiskBand reinjury = RiskBand::Low;
};

/** @brief Fitness and injury risk of one available player. */
struct MedicalRiskRow
{
  PlayerID player_id = 0;
  InjuryRiskAssessment risk;
  float condition = 100.0f;
  float sharpness = 0.0f;
  float workload_ratio = 1.0f;
  WorkloadRisk workload = WorkloadRisk::Low;
  float acute = 0.0f;   /*!< 7-day load per day. */
  float chronic = 0.0f; /*!< 28-day load per day. */
  /** Recovered from an injury within the recurrence window. */
  bool returning = false;
};

/**
 * @struct MedicalReport
 * @brief The managed club's medical centre: who is out, who is at risk and
 * what the medical staff contributes.
 */
struct MedicalReport
{
  std::vector<MedicalInjuryRow> injured; /*!< Longest layoff first. */
  std::vector<MedicalRiskRow> squad;     /*!< Highest risk first. */
  StaffEffects effects;
  std::vector<const StaffMember*> medical_staff; /*!< Physios and doctors. */
  float average_condition = 0.0f; /*!< Of the available players. */
  float average_sharpness = 0.0f;
};

namespace MedicalCentre
{
/** Days after an injury during which a recurrence is more likely. */
inline constexpr int RECURRENCE_WINDOW_DAYS = 60;

/**
 * Medical staff quality in [0, 1] from the layoff multiplier the staff
 * achieves (StaffEffects: 1.12 untrained ... 0.85 elite).
 */
float staffQuality(const StaffEffects& effects);

/**
 * The physio's estimate of the return: the remaining layoff with a margin
 * that is wider for setbacks than for early returns and narrows with a
 * better medical staff (10-30% of the layoff). Always contains the
 * remaining days.
 */
ReturnWindow returnWindow(std::uint16_t days_left, float staff_quality);

/**
 * Risk of a recurrence once back: muscle strains and long layoffs recur
 * more often, and so do injuries of players past 30; ACL tears are high.
 */
RiskBand reinjuryRisk(InjuryType type, std::uint16_t days_left, int age);

/**
 * Injury risk from observable factors only, with the multipliers of the
 * injury model: age over 25, four days or less since a match, a recent
 * injury, low condition and an acute:chronic load above 1.3, scaled by
 * the medical staff. Hidden traits are never used.
 */
InjuryRiskAssessment assess(const InjuryRiskInputs& inputs);

/** Language key naming @p band ("MEDICAL_RISK_LOW", ...). */
const char* bandKey(RiskBand band);
}  // namespace MedicalCentre
