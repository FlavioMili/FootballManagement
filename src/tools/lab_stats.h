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
#include <functional>
#include <limits>
#include <optional>
#include <span>

/**
 * Statistics of the balance lab (fm_lab). Every estimate carries a 95%
 * confidence interval so a realism verdict can tell "off target" apart from
 * "not enough samples".
 */
namespace Lab
{
/** Point estimate with a 95% confidence interval over n sampling units. */
struct Estimate
{
  double value = std::numeric_limits<double>::quiet_NaN();
  double low = std::numeric_limits<double>::quiet_NaN();
  double high = std::numeric_limits<double>::quiet_NaN();
  std::size_t n = 0;

  bool valid() const;
  /** NaN compares equal to NaN, so reports survive a JSON round trip. */
  bool operator==(const Estimate& other) const;
};

/** Closed plausible range of a calibration target. */
struct Band
{
  double low = 0.0;
  double high = 0.0;
  bool operator==(const Band&) const = default;
};

enum class Verdict : std::uint8_t
{
  Pass,        /*!< Point estimate inside the band. */
  Warn,        /*!< Outside, but the 95% interval still overlaps the band. */
  Fail,        /*!< The whole 95% interval lies outside the band. */
  Info,        /*!< Reported without a band. */
  NotMeasured  /*!< No extractor yet, or no data in this mode. */
};

/** Two-sided 97.5% quantile of Student's t (1.96 for large samples). */
double tQuantile975(std::size_t degrees_of_freedom);

/** Mean of per-unit values; t interval mean +- t * sd / sqrt(n). */
Estimate meanEstimate(std::span<const double> values);

/** Share of successes; Wilson score interval (z = 1.96). */
Estimate proportionEstimate(std::size_t successes, std::size_t trials);

/**
 * Ratio of sums sum(num) / sum(den) over sampling units (e.g. goals per
 * shot with matches as units). Delta-method interval:
 * var(R) = sum((num_i - R den_i)^2) / (n (n - 1) mean(den)^2).
 * Units with den = 0 still count (their numerator must then be 0).
 */
Estimate ratioEstimate(std::span<const double> numerators,
                       std::span<const double> denominators);

/** Median with a distribution-free (binomial order-statistic) interval. */
Estimate medianEstimate(std::span<const double> values);

/**
 * Percentile bootstrap for statistics without a closed-form interval (e.g.
 * variance/mean, rate multipliers). Resamples unit indices with a fixed-seed
 * generator, so the interval is deterministic. `statistic` receives the
 * indices of one resample (the identity resample for the point estimate)
 * and returns NaN when undefined; such resamples are skipped.
 */
Estimate bootstrapEstimate(
    std::size_t units,
    const std::function<double(std::span<const std::size_t>)>& statistic,
    std::uint64_t seed, int resamples = 400);

/**
 * PASS when the point estimate lies in the band, WARN when it does not but
 * the interval overlaps the band, FAIL when the whole interval is outside.
 * No band gives Info, an invalid estimate NotMeasured.
 */
Verdict classify(const Estimate& estimate, const std::optional<Band>& band);
}  // namespace Lab
