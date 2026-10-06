// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "tools/lab_stats.h"

/**
 * Calibration targets, verdicts and report output (Markdown + JSON) of the
 * balance lab. Report text is an English developer artifact, never shown in
 * the game UI.
 */
namespace Lab
{
enum class Tier : std::uint8_t
{
  Gate,    /*!< Must pass for release; a FAIL makes fm_lab exit 1. */
  Monitor, /*!< >= 90% must pass (NFR-018); below that fm_lab exits 2. */
  Info     /*!< Reported only: no spec band exists. */
};

/** Where a metric can be measured. */
enum class Scope : std::uint8_t
{
  Match,  /*!< Isolated matches (full engine detail) and world matches. */
  Engine, /*!< Isolated matches only (needs engine-only statistics). */
  World,  /*!< Season runs only. */
  Tactics /*!< Tactic round-robin only. */
};

/** How a value is printed: fraction as percent, or a plain number. */
enum class Unit : std::uint8_t
{
  Number,
  Percent
};

/**
 * One calibration target. `metric` names the extractor output (see
 * lab_runner.h); an empty metric marks a target without an extractor yet,
 * explained by `note`. Bands are in the metric's own unit (fractions for
 * percentages).
 */
struct TargetSpec
{
  std::string_view id;
  std::string_view metric;
  std::string_view label;
  std::string_view target;
  std::optional<Band> band;
  Tier tier = Tier::Monitor;
  std::string_view source;
  Unit unit = Unit::Number;
  Scope scope = Scope::Match;
  std::string_view note;
};

/** Every CT-M/CT-W row of the spec plus the lab's INFO metrics. */
std::span<const TargetSpec> targetTable();

/** Evaluated row of a report. */
struct ReportRow
{
  std::string id;
  std::string metric;
  std::string label;
  std::string target;
  std::optional<Band> band;
  Tier tier = Tier::Info;
  std::string source;
  Unit unit = Unit::Number;
  std::string note;
  Estimate estimate;
  Verdict verdict = Verdict::NotMeasured;

  bool operator==(const ReportRow&) const = default;
};

/** Free-form table (distributions, tactics matrix, rating gaps). */
struct TextTable
{
  std::string title;
  std::vector<std::string> header;
  std::vector<std::vector<std::string>> rows;

  bool operator==(const TextTable&) const = default;
};

struct Summary
{
  int gate_pass = 0;
  int gate_warn = 0;
  int gate_fail = 0;
  int monitor_pass = 0;
  int monitor_evaluated = 0;
  int not_measured = 0;
  /** Monitor PASS share among evaluated Monitor rows (1 when none). */
  double monitorPassRate() const;
};

/** Exit codes of fm_lab (contracts/lab-report.md). */
namespace ExitCode
{
inline constexpr int OK = 0;
inline constexpr int GATE_FAILED = 1;
inline constexpr int MONITOR_BELOW_THRESHOLD = 2;
inline constexpr int INVALID_ARGUMENTS = 3;
inline constexpr int SIMULATION_ERROR = 4;
}  // namespace ExitCode

/** Minimum Monitor pass rate (NFR-018). */
inline constexpr double MONITOR_PASS_THRESHOLD = 0.9;

struct LabReport
{
  static constexpr int SCHEMA_VERSION = 1;

  std::string mode;
  std::uint64_t seed = 0;
  /** Sampling units: matches, or seasons for season runs. */
  std::size_t samples = 0;
  unsigned threads = 1;
  double wall_seconds = 0.0;
  double ms_per_match = 0.0;
  double seconds_per_season = 0.0;
  /** Mode parameters (ratings, leagues, pairing...) for reproduction. */
  std::map<std::string, std::string> parameters;
  std::vector<ReportRow> rows;
  std::vector<TextTable> tables;
  /** Findings outside the target table (definitions, caveats). */
  std::vector<std::string> notes;

  Summary summary() const;
  int exitCode() const;
  bool operator==(const LabReport&) const = default;
};

/**
 * Evaluates every target of `scope_filter` against the measured metrics.
 * Targets whose metric is missing are kept as NOT MEASURED rows with a note,
 * so the report always lists the full spec table.
 */
std::vector<ReportRow> evaluateTargets(
    const std::map<std::string, Estimate>& metrics,
    std::span<const Scope> scopes);

std::string_view verdictName(Verdict verdict);
std::string_view tierName(Tier tier);

/** "12.3%" or "2.83" (digits chosen from the magnitude). */
std::string formatValue(double value, Unit unit);

nlohmann::json toJson(const LabReport& report);
/** Inverse of toJson(); throws nlohmann::json::exception on bad input. */
LabReport reportFromJson(const nlohmann::json& json);
std::string toMarkdown(const LabReport& report);
}  // namespace Lab
