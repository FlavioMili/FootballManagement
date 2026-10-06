// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <string>
#include <type_traits>

/**
 * Allocation-free text encoders for the save repositories (CSV and JSON
 * columns written for every row on every save).
 */
namespace TextEncoding
{
/** Appends an integer. */
template <typename T>
  requires std::is_integral_v<T>
inline void appendInt(std::string& out, T value)
{
  std::array<char, 24> buffer{};
  const auto written =
      std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
  out.append(buffer.data(), written.ptr);
}

/** Appends a float in its shortest form that parses back to the same float. */
inline void appendShortest(std::string& out, float value)
{
  std::array<char, 32> buffer{};
  const auto written =
      std::to_chars(buffer.data(), buffer.data() + buffer.size(),
                    std::isfinite(value) ? value : 0.0f);
  out.append(buffer.data(), written.ptr);
}

/**
 * Appends @p value with @p decimals (0-4) fixed decimals, rounded half away
 * from zero (like "{:.Nf}" except for exact binary ties).
 */
inline void appendFixed(std::string& out, float value, int decimals)
{
  constexpr std::array<std::int64_t, 5> SCALES = {1, 10, 100, 1000, 10000};
  const std::int64_t scale = SCALES[static_cast<std::size_t>(decimals)];
  // float * 10^k is exact in double; round() is half away from zero.
  const double scaled =
      std::isfinite(value)
          ? std::round(static_cast<double>(value) * static_cast<double>(scale))
          : 0.0;
  const auto units = static_cast<std::int64_t>(std::fabs(scaled));
  if (scaled < 0.0) out.push_back('-');
  appendInt(out, units / scale);
  if (decimals == 0) return;
  out.push_back('.');
  std::int64_t fraction = units % scale;
  std::array<char, 4> digits{};
  for (int i = decimals - 1; i >= 0; --i)
  {
    digits[static_cast<std::size_t>(i)] =
        static_cast<char>('0' + fraction % 10);
    fraction /= 10;
  }
  out.append(digits.data(), static_cast<std::size_t>(decimals));
}
}  // namespace TextEncoding
