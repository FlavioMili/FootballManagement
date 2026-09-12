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
#include <limits>
#include <span>
#include <utility>

#include "model/gamedate.h"

/**
 * @enum RngDomain
 * @brief Independent random streams of the world simulation.
 *
 * Every stream is derived from the save's world seed plus a domain tag and
 * stable keys (day, player id, team id). Consuming numbers in one domain never
 * perturbs another, and results do not depend on container iteration order,
 * so a reloaded save continues exactly like an uninterrupted one.
 */
enum class RngDomain : std::uint64_t
{
  Generation = 1,
  Migration,
  TrainingInjury,
  MatchInjury,
  MatchRating,
  Development,
  YouthIntake,
  Retirement,
  Transfers,
  Scouting,
  Staff,
  Training,
};

/**
 * @class WorldRng
 * @brief Deterministic, portable pseudo random generator (xoshiro256**).
 *
 * All distributions are implemented here instead of using <random>
 * distributions, whose algorithms differ between standard libraries. The same
 * seed therefore produces the same world on every platform.
 */
class WorldRng
{
 public:
  using result_type = std::uint64_t;

  /** Seeds the four state words from @p seed through splitmix64. */
  explicit WorldRng(std::uint64_t seed);

  /** Creates the stream for @p domain keyed by up to two stable values. */
  static WorldRng stream(std::uint64_t world_seed, RngDomain domain,
                         std::uint64_t key_a = 0, std::uint64_t key_b = 0);

  /** Single uniform draw in [0, 1) without constructing a stream. */
  static double hashUniform(std::uint64_t world_seed, RngDomain domain,
                            std::uint64_t key_a, std::uint64_t key_b = 0);

  static constexpr result_type min() { return 0; }
  static constexpr result_type max()
  {
    return std::numeric_limits<result_type>::max();
  }
  result_type operator()() { return next(); }

  /** Next raw 64-bit value. */
  std::uint64_t next();

  /** Uniform double in [0, 1). */
  double uniform01();

  /** Uniform float in [lo, hi). */
  float uniform(float lo, float hi);

  /** Uniform integer in [lo, hi] (inclusive, unbiased). */
  int uniformInt(int lo, int hi);

  /** Normally distributed value (Box-Muller). */
  float normal(float mean, float stddev);

  /** Lognormal value with the given median and log-space sigma. */
  float lognormal(float median, float sigma);

  /** Bernoulli trial with probability @p p. */
  bool chance(double p);

  /** Index drawn proportionally to non-negative @p weights. */
  std::size_t weightedIndex(std::span<const float> weights);

  /** Fisher-Yates shuffle with this generator. */
  template <typename T>
  void shuffle(std::span<T> values)
  {
    for (std::size_t i = values.size(); i > 1; --i)
    {
      const auto j =
          static_cast<std::size_t>(uniformInt(0, static_cast<int>(i - 1)));
      std::swap(values[i - 1], values[j]);
    }
  }

 private:
  std::uint64_t state[4];
};

/** Mixes two 64-bit values into a well distributed hash (splitmix64). */
std::uint64_t mixHash(std::uint64_t a, std::uint64_t b);

/** Days since 1970-01-01 for a proleptic Gregorian date. */
std::int32_t dayOrdinal(const GameDateValue& date);

/** Converts a date to the compact integer form YYYYMMDD. */
std::int32_t dateToInt(const GameDateValue& date);

/** Converts the compact integer form YYYYMMDD back to a date. */
GameDateValue dateFromInt(std::int32_t value);
