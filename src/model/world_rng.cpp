// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#include "model/world_rng.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <numbers>

namespace
{
std::uint64_t splitmix64(std::uint64_t& x)
{
  std::uint64_t z = (x += 0x9E3779B97F4A7C15ULL);
  z = (z ^ (z >> 30U)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27U)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31U);
}

double toUnit(std::uint64_t value)
{
  return static_cast<double>(value >> 11U) * 0x1.0p-53;
}
}  // namespace

std::uint64_t mixHash(std::uint64_t a, std::uint64_t b)
{
  std::uint64_t x = a ^ std::rotl(b, 23) ^ 0xD1B54A32D192ED03ULL;
  splitmix64(x);
  return splitmix64(x) ^ b;
}

WorldRng::WorldRng(std::uint64_t seed)
{
  for (auto& word : state) word = splitmix64(seed);
}

WorldRng WorldRng::stream(std::uint64_t world_seed, RngDomain domain,
                          std::uint64_t key_a, std::uint64_t key_b)
{
  const std::uint64_t domain_hash =
      mixHash(world_seed, static_cast<std::uint64_t>(domain));
  return WorldRng(mixHash(mixHash(domain_hash, key_a), key_b));
}

double WorldRng::hashUniform(std::uint64_t world_seed, RngDomain domain,
                             std::uint64_t key_a, std::uint64_t key_b)
{
  const std::uint64_t domain_hash =
      mixHash(world_seed, static_cast<std::uint64_t>(domain));
  return toUnit(mixHash(mixHash(domain_hash, key_a), key_b));
}

std::uint64_t WorldRng::next()
{
  const std::uint64_t result = std::rotl(state[1] * 5, 7) * 9;
  const std::uint64_t t = state[1] << 17U;
  state[2] ^= state[0];
  state[3] ^= state[1];
  state[1] ^= state[2];
  state[0] ^= state[3];
  state[2] ^= t;
  state[3] = std::rotl(state[3], 45);
  return result;
}

double WorldRng::uniform01() { return toUnit(next()); }

float WorldRng::uniform(float lo, float hi)
{
  return lo + static_cast<float>(uniform01()) * (hi - lo);
}

int WorldRng::uniformInt(int lo, int hi)
{
  if (hi <= lo) return lo;
  const auto range =
      static_cast<std::uint64_t>(static_cast<std::int64_t>(hi) - lo) + 1U;
  // Rejection sampling removes the modulo bias for exact uniformity.
  const std::uint64_t threshold = (0U - range) % range;
  std::uint64_t value = next();
  while (value < threshold) value = next();
  return static_cast<int>(static_cast<std::int64_t>(lo) +
                          static_cast<std::int64_t>(value % range));
}

float WorldRng::normal(float mean, float stddev)
{
  double u1 = uniform01();
  while (u1 <= 0.0) u1 = uniform01();
  const double u2 = uniform01();
  const double z =
      std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * std::numbers::pi * u2);
  return mean + static_cast<float>(z) * stddev;
}

float WorldRng::lognormal(float median, float sigma)
{
  return median * std::exp(normal(0.0f, sigma));
}

bool WorldRng::chance(double p) { return uniform01() < p; }

std::size_t WorldRng::weightedIndex(std::span<const float> weights)
{
  double total = 0.0;
  for (const float weight : weights)
    total += static_cast<double>(std::max(0.0f, weight));
  if (weights.empty() || total <= 0.0) return 0;
  double pick = uniform01() * total;
  for (std::size_t i = 0; i < weights.size(); ++i)
  {
    pick -= static_cast<double>(std::max(0.0f, weights[i]));
    if (pick < 0.0) return i;
  }
  return weights.size() - 1;
}

std::int32_t dayOrdinal(const GameDateValue& date)
{
  // Howard Hinnant's days_from_civil.
  const int month = date.month;
  const int year = static_cast<int>(date.year) - (month <= 2 ? 1 : 0);
  const int era = (year >= 0 ? year : year - 399) / 400;
  const int year_of_era = year - era * 400;
  const int day_of_year =
      (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + date.day - 1;
  const int day_of_era =
      year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year;
  return era * 146097 + day_of_era - 719468;
}

std::int32_t dateToInt(const GameDateValue& date)
{
  return static_cast<std::int32_t>(date.year) * 10000 + date.month * 100 +
         date.day;
}

GameDateValue dateFromInt(std::int32_t value)
{
  return GameDateValue(static_cast<std::uint16_t>(value / 10000),
                       static_cast<std::uint8_t>((value / 100) % 100),
                       static_cast<std::uint8_t>(value % 100));
}
